"""
Example PyForge Mod - Invert Filter
====================================

This Python file will be transpiled to C++ by PyForge.
Place in mods/ directory and it will be auto-compiled.
"""

from camforge import Filter, Frame

class InvertFilter(Filter):
    """Inverts all pixel colors"""
    
    def process(self, frame: Frame) -> Frame:
        for y in range(frame.height):
            for x in range(frame.width):
                r, g, b = frame.get_pixel(x, y)
                frame.set_pixel(x, y, 255 - r, 255 - g, 255 - b)
        return frame
