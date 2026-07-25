import struct
from typing import Optional, Any, Union, cast, Iterator, Sequence, BinaryIO

from dataclasses import dataclass
from PIL import Image

import bmpto105

from bmpto105.functions import tile_hash, create_bitmap
from bmpto105.dct import DCT


# constants
TILE_WIDTH: int = 8
TILE_HEIGHT: int = 8
# specially made for 105-colours bitmap
TILE_ROW_WIDTH: int = 4  # len([fg0, bg0, fg1, bg1])

debug = print

#
# Python-side classes
#

@dataclass
class RGBColor:
    r: int
    g: int
    b: int


@dataclass
class MSXUnit_105:
    '''1x8 block unit from a tile'''
    c0: int
    p0: int
    c1: int
    p1: int

    def to_rgb(self, x: int, palette: list[RGBColor], frames: int = 0b11) -> tuple[int, int, int]:
        """Return RGB pixel value equivalent to MSX 105-colour bitmap."""
        bit: int = 1 << ((TILE_WIDTH - 1) - (x % TILE_WIDTH))
        p0: bool
        p1: bool
        f0: int
        f1: int
        b0: int
        b1: int

        if frames & 0b01 == 0b01:
            p0 = True if self.p0 & bit else False
            f0, b0 = (self.c0 >> 4) & 0xf, self.c0 & 0xf
            if frames == 1:
                p1, f1, b1 = p0, f0, b0
        if frames & 0b10 == 0b10:
            p1 = True if self.p1 & bit else False
            f1, b1 = (self.c1 >> 4) & 0xf, self.c1 & 0xf
            if frames == 2:
                p0, f0, b0 = p1, f1, b1
        return (
            ((palette[f0].r if p0 else palette[b0].r) +
             (palette[f1].r if p1 else palette[b1].r)) // 2,
            ((palette[f0].g if p0 else palette[b0].g) +
             (palette[f1].g if p1 else palette[b1].g)) // 2,
            ((palette[f0].b if p0 else palette[b0].b) +
             (palette[f1].b if p1 else palette[b1].b)) // 2
        )


    def from_rgb(self, x: int, b: bool, fg: int | None = None, bg: int | None = None, frames: int = 0b11) -> None:
        bit: int = 1 << ((TILE_WIDTH - 1) - (x % TILE_WIDTH))
        if frames & 0b01 == 0b01:
            if b:
                self.p0 |= bit
            else:
                self.p0 &= ~bit
            if not fg is None:
                self.c0 = (self.c0 & 0x0f) | (fg << 4)
            if not bg is None:
                self.c0 = (self.c0 & 0xf0) | bg
        if frames & 0b10 == 0b10:
            if b:
                self.p1 |= bit
            else:
                self.p1 &= ~bit
            if not fg is None:
                self.c1 = (self.c1 & 0x0f) | (fg << 4)
            if not bg is None:
                self.c1 = (self.c1 & 0xf0) | bg


class MSXRow_105:
    '''screen line from 0 to 255'''
    width: int
    _data: list[MSXUnit_105]

    def __init__(self, width: int, data: Optional[list[int]] = None) -> None:
        """width: number of tiles horizontally"""
        self.width = width
        if data is None:
            data = [0] * TILE_ROW_WIDTH * self.width
        else:
            if len(data) / TILE_ROW_WIDTH != self.width:
                raise ValueError('105-colour image row size and specified width don\'t match')
            self.data = data

    @property
    def data(self) -> list[MSXUnit_105]:
        return self._data

    @data.setter
    def data(self, data: list[int]) -> None:
        if len(data) % TILE_ROW_WIDTH != 0:
            raise ValueError(f'105-colour image data is not a multiple of {TILE_ROW_WIDTH}')
        self._data = [MSXUnit_105(*data[i: i + TILE_ROW_WIDTH]) for i in range(0, len(data), TILE_ROW_WIDTH)]

    def __getitem__(self, x: int) -> MSXUnit_105:
        return self.data[x]

    def __iter__(self) -> Iterator[MSXUnit_105]:
        """Make MSXRow_105 iterable"""
        return iter(self.data)

    def __len__(self) -> int:
        """Return the number of tiles in the row"""
        return len(self.data)


class MSXBitmap:
    width: int
    height: int
    _palette: list[RGBColor]
    _data: list[MSXRow_105]

    def __init__(self, width: int, height: int, palette: list[RGBColor], data: Optional[list[int]] = None) -> None:
        """height: number of rows vertically, width: number of tiles (not pixels) horizontally"""
        self.width = width
        self.height = height
        self.palette = palette
        if data is None:
            self.data = [0] * width * TILE_ROW_WIDTH * height
        else:
            self.data = data

    @property
    def palette(self) -> list[RGBColor]:
        return self._palette

    @palette.setter
    def palette(self, palette: list[RGBColor]) -> None:
        if len(palette) != 16:
            raise ValueError(f'16 colours list of RGBColor structure expected')
        self._palette = palette

    @property
    def data(self) -> list[MSXRow_105]:
        return self._data

    @data.setter
    def data(self, data: list[int]) -> None:
        length: int = self.width * self.height * TILE_ROW_WIDTH
        if len(data) != length:
            raise ValueError(f'105-colour image data size and dimensions don\'t match, expected {length}, got {len(data)}')
        # stride is the size of a single line from the image
        stride: int = self.width * TILE_ROW_WIDTH
        self._data = [MSXRow_105(self.width, data[i: i + stride]) for i in range(0, len(data), stride)]

    def __getitem__(self, key: Union[int, slice]) -> Union[MSXRow_105, list[MSXRow_105]]:
        """Support both integer and slice indexing"""
        if isinstance(key, slice):
            return self.data[key]
        return self.data[key]

    def __iter__(self) -> Iterator[MSXRow_105]:
        """Make MSX Bitmap iterable"""
        return iter(self.data)

    def __len__(self) -> int:
        """Return the number of rows in the bitmap"""
        return len(self.data)

    def to_tile(self, y: int, x: int, frame: int) -> list[tuple[int, int, int]]:
        return [row[x].to_rgb(n, self.palette, frames=frame) for n in range(TILE_WIDTH) for row in cast(list[MSXRow_105], self[y : y + 8])]

    def save_msx(self, filename: str) -> None:
        debug(f'Saving "{filename}"... ', end='')
        with open(filename, 'wb') as file:
            self.save(file)
        debug('Done!')

    def save(self, file: BinaryIO) -> BinaryIO:
        # dimensions header
        file.write(struct.pack('BB', self.width, self.height // 8))
        rows: list[MSXRow_105]

        # Save patterns for even image
        for y in range(0, self.height, TILE_HEIGHT):
            rows = cast(list[MSXRow_105], self[y : y + TILE_HEIGHT])
            for x in range(0, self.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p0 for pixel in [row[x] for row in rows]]))

        # Save colours for even image
        for y in range(0, self.height, TILE_HEIGHT):
            rows = cast(list[MSXRow_105], self[y : y + TILE_HEIGHT])
            for x in range(0, self.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c0 for pixel in [row[x] for row in rows]]))

        # Save patterns for odd image
        for y in range(0, self.height, TILE_HEIGHT):
            rows = cast(list[MSXRow_105], self[y : y + TILE_HEIGHT])
            for x in range(0, self.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p1 for pixel in [row[x] for row in rows]]))

        # Save colours for even image
        for y in range(0, self.height, TILE_HEIGHT):
            rows = cast(list[MSXRow_105], self[y : y + TILE_HEIGHT])
            for x in range(0, self.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c1 for pixel in [row[x] for row in rows]]))

        return file

    def to_metatile(self, x0: int, y0: int, width: int = 1, height: int = 8, frame: int = 1) -> list[int]:
        """Return the metatile pattern and colors at a position withou combining frames (just frame 1 or 2)"""
        if y0 % 8 != 0 or height % 8 != 0:
            raise IndexError('y and height must be multiple of 8')
        metatile: list[int] = []
        for y in range(y0, y0 + height):
            for x in range(x0, x0 + width):
                tile: MSXUnit_105 = cast(MSXUnit_105, self[y][x])
                p: int = [tile.p0, tile.p1][frame - 1]
                c: int = [tile.c0, tile.c1][frame - 1]
                metatile.extend([p, c])
        return metatile

    def to_image(self, frames: int = 0b11) -> Image.Image:
        """convert MSX Bitmap to PIL Image"""
        dst: Image.Image = Image.new('RGB', (self.width * TILE_WIDTH, self.height))
        width: int
        height: int
        width, height = dst.size
        for y in range(height):
            for x in range(self.width):
                for tx in range(TILE_WIDTH):
                    tile: MSXUnit_105 = cast(MSXUnit_105, self[y][x])
                    pixel: tuple[int, int, int] = tile.to_rgb(tx, self.palette, frames)
                    dst.putpixel((x * TILE_WIDTH + tx, y), pixel)
        return dst

    def save_image(self, filename: str) -> None:
        """save Bitmap to a file"""
        self.to_image().save(filename)


class Engine:
    '''encapsulates BmpTo105 C++ class'''

    def __init__(self, palette: list[tuple[int, int, int]]):
        self.palette = palette
        self.bmpTo105 = bmpto105.BmpTo105(palette)

    def convert(self, image: Image.Image) -> MSXBitmap:
        return self.bmpTo105.convert(image)

    def stats(self, bitmap: MSXBitmap, begin: int, end: int | None = None, threshold: float = 0.0) -> tuple[int, int]:
        if end is None:
            end = bitmap.height
        #tiles: dict[str, list[tuple[int, int, int]]] = {}
        p: DCT = DCT(threshold)
        # pattern generator table (pgt[hash: str | pattern_no: int] -> (patterno_no, pattern_data: str))
        pgt: dict[str | int, tuple[int, list[int]]] = {}
        # pattern name table (pnt[frame: int][index: int] -> pattern_no: int)
        pnt: tuple[list[int], list[int]] = ([], [])
        rep: int = 0
        # process each frame individually
        frame0: Image.Image = bitmap.to_image(0b01).crop((0, begin, bitmap.width * TILE_WIDTH, end))
        frame1: Image.Image = bitmap.to_image(0b10).crop((0, begin, bitmap.width * TILE_WIDTH, end))
        pos: int = 0
        for y in range(0, end - begin, TILE_HEIGHT):
            for x in range(0, bitmap.width * TILE_WIDTH, TILE_WIDTH):
                # even frame tile
                tile = frame0.crop((x, y, x + TILE_WIDTH, y + TILE_HEIGHT))
                bytes_ = bytes(channels for pixel in list(tile.getdata()) for channels in pixel)
                approx = p.approximate_tile(bytes_)
                hash_ = tile_hash(approx)
                if not hash_ in pgt:
                    # convert [r0,g0,b0,r1,g1,b1,...] back into [(r0,g0,b0),(r1,g1,b1),...]
                    unflattened = [(approx[i], approx[i + 1], approx[i + 2]) for i in range(0, len(approx), 3)]
                    # convert bitmap into MSX tile
                    t = [(row[0].c0, row[0].p0) for row in self.bmpTo105.convert(
                        create_bitmap(TILE_WIDTH, TILE_HEIGHT, unflattened))]
                    # store tile as the hash to VRAM position
                    pgt[hash_] = pgt[pos] = (pos, t)
                    # add tile reference to pattern name table
                    pnt[0].append(pos)
                    pos += 1
                else:
                    pnt[0].append(pgt[hash_][0])
                    rep += 1
                # odd frame tile
                tile = frame1.crop((x, y, x + TILE_WIDTH, y + TILE_HEIGHT))
                bytes_ = bytes(channel for pixel in list(tile.getdata()) for channel in pixel)
                approx = p.approximate_tile(bytes_)
                hash_ = tile_hash(approx)
                if not hash_ in pgt:
                    # convert [r0,g0,b0,r1,g1,b1,...] back into [(r0,g0,b0),(r1,g1,b1),...]
                    unflattened = [(approx[i], approx[i + 1], approx[i + 2]) for i in range(0, len(approx), 3)]
                    # convert bitmap into MSX tile
                    t = [(row[0].c0, row[0].p0) for row in self.bmpTo105.convert(
                        create_bitmap(TILE_WIDTH, TILE_HEIGHT, unflattened))]
                    # store tile as the hash to VRAM position
                    pgt[hash_] = pgt[pos] = (pos, t)
                    # add tile reference to pattern name table
                    pnt[1].append(pos)
                    pos += 1
                else:
                    pnt[1].append(pgt[hash_][0])
                    rep += 1

        # return (number of repetitions, total number of used tiles, pattern generator table and pattern name table)
        return rep, len(pgt) // 2, pgt, pnt
