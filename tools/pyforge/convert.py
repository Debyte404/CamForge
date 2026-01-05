#!/usr/bin/env python3
"""
convert.py - High-Performance Code Generator for ESP32-S3
==========================================================

Generates optimized C++ code with:
1. Pre-computed Lookup Tables (LUTs) for complex math
2. Constexpr arrays for static data (flash storage)
3. Loop unrolling directives (#pragma GCC unroll)
4. Fixed-point Q8/Q15 coefficient generation
5. SIMD-ready code patterns

Usage:
    python convert.py --filter grayscale --output lut_grayscale.hpp
    python convert.py --all --output-dir src/luts/

HARDWARE TARGETING:
- ESP32-S3 Xtensa LX7 (240MHz Dual Core)
- 400KB SRAM (fast) + 8MB PSRAM (slow)
- No FPU - all math must be integer-only
"""

import argparse
import math
import os
from pathlib import Path
from typing import List, Tuple, Dict

# ============================================================
# CONFIGURATION
# ============================================================

# Fixed-point format for filter coefficients
Q8_SCALE = 256      # Q8: 8 fractional bits (shift right by 8)
Q15_SCALE = 32768   # Q15: 15 fractional bits

# Grayscale luminance coefficients (ITU-R BT.601)
# Y = 0.299*R + 0.587*G + 0.114*B
LUMA_R = 0.299
LUMA_G = 0.587
LUMA_B = 0.114

# Sepia transformation matrix
SEPIA_MATRIX = [
    [0.393, 0.769, 0.189],  # R output
    [0.349, 0.686, 0.168],  # G output
    [0.272, 0.534, 0.131],  # B output
]

# Vintage (warm) color adjustment
VINTAGE_WARMTH = 30      # Orange/yellow shift
VINTAGE_FADE = 20        # Lift shadows
VINTAGE_DESAT = 0.7      # 70% saturation

# Cool color adjustment
COOL_SHIFT = 25          # Blue shift amount
COOL_CONTRAST = 1.08     # 8% contrast boost

# ============================================================
# LUT GENERATION FUNCTIONS
# ============================================================

def generate_grayscale_lut() -> Tuple[List[int], List[int], List[int]]:
    """
    Generate RGB-to-grayscale lookup tables.
    
    OPTIMIZATION: Instead of computing luma = R*0.299 + G*0.587 + B*0.114
    at runtime, we pre-compute partial sums:
    
    luma = GRAY_R_LUT[r] + GRAY_G_LUT[g] + GRAY_B_LUT[b]
    
    Each LUT entry is the weighted contribution of that channel.
    Sum is done with integer addition (fast) instead of multiply (slow).
    
    Returns:
        Tuple of (R_LUT, G_LUT, B_LUT), each with 256 entries
    """
    # Q8 coefficients
    coef_r = int(LUMA_R * Q8_SCALE + 0.5)  # 77
    coef_g = int(LUMA_G * Q8_SCALE + 0.5)  # 150
    coef_b = int(LUMA_B * Q8_SCALE + 0.5)  # 29
    
    lut_r = [(i * coef_r) >> 8 for i in range(256)]
    lut_g = [(i * coef_g) >> 8 for i in range(256)]
    lut_b = [(i * coef_b) >> 8 for i in range(256)]
    
    return lut_r, lut_g, lut_b


def generate_sepia_lut() -> Dict[str, List[int]]:
    """
    Generate sepia transformation lookup tables.
    
    Sepia is a 3x3 matrix multiplication on RGB:
    [tr]   [0.393  0.769  0.189] [r]
    [tg] = [0.349  0.686  0.168] [g]
    [tb]   [0.272  0.534  0.131] [b]
    
    We generate separate LUTs for each (output, input) combination.
    
    Returns:
        Dict with keys: 'RR', 'RG', 'RB', 'GR', 'GG', 'GB', 'BR', 'BG', 'BB'
    """
    luts = {}
    
    # Convert matrix to Q8 and create LUTs
    luts['RR'] = [(i * int(SEPIA_MATRIX[0][0] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['RG'] = [(i * int(SEPIA_MATRIX[0][1] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['RB'] = [(i * int(SEPIA_MATRIX[0][2] * Q8_SCALE)) >> 8 for i in range(256)]
    
    luts['GR'] = [(i * int(SEPIA_MATRIX[1][0] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['GG'] = [(i * int(SEPIA_MATRIX[1][1] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['GB'] = [(i * int(SEPIA_MATRIX[1][2] * Q8_SCALE)) >> 8 for i in range(256)]
    
    luts['BR'] = [(i * int(SEPIA_MATRIX[2][0] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['BG'] = [(i * int(SEPIA_MATRIX[2][1] * Q8_SCALE)) >> 8 for i in range(256)]
    luts['BB'] = [(i * int(SEPIA_MATRIX[2][2] * Q8_SCALE)) >> 8 for i in range(256)]
    
    return luts


def generate_gamma_lut(gamma: float = 2.2) -> List[int]:
    """
    Generate gamma correction lookup table.
    
    FORMULA: output = 255 * (input / 255) ^ (1/gamma)
    
    This replaces expensive pow() calls at runtime.
    
    Args:
        gamma: Gamma value (2.2 is sRGB standard)
    
    Returns:
        256-entry LUT mapping input value to gamma-corrected output
    """
    inv_gamma = 1.0 / gamma
    return [int(255.0 * pow(i / 255.0, inv_gamma) + 0.5) for i in range(256)]


def generate_contrast_lut(factor: float = 1.2) -> List[int]:
    """
    Generate contrast adjustment lookup table.
    
    FORMULA: output = clamp((input - 128) * factor + 128, 0, 255)
    
    Args:
        factor: Contrast multiplier (1.0 = no change)
    
    Returns:
        256-entry LUT
    """
    return [
        max(0, min(255, int((i - 128) * factor + 128 + 0.5)))
        for i in range(256)
    ]


def generate_brightness_lut(offset: int = 20) -> List[int]:
    """
    Generate brightness adjustment lookup table.
    
    FORMULA: output = clamp(input + offset, 0, 255)
    
    Args:
        offset: Brightness offset (-128 to +127)
    
    Returns:
        256-entry LUT
    """
    return [max(0, min(255, i + offset)) for i in range(256)]


def generate_vignette_factor_lut(size: int = 128) -> List[int]:
    """
    Generate vignette darkening factors based on distance.
    
    Instead of computing sqrt(dx^2 + dy^2) at runtime, we use
    squared distance which is monotonic (good enough for vignette).
    
    The LUT maps a normalized distance (0-127) to a darkening factor (0-255).
    
    Args:
        size: LUT size (default 128 for signed distance mapping)
    
    Returns:
        LUT mapping normalized distance to darkening factor
    """
    return [
        int(255 * (1.0 - (i / size) ** 0.5) + 0.5)  # Sqrt falloff
        for i in range(size)
    ]


def generate_rgb565_pack_lut() -> Tuple[List[int], List[int], List[int]]:
    """
    Generate RGB565 packing lookup tables.
    
    Instead of: rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    We use:     rgb565 = R_PACK_LUT[r] | G_PACK_LUT[g] | B_PACK_LUT[b]
    
    This trades 768 bytes of flash for faster packing.
    
    Returns:
        Tuple of (R_pack, G_pack, B_pack) LUTs
    """
    r_pack = [((i & 0xF8) << 8) for i in range(256)]
    g_pack = [((i & 0xFC) << 3) for i in range(256)]
    b_pack = [(i >> 3) for i in range(256)]
    
    return r_pack, g_pack, b_pack


# ============================================================
# CODE GENERATION FUNCTIONS
# ============================================================

def format_lut_array(name: str, values: List[int], type_name: str = "uint8_t",
                     cols: int = 16, progmem: bool = True) -> str:
    """
    Format a LUT as a C++ constexpr array.
    
    OPTIMIZATION: constexpr forces data into flash/ro-data section,
    saving precious SRAM for runtime variables.
    
    Args:
        name: Variable name
        values: List of integer values
        type_name: C++ type (uint8_t, uint16_t, etc.)
        cols: Values per line for formatting
        progmem: If True, add PROGMEM attribute (AVR) or keep constexpr
    
    Returns:
        C++ array definition string
    """
    lines = []
    
    # Header comment
    lines.append(f"// {name}: {len(values)} entries, {type_name}")
    lines.append(f"// HARDWARE: Stored in flash (constexpr) to save SRAM")
    
    # Array declaration
    lines.append(f"static constexpr {type_name} {name}[{len(values)}] = {{")
    
    # Format values in rows
    for i in range(0, len(values), cols):
        row_values = values[i:i+cols]
        if type_name in ("uint8_t", "int8_t"):
            row_str = ", ".join(f"{v:3d}" for v in row_values)
        else:
            row_str = ", ".join(f"0x{v:04X}" for v in row_values)
        
        # Add comma except for last row
        if i + cols < len(values):
            lines.append(f"    {row_str},")
        else:
            lines.append(f"    {row_str}")
    
    lines.append("};")
    lines.append("")
    
    return "\n".join(lines)


def generate_simd_filter_code(filter_name: str) -> str:
    """
    Generate SIMD-optimized filter function with loop unrolling.
    
    HARDWARE EXPLOITATION:
    - #pragma GCC unroll: Instructs compiler to unroll loops
    - FORCEINLINE: Eliminates function call overhead
    - IRAM_ATTR: Places code in instruction cache
    """
    code = []
    
    if filter_name == "grayscale":
        code.append("""
/**
 * @brief SIMD-optimized grayscale filter using LUT
 * HARDWARE: Uses pre-computed LUT (no multiply), unrolled loop
 * 
 * @param pixels Pointer to RGB565 pixel data
 * @param count Number of pixels (should be multiple of 8)
 */
static FORCEINLINE void IRAM_ATTR filter_grayscale_lut(uint16_t* pixels, int count) {
    const int aligned = count & ~7;  // Process 8 at a time
    
    #pragma GCC unroll 8
    for (int i = 0; i < aligned; i++) {
        // Extract RGB from RGB565
        uint8_t r = (pixels[i] >> 8) & 0xF8;
        uint8_t g = (pixels[i] >> 3) & 0xFC;
        uint8_t b = (pixels[i] << 3) & 0xF8;
        
        // LUT-based grayscale (3 memory reads, 2 adds, no multiply!)
        uint8_t gray = GRAY_LUT_R[r] + GRAY_LUT_G[g] + GRAY_LUT_B[b];
        
        // Pack back to RGB565
        pixels[i] = (gray & 0xF8) << 8 | (gray & 0xFC) << 3 | (gray >> 3);
    }
    
    // Handle remainder (same logic, no unroll)
    for (int i = aligned; i < count; i++) {
        uint8_t r = (pixels[i] >> 8) & 0xF8;
        uint8_t g = (pixels[i] >> 3) & 0xFC;
        uint8_t b = (pixels[i] << 3) & 0xF8;
        uint8_t gray = GRAY_LUT_R[r] + GRAY_LUT_G[g] + GRAY_LUT_B[b];
        pixels[i] = (gray & 0xF8) << 8 | (gray & 0xFC) << 3 | (gray >> 3);
    }
}
""")
    
    elif filter_name == "sepia":
        code.append("""
/**
 * @brief SIMD-optimized sepia filter using LUT
 * HARDWARE: 9-way LUT lookup replaces 9 multiplications
 * 
 * @param pixels Pointer to RGB565 pixel data
 * @param count Number of pixels
 */
static FORCEINLINE void IRAM_ATTR filter_sepia_lut(uint16_t* pixels, int count) {
    #pragma GCC unroll 4
    for (int i = 0; i < count; i++) {
        // Extract RGB
        uint8_t r = (pixels[i] >> 8) & 0xF8;
        uint8_t g = (pixels[i] >> 3) & 0xFC;
        uint8_t b = (pixels[i] << 3) & 0xF8;
        
        // Matrix multiply via LUT lookups (9 reads + 6 adds vs 9 muls)
        int tr = SEPIA_LUT_RR[r] + SEPIA_LUT_RG[g] + SEPIA_LUT_RB[b];
        int tg = SEPIA_LUT_GR[r] + SEPIA_LUT_GG[g] + SEPIA_LUT_GB[b];
        int tb = SEPIA_LUT_BR[r] + SEPIA_LUT_BG[g] + SEPIA_LUT_BB[b];
        
        // Saturate to 255
        r = tr > 255 ? 255 : tr;
        g = tg > 255 ? 255 : tg;
        b = tb > 255 ? 255 : tb;
        
        // Pack back
        pixels[i] = (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3);
    }
}
""")
    
    return "\n".join(code)


def generate_header_file(output_path: str, include_all: bool = True):
    """
    Generate complete LUT header file.
    
    Args:
        output_path: Path to output .hpp file
        include_all: If True, include all LUTs; else just grayscale
    """
    content = []
    
    # File header
    content.append("""#pragma once
/**
 * @file lut_tables.hpp
 * @brief Pre-computed Lookup Tables for ESP32-S3 HPC Filters
 * 
 * GENERATED BY: convert.py
 * DO NOT EDIT MANUALLY!
 * 
 * HARDWARE OPTIMIZATION:
 * - All tables are constexpr (stored in flash, not SRAM)
 * - Replaces runtime multiplication with table lookup
 * - Each LUT trades flash space for CPU cycles
 * 
 * MEMORY USAGE:
 * - Grayscale LUTs: 768 bytes (3 x 256)
 * - Sepia LUTs: 2304 bytes (9 x 256)
 * - Total: ~3KB flash
 */

#include <stdint.h>

// Compiler optimization hints
#ifndef IRAM_ATTR
    #define IRAM_ATTR __attribute__((section(".iram1")))
#endif
#define FORCEINLINE __inline__ __attribute__((always_inline))

namespace lut {

""")
    
    # Grayscale LUTs
    lut_r, lut_g, lut_b = generate_grayscale_lut()
    content.append(format_lut_array("GRAY_LUT_R", lut_r, "uint8_t"))
    content.append(format_lut_array("GRAY_LUT_G", lut_g, "uint8_t"))
    content.append(format_lut_array("GRAY_LUT_B", lut_b, "uint8_t"))
    
    if include_all:
        # Sepia LUTs
        sepia_luts = generate_sepia_lut()
        for key, values in sepia_luts.items():
            content.append(format_lut_array(f"SEPIA_LUT_{key}", values, "uint8_t"))
        
        # Gamma LUT
        gamma_lut = generate_gamma_lut(2.2)
        content.append(format_lut_array("GAMMA_LUT_22", gamma_lut, "uint8_t"))
        
        # Inverse gamma (for linear workflow)
        inv_gamma_lut = generate_gamma_lut(1.0/2.2)
        content.append(format_lut_array("GAMMA_LUT_INV", inv_gamma_lut, "uint8_t"))
        
        # Vignette factors
        vignette_lut = generate_vignette_factor_lut(128)
        content.append(format_lut_array("VIGNETTE_LUT", vignette_lut, "uint8_t"))
        
        # RGB565 packing LUTs
        r_pack, g_pack, b_pack = generate_rgb565_pack_lut()
        content.append(format_lut_array("RGB565_R_PACK", r_pack, "uint16_t"))
        content.append(format_lut_array("RGB565_G_PACK", g_pack, "uint16_t"))
        content.append(format_lut_array("RGB565_B_PACK", b_pack, "uint8_t"))
    
    # SIMD filter functions
    content.append(generate_simd_filter_code("grayscale"))
    if include_all:
        content.append(generate_simd_filter_code("sepia"))
    
    # Namespace close
    content.append("""
} // namespace lut
""")
    
    # Write file
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else ".", exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("\n".join(content))
    
    print(f"[convert.py] Generated: {output_path}")
    print(f"[convert.py] LUTs included: grayscale" + (", sepia, gamma, vignette, rgb565" if include_all else ""))


def generate_filter_coefficients() -> str:
    """
    Generate Q8/Q15 fixed-point coefficients for all filters.
    
    Returns:
        C++ code with constexpr coefficient definitions
    """
    code = []
    
    code.append("""
// ============================================================
// FIXED-POINT COEFFICIENTS (Q8 Format)
// ============================================================
// HARDWARE: Integer multiply + right-shift replaces float multiply
// 
// Q8 format: value * 256, then >> 8 after multiply
// Example: 0.299 * 256 = 76.544 ≈ 77
// ============================================================

namespace coef {

""")
    
    # Grayscale coefficients
    code.append(f"// Grayscale luminance (Y = {LUMA_R}*R + {LUMA_G}*G + {LUMA_B}*B)")
    code.append(f"static constexpr uint8_t LUMA_R_Q8 = {int(LUMA_R * Q8_SCALE + 0.5)};  // {LUMA_R:.3f} * 256")
    code.append(f"static constexpr uint8_t LUMA_G_Q8 = {int(LUMA_G * Q8_SCALE + 0.5)};  // {LUMA_G:.3f} * 256")
    code.append(f"static constexpr uint8_t LUMA_B_Q8 = {int(LUMA_B * Q8_SCALE + 0.5)};  // {LUMA_B:.3f} * 256")
    code.append("")
    
    # Sepia matrix coefficients
    code.append("// Sepia transformation matrix (Q8)")
    for i, row in enumerate(SEPIA_MATRIX):
        row_name = ['R', 'G', 'B'][i]
        for j, val in enumerate(row):
            col_name = ['R', 'G', 'B'][j]
            q8_val = int(val * Q8_SCALE + 0.5)
            code.append(f"static constexpr uint8_t SEPIA_{row_name}{col_name}_Q8 = {q8_val};  // {val:.3f} * 256")
    code.append("")
    
    # Contrast/brightness constants
    code.append(f"// Vintage filter constants")
    code.append(f"static constexpr uint8_t VINTAGE_WARMTH = {VINTAGE_WARMTH};")
    code.append(f"static constexpr uint8_t VINTAGE_FADE = {VINTAGE_FADE};")
    code.append(f"static constexpr uint8_t VINTAGE_DESAT_Q8 = {int(VINTAGE_DESAT * Q8_SCALE)};  // {VINTAGE_DESAT:.1f}")
    code.append("")
    
    code.append(f"// Cool filter constants")
    code.append(f"static constexpr uint8_t COOL_SHIFT = {COOL_SHIFT};")
    code.append(f"static constexpr uint16_t COOL_CONTRAST_Q8 = {int(COOL_CONTRAST * Q8_SCALE)};  // {COOL_CONTRAST:.2f}")
    code.append("")
    
    code.append("} // namespace coef")
    
    return "\n".join(code)


# ============================================================
# MAIN ENTRY POINT
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate optimized C++ code for ESP32-S3 filters",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python convert.py --all --output src/luts/lut_tables.hpp
  python convert.py --filter grayscale --output lut_gray.hpp
  python convert.py --coefficients --output coefficients.hpp
        """
    )
    
    parser.add_argument("--all", action="store_true",
                        help="Generate all LUTs and filter code")
    parser.add_argument("--filter", choices=["grayscale", "sepia", "gamma", "vignette"],
                        help="Generate specific filter LUT")
    parser.add_argument("--coefficients", action="store_true",
                        help="Generate fixed-point coefficients only")
    parser.add_argument("--output", "-o", default="lut_tables.hpp",
                        help="Output file path")
    
    args = parser.parse_args()
    
    if args.coefficients:
        # Generate just coefficients
        content = generate_filter_coefficients()
        with open(args.output, 'w') as f:
            f.write("#pragma once\n")
            f.write("// Generated by convert.py\n\n")
            f.write('#include <stdint.h>\n\n')
            f.write(content)
        print(f"[convert.py] Generated coefficients: {args.output}")
    
    elif args.all or not args.filter:
        # Generate complete header with all LUTs
        generate_header_file(args.output, include_all=True)
    
    elif args.filter:
        # Generate specific filter
        generate_header_file(args.output, include_all=(args.filter != "grayscale"))
    
    print("[convert.py] Done!")


if __name__ == "__main__":
    main()
