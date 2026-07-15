import numpy as np
import hashlib

from scipy.fftpack import dct, idct
from typing import List, Tuple
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


#
# Singular Vector Decompostion
#


def approximate_tile_rgb(tile, rank = 8):
    """
    tile:
        numpy uint8_t array (8,8,3)

    rank:
        number of maintained singular values
        (1 <= rank <= 8)

    returns:
        approximate tile (8,8,3)
    """

    tile = np.asarray(tile, dtype=np.float64)

    result = np.empty_like(tile)

    for channel in range(3):
        matrix = tile[:, :, channel]
        U, S, VT = np.linalg.svd(matrix, full_matrices=False)
        S[rank:] = 0
        reconstructed = U @ np.diag(S) @ VT

        result[:, :, channel] = reconstructed
    result = np.clip(np.rint(result), 0, 255)

    return result.astype(np.uint8)


def tile_hash(tile):
    return hashlib.md5(tile).hexdigest()


def remove_similar_tiles(tiles, rank):
    """
    tiles:
        list of arrays (8,8,3)

    returns:
        unique tiles, mapping
    """
    unique = []
    lookup = {}
    mapping = []

    for tile in tiles:
        approx = approximate_tile_rgb(tile, rank)

        h = tile_hash(approx)
        if h in lookup:
            mapping.append(lookup[h])
        else:
            idx = len(unique)
            lookup[h] = idx
            unique.append(tile)
            mapping.append(idx)

    return unique, mapping
