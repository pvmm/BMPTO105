#!/bin/env python

import sys
import struct

from pathlib import Path
from typing import Any, Final, cast
from PIL import Image
from dataclasses import dataclass


TILE_WIDTH = TILE_HEIGHT = 8
debug = print

# default palette
MSX1_PALETTE = [
    (0, 0, 0), (0, 0, 0), (0x24, 0xda, 0x24), (0x68, 0xff, 0x68), (0x24, 0x24, 0xff), (0x48, 0x68, 0xff),
    (0xb6, 0x24, 0x24), (0x48, 0xda, 0xff), (0xff, 0x24, 0x24), (0xff, 0x68, 0x68), (0xda, 0xda, 0x24),
    (0xda, 0xda, 0x91), (0x24, 0x91, 0x24), (0xda, 0x48, 0xb6), (0xb6, 0xb6, 0xb6), (0xff, 0xff, 0xff)
]


def color_mse(rgb1: tuple[int, int, int], rgb2: tuple[int, int, int]) -> int:
    return (rgb1[0] - rgb2[0]) ** 2 + (rgb1[1] - rgb2[1]) ** 2 + (rgb1[2] - rgb2[2]) ** 2


class RGBColor:
    r: int
    g: int
    b: int

    def __init__(self, rgb: tuple[int, int, int]):
        self.r = rgb[0]
        self.g = rgb[1]
        self.b = rgb[2]


class ColorCombo:
    c0: int
    c1: int
    r: int
    g: int
    b: int

    @property
    def rgb(self):
        return self.r, self.g, self.b

    def __init__(self, c0: int, c1: int, r: int, g: int, b: int):
        self.c0 = c0
        self.c1 = c1
        self.r = r
        self.g = g
        self.b = b


@dataclass
class MSXTile105:
    c0: int
    c1: int
    p0: int
    p1: int

    def rgb(self, x, palette: list[RGBColor]) -> tuple[int, int, int]:
        """Return RGB pixel value equivalent to MSX 105-colour bitmap."""
        bit = 1 << ((TILE_WIDTH - 1) - (x % TILE_WIDTH))
        p0: bool = True if self.p0 & bit else False
        p1: bool = True if self.p1 & bit else False
        fg0, bg0 = (self.c0 // 16) & 0xf, self.c0 & 0xf
        fg1, bg1 = (self.c1 // 16) & 0xf, self.c1 & 0xf
        return (((palette[fg0].r if p0 else palette[bg0].r) +
                 (palette[fg1].r if p1 else palette[bg1].r)) // 2,
                ((palette[fg0].g if p0 else palette[bg0].g) +
                 (palette[fg1].g if p1 else palette[bg1].g)) // 2,
                ((palette[fg0].b if p0 else palette[bg0].b) +
                 (palette[fg1].b if p1 else palette[bg1].b)) // 2)


class MSXRow105:
    width: int
    data: list[MSXTile105]

    def __init__(self, width):
        self.width = width
        self.data = [MSXTile105(0, 0, 0, 0) for _ in range(self.width)]

    def __getitem__(self, x):
        return self.data[x]


class MSXBitmap105:
    width: int
    height: int
    data: list[MSXRow105]

    def __init__(self, width: int, height: int):
        self.width = width // TILE_WIDTH
        self.height = height // TILE_HEIGHT # tile based width and height
        self.data = [MSXRow105(self.width) for _ in range(height)]

    def __getitem__(self, y):
        return self.data[y]


def create_palette(palette: list[tuple[int, int, int]]) -> list[RGBColor]:
    result: list[RGBColor] = []
    for value in palette:
        result.append(RGBColor(value))
    return result


def create_combo(idx1: int, idx2: int, palette: list[RGBColor]) -> ColorCombo:
    return ColorCombo(
            idx1, idx2,
            (palette[idx1].r + palette[idx2].r) // 2,
            (palette[idx1].g + palette[idx2].g) // 2,
            (palette[idx1].b + palette[idx2].b) // 2
    )


def create_105_color_table_combo(palette: list[RGBColor]) -> list[list[ColorCombo]]:
    result = []
    for bg0 in range(1, 16):
        for fg0 in range(bg0 + 1, 16):
            for bg1 in range(bg0, 16):
                for fg1 in range(bg1 + 1, 16):
                    cc_list = [
                            create_combo(bg0, bg1, palette),
                            create_combo(fg0, bg1, palette),
                            create_combo(bg0, fg1, palette),
                            create_combo(fg0, fg1, palette)
                    ]
                    result.append(cc_list)
    return result


# find the 4-colour combination in color_combo_table that best matches all colors in a 8x1 "tile"
def find_best_match(tile: tuple[tuple[int, ...], ...], color_combo_table: list[list[ColorCombo]]) -> int:
    """
    real	6m22.505s
    user	6m21.402s
    sys	0m0.129s
    """
    best_idx = 0
    best_mse = sys.maxsize
    for i, color in enumerate(color_combo_table):
        mse = 0
        rgb_list = [c.rgb for c in color]
        for dx in range(8):
            t_list = [color_mse(rgb, tile[dx]) for rgb in rgb_list]
            mse += min(t_list)
        if mse < best_mse:
            best_mse = mse
            best_idx = i
    return best_idx


def rindex(items: list[Any], target: Any) -> Any:
    """return last occurence of element in collection"""
    return max(i for i, val in enumerate(items) if val == target)


def convert(src: Image.Image, dst: MSXBitmap105, color_combo_table: list[list[ColorCombo]]) -> None:
    data = src.get_flattened_data()
    width, height = src.size

    for y in range(0, height):
        print(f'\rConverting line {y + 1}/{height}... ', end='')

        # process tile width each iteration
        for x in range(0, width // 8):
            pos = x * 8 + y * width
            tile: tuple[tuple[int, int, int], ...] = cast(tuple[tuple[int, int, int], ...], tuple([x for x in data[pos : pos + 8]]))

            # find best colour combinations of (bg0, fg0, bg1, fg1) for each pixel
            best_match = find_best_match(tile, color_combo_table)
            rgb_list = [c.rgb for c in color_combo_table[best_match]]

            pattern0 = 0
            pattern1 = 0

            # process pixel in each tile width
            for tx in range(TILE_WIDTH):
                # shift left to prepare to set next bit
                pattern0 <<= 1
                pattern1 <<= 1

                # get distance of each colour from a colour combination to the actual tile colours
                # t ↴
                #   0: bg0 | bg1
                #   1: fg0 | bg1
                #   2: bg0 | fg1
                #   3: fg0 | fg1
                t = list([color_mse(rgb, tile[tx]) for rgb in rgb_list])

                # select key whose value is the smaller distance among (bg0 | bg1), (fg0 | bg1), (bg0 | fg1), (fg0 | fg1) 
                i = rindex(t, min(t))

                # set pattern in even image (fg0) because: 1 -> (fg0 | bg1) or 3 -> (fg0 | fg1)
                pattern0 |= (i & 1)
                # set pattern in odd image (fg1) because: 2 -> (bg0 | fg1) or 3 -> (fg0 | fg1)
                pattern1 |= (i & 2) >> 1

            # even frame combination: fg0 | bg0
            dst[y][x].c0 = color_combo_table[best_match][0].c0 + 16 * color_combo_table[best_match][1].c0
            # even frame pattern
            dst[y][x].p0 = pattern0
            # odd frame combination: fg1 | bg1
            dst[y][x].c1 = color_combo_table[best_match][0].c1 + 16 * color_combo_table[best_match][2].c1
            # odd frame pattern
            dst[y][x].p1 = pattern1

    print('\nDone!')


def save_msx_bitmap(filename: str, image: MSXBitmap105) -> None:
    print(f'Saving "{filename}"... ', end='')
    with open(filename, 'wb') as file:
        # dimensions header
        file.write(struct.pack('BB', image.width, image.height))

        # Save patterns for even image
        for y in range(0, image.height * TILE_HEIGHT, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p0 for pixel in [row[x] for row in image[s]]]))

        # Save colours for even image
        for y in range(0, image.height * TILE_HEIGHT, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c0 for pixel in [row[x] for row in image[s]]]))

        # Save patterns for odd image
        for y in range(0, image.height * TILE_HEIGHT, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p1 for pixel in [row[x] for row in image[s]]]))

        # Save colours for even image
        for y in range(0, image.height * TILE_HEIGHT, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c1 for pixel in [row[x] for row in image[s]]]))

    print('done!')


def save_bitmap(filename: str, src: MSXBitmap105, palette: list[RGBColor]) -> None:
    dst = Image.new('RGB', (src.width * TILE_WIDTH, src.height * TILE_HEIGHT))
    width, height = dst.size
    #data = dst.get_flattened_data()

    for y in range(height):
        for x in range(src.width):
            for tx in range(TILE_WIDTH):
                pixel = src[y][x].rgb(tx, palette)
                dst.putpixel((x * TILE_WIDTH + tx, y), pixel)
    dst.save(filename)


def main():
    if len(sys.argv) < 2:
        sys.exit(f'usage: {sys.argv[0]} <image file>')

    # Create default palette and color combo table
    palette = create_palette(MSX1_PALETTE)
    color_combo_table = create_105_color_table_combo(palette)
    debug(f'Created table of {len(color_combo_table)} color combinations')

    path = Path(sys.argv[1])
    src = Image.open(str(path))
    width, height = src.size
    if width % 8 != 0:
        sys.exit('expected width multiple of 8')
    if width // 8 > 255:
        sys.exit('width too big')
    if height % 8 != 0:
        sys.exit('expected height multiple of 8')
    if height // 8 > 255:
        sys.exit('height too big')

    dst = MSXBitmap105(width, height)
    convert(src, dst, color_combo_table)

    # Save MSX bitmap and equivalent 105-colours bitmap
    save_msx_bitmap(str(path.with_suffix('.si2')), dst)
    save_bitmap(str(path.with_suffix('.105.png')), dst, palette)


if __name__ == '__main__':
    main()
