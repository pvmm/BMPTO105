#!/bin/env python
"""
******************************************************************************
**
** Copyright (C) 2026 Pedro de Medeiros
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
******************************************************************************
"""

import sys

from pathlib import Path

sys.path.append('..')
import bmpto105


TILE_WIDTH = TILE_HEIGHT = 8
# specially made for 105-colours bitmap
TILE_ROW_WIDTH = 4 # len([fg0, bg0, fg1, bg1])


# default palette
MSX1_PALETTE = [
    (0, 0, 0), (0, 0, 0), (0x24, 0xda, 0x24), (0x68, 0xff, 0x68), (0x24, 0x24, 0xff), (0x48, 0x68, 0xff),
    (0xb6, 0x24, 0x24), (0x48, 0xda, 0xff), (0xff, 0x24, 0x24), (0xff, 0x68, 0x68), (0xda, 0xda, 0x24),
    (0xda, 0xda, 0x91), (0x24, 0x91, 0x24), (0xda, 0x48, 0xb6), (0xb6, 0xb6, 0xb6), (0xff, 0xff, 0xff)
]


def main():
    palette = MSX1_PALETTE

    if len(sys.argv) < 2:
        sys.exit(f'usage: {sys.argv[0]} <image file> <threshold: 0.0 .. 1.0>')

    path = Path(sys.argv[1])
    src = bmpto105.open_bitmap(str(path))

    # Create default palette and color combo table
    engine = bmpto105.Engine(palette)

    # Convert png image to 105 mode
    dst = engine.convert(src)

    # the bigger the threshold, the greater the lossy compression
    threshold = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
    print(f'threshold: {threshold}')

    # Print the stats of tile use
    rep, total = engine.stats(dst, 0, 64, threshold)
    print(f'range: 000-064: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')
    rep, total = engine.stats(dst, 64, 128, threshold)
    print(f'range: 064-128: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')
    rep, total = engine.stats(dst, 128, 192, threshold)
    print(f'range: 128-192: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')

    # Save MSX bitmap and equivalent 105-colours bitmap
    dst.save(str(path.with_suffix('.si2')))
    dst.save_bitmap(str(path.with_suffix('.105.png')))


if __name__ == '__main__':
    main()
