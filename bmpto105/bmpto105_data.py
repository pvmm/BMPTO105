import struct
from typing import Optional, Any, Union, cast, Iterator, Sequence

from dataclasses import dataclass
from PIL import Image

from bmpto105.bmpto105_func import tile_hash
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
class MSXTile_105:
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


class MSXRow_105:
    width: int
    _data: list[MSXTile_105]

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
    def data(self) -> list[MSXTile_105]:
        return self._data

    @data.setter
    def data(self, data: list[int]) -> None:
        if len(data) % TILE_ROW_WIDTH != 0:
            raise ValueError(f'105-colour image data is not a multiple of {TILE_ROW_WIDTH}')
        self._data = [MSXTile_105(*data[i: i + TILE_ROW_WIDTH]) for i in range(0, len(data), TILE_ROW_WIDTH)]

    def __getitem__(self, x: int) -> MSXTile_105:
        return self.data[x]

    def __iter__(self) -> Iterator[MSXTile_105]:
        """Make MSXRow_105 iterable"""
        return iter(self.data)

    def __len__(self) -> int:
        """Return the number of tiles in the row"""
        return len(self.data)


class MSXBitmap_105:
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
        """Make MSXBitmap_105 iterable"""
        return iter(self.data)

    def __len__(self) -> int:
        """Return the number of rows in the bitmap"""
        return len(self.data)

    def stats(self, begin: int = 0, end: Optional[int] = None, threshold: float = 0.1) -> tuple[int, int]:
        if end is None:
            end = self.height
        p: DCT = DCT(threshold)
        stg: dict[str, bool] = {}
        rep: int = 0
        dst1: Image.Image = self.to_image(0b01).crop((0, begin, self.width * TILE_WIDTH, end))
        dst2: Image.Image = self.to_image(0b10).crop((0, begin, self.width * TILE_WIDTH, end))
        for y in range(0, end - begin, TILE_HEIGHT):
            for x in range(0, self.width * TILE_WIDTH, TILE_WIDTH):
                tile: Image.Image = dst1.crop((x, y, x + TILE_WIDTH, y + TILE_HEIGHT))
                bytes_: bytes = bytes(channel for pixel in list(tile.getdata()) for channel in pixel)
                approx: str = tile_hash(p.approximate_tile(bytes_))
                if approx in stg:
                    rep += 1
                else:
                    stg[approx] = True
                tile = dst2.crop((x, y, x + TILE_WIDTH, y + TILE_HEIGHT))
                bytes_ = bytes(channel for pixel in list(tile.getdata()) for channel in pixel)
                approx = tile_hash(p.approximate_tile(bytes_))
                if approx in stg:
                    rep += 1
                else:
                    stg[approx] = True
        # return (number of repetitions, number of used tiles) for the begin..end interval
        return rep, len(stg)

    def save(self, filename: str) -> None:
        """Save MSXBitmap_105 to disk"""
        debug(f'Saving "{filename}"... ', end='')
        with open(filename, 'wb') as file:
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
        debug('Done!')

    def to_metatile(self, x: int, y: int, width: int = 1, height: int = 8, frame: int = 0) -> list[int]:
        """Return the metatile pattern and colors at a position"""
        if y % 8 != 0 or height % 8 != 0:
            raise IndexError('y and height must be multiple of 8')
        metatile: list[int] = []
        for ty in range(y, y + height, TILE_HEIGHT):
            for xx in range(x, x + width):
                for yy in range(ty, ty + TILE_HEIGHT):
                    tile: MSXTile_105 = cast(MSXTile_105, self[yy][xx])
                    p: int = tile.p0 if frame == 0 else tile.p1
                    c: int = tile.c0 if frame == 0 else tile.c1
                    metatile.extend([p, c])
        return metatile

    def to_image(self, frames: int = 0b11) -> Image.Image:
        """convert MSXBitmap_105 to PIL Image"""
        dst: Image.Image = Image.new('RGB', (self.width * TILE_WIDTH, self.height))
        width: int
        height: int
        width, height = dst.size
        for y in range(height):
            for x in range(self.width):
                for tx in range(TILE_WIDTH):
                    tile: MSXTile_105 = cast(MSXTile_105, self[y][x])
                    pixel: tuple[int, int, int] = tile.to_rgb(tx, self.palette, frames)
                    dst.putpixel((x * TILE_WIDTH + tx, y), pixel)
        return dst

    def save_bitmap(self, filename: str) -> None:
        """save MSXBitmap_105 as a PNG image"""
        self.to_image().save(filename)
