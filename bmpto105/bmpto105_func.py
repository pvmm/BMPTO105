import numpy as np
import hashlib

from scipy.fftpack import dct, idct

from typing import List, Tuple, TypeVar, Callable
from typing_extensions import Buffer

from PIL import Image


TILE_WIDTH = TILE_HEIGHT = 8


def open_bitmap(filename: str) -> Image.Image:
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


P = TypeVar("P")


def tile_hash(tile: Buffer) -> str:
    return hashlib.md5(tile).hexdigest()


def remove_similar_tiles(tiles: list[Buffer], approximate_tile: Callable[[Buffer, int | float], Buffer], rank: int | float) -> tuple[list[Buffer], list[int]]:
    """
    tiles:
        list of arrays (8,8,3)

    returns:
        unique tiles, mapping
    """
    unique: list[Buffer] = []
    lookup: dict[str, int] = {}
    mapping = []

    for tile in tiles:
        approx = approximate_tile(tile, rank)

        h = tile_hash(approx)
        if h in lookup:
            mapping.append(lookup[h])
        else:
            idx = len(unique)
            lookup[h] = idx
            unique.append(tile)
            mapping.append(idx)

    return unique, mapping
