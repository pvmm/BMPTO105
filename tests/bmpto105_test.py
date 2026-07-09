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

sys.path.append('..')
import bmpto105

from bmpto105 import BmpTo105

from pathlib import Path

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
        sys.exit(f'usage: {sys.argv[0]} <image file>')

    path = Path(sys.argv[1])
    src = bmpto105.open_bitmap(str(path))

    # Create default palette and color combo table
    engine = BmpTo105(palette)
    dst = engine.convert(src)

    rep, total = dst.stats(0, 64)
    print(f'range: {0:03d}-{64:03d}: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')
    rep, total = dst.stats(64, 128)
    print(f'range: {64:03d}-{128:03d}: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')
    rep, total = dst.stats(128, 192)
    print(f'range: {128:03d}-{192:03d}: size: {total}{'*' if total > 256 else ''}, repetition: {rep}')

    # Save MSX bitmap and equivalent 105-colours bitmap
    bmpto105.save_msx_bitmap(str(path.with_suffix('.si2')), dst)
    bmpto105.save_bitmap(str(path.with_suffix('.105.png')), dst)


if __name__ == '__main__':
    main()
