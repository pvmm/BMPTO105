import bmpto105

from bmpto105 import RGBBitmap

# Create palette (16 uint32_t values)
palette = [0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF,
           0xFFFF00, 0x00FFFF, 0xFF00FF, 0x800000, 0x808000,
           0x008000, 0x800080, 0x008080, 0x000080, 0x808080,
           0xC0C0C0]

# Create engine instance
engine = bmpto105.BmpTo105(palette)

# Use it
msxImage = engine.convert(RGBBitmap(100, 100, 3))
print(f'msxImage[{msxImage.width}, {msxImage.height}]')

print("✅ Module works!")
