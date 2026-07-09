import struct

from dataclasses import dataclass
from PIL import Image

# constants
TILE_WIDTH = TILE_HEIGHT = 8
# specially made for 105-colours bitmap
TILE_ROW_WIDTH = 4 # len([fg0, bg0, fg1, bg1])

debug = print

#
# Python-side classes
#

@dataclass
class MSXTile_105:
    c0: int
    p0: int
    c1: int
    p1: int


    def to_rgb(self, x, palette: list[RGBColor]) -> tuple[int, int, int]:
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


class MSXRow_105:
    width: int
    _data: list[MSXTile_105]


    def __init__(self, width: int, data: list[int] | None = None):
        """width: number of tiles horizontally"""
        self.width = width
        if data is None:
            data = [0] * TILE_ROW_WIDTH * self.width
        else:
            if len(data) / TILE_ROW_WIDTH != self.width:
                raise ValueError('105-colour image row size and specified width don\'t match')
            self.data = data


    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, data):
        if len(data) % TILE_ROW_WIDTH != 0:
            raise ValueError(f'105-colour image data is not a multiple of {TILE_ROW_WIDTH}')
        self._data = [MSXTile_105(*data[i : i + TILE_ROW_WIDTH]) for i in range(0, len(data), TILE_ROW_WIDTH)]


    def __getitem__(self, x):
        return self.data[x]


class MSXBitmap_105:
    width: int
    height: int
    _palette: list[RGBColor]
    _data: list[MSXRow_105]


    def __init__(self, width: int, height: int, palette: list[RGBColor], data: list[int] | None = None):
        """height: number of rows vertically, width: number of tiles (not pixels) horizontally"""
        self.width = width
        self.height = height
        self.palette = palette
        if data is None:
            self.data = [0] * width * TILE_ROW_WIDTH * height
        else:
            self.data = data


    @property
    def palette(self):
        return self._palette

    @palette.setter
    def palette(self, palette):
        if len(palette) != 16:
            raise ValueError(f'16 colours list of RGBColor structure expected')
        self._palette = palette


    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, data):
        length = self.width * self.height * TILE_ROW_WIDTH
        if len(data) != length:
            raise ValueError(f'105-colour image data size and specified dimensions don\'t match, expected {length}, got {len(data)}')
        # stride is the size of a single line from the image
        stride = self.width * TILE_ROW_WIDTH
        self._data = [MSXRow_105(self.width, data[i : i + stride]) for i in range(0, len(data), stride)]


    def __getitem__(self, y):
        return self.data[y]


    def stats(self, begin: int = 0, end: int | None = None) -> tuple[int, int]:
        """Count how many tiles repeat and the total amount"""
        stg = {}
        rep = 0
        image = self
        if end is None: end = self.height
        for y in range(begin, end, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT) # get the tile content from height to height + 8
            for x in range(0, image.width):
                pat = ''.join([f'{pixel.p0:02x}' for pixel in [row[x] for row in image[s]]])
                col = ''.join([f'{pixel.c0:02x}' for pixel in [row[x] for row in image[s]]])
                key = f'{pat}:{col}'
                if key in stg:
                    rep += 1
                else:
                    stg[key] = True

                pat = ''.join([f'{pixel.p1:02x}' for pixel in [row[x] for row in image[s]]])
                col = ''.join([f'{pixel.c1:02x}' for pixel in [row[x] for row in image[s]]])
                key = f'{pat}:{col}'
                if key in stg:
                    rep += 1
                else:
                    stg[key] = True
        # return (number of repetitions, number of used tiles) for the begin..end interval
        return rep, len(stg)


    def save(self, filename: str) -> None:
        """Save MSXBitmap_105 to disk"""
        debug(f'Saving "{filename}"... ', end='')
        with open(filename, 'wb') as file:
            # dimensions header
            file.write(struct.pack('BB', self.width, self.height // 8))

            # Save patterns for even image
            for y in range(0, self.height, TILE_HEIGHT):
                s = slice(y, y + TILE_HEIGHT) # get the tile content from height to height + 8
                for x in range(0, self.width):
                    file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p0 for pixel in [row[x] for row in self[s]]]))

            # Save colours for even image
            for y in range(0, self.height, TILE_HEIGHT):
                s = slice(y, y + TILE_HEIGHT)
                for x in range(0, self.width):
                    file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c0 for pixel in [row[x] for row in self[s]]]))

            # Save patterns for odd image
            for y in range(0, self.height, TILE_HEIGHT):
                s = slice(y, y + TILE_HEIGHT)
                for x in range(0, self.width):
                    file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p1 for pixel in [row[x] for row in self[s]]]))

            # Save colours for even image
            for y in range(0, self.height, TILE_HEIGHT):
                s = slice(y, y + TILE_HEIGHT)
                for x in range(0, self.width):
                    file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c1 for pixel in [row[x] for row in self[s]]]))
        debug('Done!')


    def to_image(self) -> Image:
        """convert MSXBitmap_105 to PIL Image"""
        dst = Image.new('RGB', (self.width * TILE_WIDTH, self.height))
        width, height = dst.size
        for y in range(height):
            for x in range(self.width):
                for tx in range(TILE_WIDTH):
                    pixel = self[y][x].to_rgb(tx, self.palette)
                    dst.putpixel((x * TILE_WIDTH + tx, y), pixel)
        return dst


    def save_bitmap(self, filename: str) -> None:
        """save MSXBitmap_105 as a PNG image"""
        self.to_image().save(filename)

