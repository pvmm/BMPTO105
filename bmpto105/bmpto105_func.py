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


def save_bitmap(filename: str, src: MSXBitmap_105) -> None:
    """save MSXBitmap_105 as a PNG image"""
    dst = Image.new('RGB', (src.width * TILE_WIDTH, src.height))
    width, height = dst.size

    for y in range(height):
        for x in range(src.width):
            for tx in range(TILE_WIDTH):
                pixel = src[y][x].rgb(tx, src.palette)
                dst.putpixel((x * TILE_WIDTH + tx, y), pixel)
    dst.save(filename)


def save_msx_bitmap(filename: str, image: MSXBitmap_105) -> None:
    """Save MSXBitmap_105 to disk"""
    print(f'Saving "{filename}"... ', end='')
    with open(filename, 'wb') as file:
        # dimensions header
        file.write(struct.pack('BB', image.width, image.height // 8))

        # Save patterns for even image
        for y in range(0, image.height, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT) # get the tile content from height to height + 8
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p0 for pixel in [row[x] for row in image[s]]]))

        # Save colours for even image
        for y in range(0, image.height, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c0 for pixel in [row[x] for row in image[s]]]))
    
        # Save patterns for odd image
        for y in range(0, image.height, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width):
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.p1 for pixel in [row[x] for row in image[s]]]))

        # Save colours for even image
        for y in range(0, image.height, TILE_HEIGHT):
            s = slice(y, y + TILE_HEIGHT)
            for x in range(0, image.width): 
                file.write(struct.pack(f'{TILE_HEIGHT}B', *[pixel.c1 for pixel in [row[x] for row in image[s]]]))

    print('done!')

