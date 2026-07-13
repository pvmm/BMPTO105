import struct

from PIL import Image


TILE_WIDTH = TILE_HEIGHT = 8


def open_bitmap(filename: str) -> Image:
    image = Image.open(filename)
    width, height = image.size
    if width % 8 != 0:
        raise TypeError('expected width multiple of 8')
    if width // 8 > 255:
        raise TypeError('width too big')
    if height % 8 != 0:
        raise TypeError('expected height multiple of 8')
    if height // 8 > 255:
        raise TypeError('height too big')
    return image
