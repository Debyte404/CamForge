#!/usr/bin/env python3
"""
PyForge PlatformIO Pre-Build Script
====================================

Automatically transpiles Python mods to C++ before compilation.
Place in platformio.ini: extra_scripts = pre:tools/pyforge/prebuild.py
"""

import os
import sys
from pathlib import Path

# Add pyforge to path
script_dir = Path(__file__).parent
sys.path.insert(0, str(script_dir))

from pyforge import compile_file

Import("env")

def before_build(source, target, env):
    """Called before build starts"""
    project_dir = Path(env['PROJECT_DIR'])
    mods_dir = project_dir / "mods"
    output_dir = project_dir / "src" / "mods"
    
    if not mods_dir.exists():
        print("[PyForge] No mods directory found, skipping...")
        return
    
    # Find all Python files in mods/
    py_files = list(mods_dir.glob("*.py"))
    
    if not py_files:
        print("[PyForge] No Python mods found, skipping...")
        return
    
    print(f"[PyForge] Found {len(py_files)} Python mod(s)")
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Compile each Python file
    for py_file in py_files:
        cpp_file = output_dir / (py_file.stem + ".cpp")
        
        # Check if recompilation needed
        if cpp_file.exists():
            if cpp_file.stat().st_mtime > py_file.stat().st_mtime:
                print(f"[PyForge] Skipping {py_file.name} (up to date)")
                continue
        
        compile_file(str(py_file), str(cpp_file))

# Register pre-build action
env.AddPreAction("buildprog", before_build)

print("[PyForge] Pre-build hook registered")
