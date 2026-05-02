/*****************************************************************************
**
** Copyright (C) 2006 Daniel Vik
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
*/
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <format>
#include <cstdlib>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

union RgbColor
{
    struct 
    {
        uint8_t unused;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } rgb;
    uint32_t raw;
};

struct RgbBitmap
{
    uint32_t width;
    uint32_t height;
    RgbColor bitmap[1];
};

struct Msx105Bitmap
{
    uint32_t width;
    uint32_t height;
    struct
    {
        uint8_t c0;
        uint8_t p0;
        uint8_t c1;
        uint8_t p1;
    } bitmap[1];
};

struct BmpHeader {
    uint32_t size;
    uint32_t reserved1;
    uint32_t offBits;
    uint32_t bmHeaderSize;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t sizeImage;
    uint32_t xPelsPerMeter;
    uint32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};

RgbBitmap* loadBitmap(const char* fileName)
{
    FILE* f;

    f = fopen(fileName, "rb");

    if (f == NULL)
    {
        std::cout << std::format("ERROR: Failed opening file \"{}\"\n", fileName);
        return NULL;
    }

    char fileType[2];
    if (fread(fileType, 1, 2, f) != 2) {
        std::cout << "ERROR: Failed reading bitmap header.\n";
        fclose(f);
        return NULL;
    }

    if (memcmp(fileType, "BM", 2) != 0) {
        std::cout << "ERROR: Not a valid bitmap file.\n";
        fclose(f);
        return NULL;
    }

    BmpHeader bmpHeader;
    if (fread(&bmpHeader, 1, sizeof(bmpHeader), f) != sizeof(bmpHeader)) {
        std::cout << "ERROR: Failed reading bitmap header.\n";
        fclose(f);
        return NULL;
    }

    if (bmpHeader.planes != 1 || bmpHeader.compression != 0 || bmpHeader.bitCount != 24)
    {
        std::cout << "ERROR: Unsupported bitmap format.\n";
        fclose(f);
        return NULL;
    }

    RgbBitmap* bm = (RgbBitmap*) std::calloc(1, sizeof(RgbBitmap) + bmpHeader.width * bmpHeader.height * sizeof(RgbColor));
    if (bm == NULL)
    {
        std::cout << "ERROR: Failed allocating memory for bitmap.\n";
        fclose(f);
        return NULL;
    }

    bm->width = bmpHeader.width;
    bm->height = bmpHeader.height;

    for (uint32_t y = 0; y < bm->height; y++)
    {
        for (uint32_t x = 0; x < bm->width; x++)
        {
            uint8_t color[3];
            if (fread(color, 1, 3, f) != 3) {
                std::free(bm);
                std::cout << "ERROR: Failed reading bitmap data.\n";
                fclose(f);
                return NULL;
            }
            uint32_t i = (bm->height - y - 1) * bm->width + x;
            bm->bitmap[i].rgb.b = color[0];
            bm->bitmap[i].rgb.g = color[1];
            bm->bitmap[i].rgb.r = color[2];
        }
    }

    fclose(f);

    return bm;
}


const uint32_t defaultPalette[16] =
{
    0x000000, 0x000000, 0x24da24, 0x68ff68, 0x2424ff, 0x4868ff, 0xb62424, 0x48daff,
    0xff2424, 0xff6868, 0xdada24, 0xdada91, 0x249124, 0xda48b6, 0xb6b6b6, 0xffffff
};

RgbColor msxPalette[16];

void initPalette()
{
    for (int i = 0; i < 16; i++)
    {
        msxPalette[i].rgb.unused = 0;
        msxPalette[i].rgb.r = (uint8_t) ((defaultPalette[i] >> 16) & 0xff);
        msxPalette[i].rgb.g = (uint8_t) ((defaultPalette[i] >>  8) & 0xff);
        msxPalette[i].rgb.b = (uint8_t) ((defaultPalette[i] >>  0) & 0xff);
    }
}

struct Color105
{
    struct {
        uint8_t c0;
        uint8_t c1;
    } msx;
    RgbColor rgb;
};

uint32_t   msxColorTableSize = 0;
Color105 msxColorTable[8192][4];

void createColorTable()
{
    initPalette();

    for (int bg0 = 1; bg0 < 16; bg0++)
    {
        for (int fg0 = bg0 + 1; fg0 < 16; fg0++)
        {
            for (int bg1 = bg0; bg1 < 16; bg1++)
            {
                for (int fg1 = bg1 + 1; fg1 < 16; fg1++)
                {
                    uint32_t i = msxColorTableSize;

                    // create 4-colour combinations
                    // 0: even frame bg (bg0) and odd frame bg (bg1)
                    msxColorTable[i][0].msx.c0 = bg0;
                    msxColorTable[i][0].msx.c1 = bg1;
                    msxColorTable[i][0].rgb.rgb.r  = ((int)msxPalette[bg0].rgb.r + msxPalette[bg1].rgb.r) / 2;
                    msxColorTable[i][0].rgb.rgb.g  = ((int)msxPalette[bg0].rgb.g + msxPalette[bg1].rgb.g) / 2;
                    msxColorTable[i][0].rgb.rgb.b  = ((int)msxPalette[bg0].rgb.b + msxPalette[bg1].rgb.b) / 2;

                    // 1: even frame fg (fg0) and odd frame bg (bg1)
                    msxColorTable[i][1].msx.c0 = fg0;
                    msxColorTable[i][1].msx.c1 = bg1;
                    msxColorTable[i][1].rgb.rgb.r  = ((int)msxPalette[fg0].rgb.r + msxPalette[bg1].rgb.r) / 2;
                    msxColorTable[i][1].rgb.rgb.g  = ((int)msxPalette[fg0].rgb.g + msxPalette[bg1].rgb.g) / 2;
                    msxColorTable[i][1].rgb.rgb.b  = ((int)msxPalette[fg0].rgb.b + msxPalette[bg1].rgb.b) / 2;

                    // 2: even frame bg (bg0) and odd frame fg (fg1)
                    msxColorTable[i][2].msx.c0 = bg0;
                    msxColorTable[i][2].msx.c1 = fg1;
                    msxColorTable[i][2].rgb.rgb.r  = ((int)msxPalette[bg0].rgb.r + msxPalette[fg1].rgb.r) / 2;
                    msxColorTable[i][2].rgb.rgb.g  = ((int)msxPalette[bg0].rgb.g + msxPalette[fg1].rgb.g) / 2;
                    msxColorTable[i][2].rgb.rgb.b  = ((int)msxPalette[bg0].rgb.b + msxPalette[fg1].rgb.b) / 2;

                    // 3: even frame fg (fg0) and odd frame fg (fg1)
                    msxColorTable[i][3].msx.c0 = fg0;
                    msxColorTable[i][3].msx.c1 = fg1;
                    msxColorTable[i][3].rgb.rgb.r  = ((int)msxPalette[fg0].rgb.r + msxPalette[fg1].rgb.r) / 2;
                    msxColorTable[i][3].rgb.rgb.g  = ((int)msxPalette[fg0].rgb.g + msxPalette[fg1].rgb.g) / 2;
                    msxColorTable[i][3].rgb.rgb.b  = ((int)msxPalette[fg0].rgb.b + msxPalette[fg1].rgb.b) / 2;

                    msxColorTableSize++;
                }
            }
        }
    }
}

inline uint32_t COLOR_MSE(const RgbColor& c1, const RgbColor& c2) {
    int dr = c1.rgb.r - c2.rgb.r;
    int dg = c1.rgb.g - c2.rgb.g;
    int db = c1.rgb.b - c2.rgb.b;
    return dr*dr + dg*dg + db*db;
}

// find the 4-colour combination in msxColorTable that best matches all colors in a 8x1 "tile"
uint32_t findBestMatch(RgbColor* source)
{
    uint32_t bestIndex = 0;
    uint32_t bestMse   = 0xffffffff;

    // Search all 4-colours combo for one that best matches 8x1 "tile"
    for (uint32_t i = 0; i < msxColorTableSize; i++)
    {
        uint32_t mse = 0;

        RgbColor rgb0 = msxColorTable[i][0].rgb; // combo bg0 | bg1
        RgbColor rgb1 = msxColorTable[i][1].rgb; // combo fg0 | bg1
        RgbColor rgb2 = msxColorTable[i][2].rgb; // combo fg1 | bg0
        RgbColor rgb3 = msxColorTable[i][3].rgb; // combo fg0 | fg1

        // scan 8x1 from tile
        for (uint32_t j = 0; j < 8; j++)
        {
            // select which colour combination of alternating (fg | bg) is more similar
            uint32_t t0 = COLOR_MSE(rgb0, source[j]);
            uint32_t t1 = COLOR_MSE(rgb1, source[j]);
            uint32_t t2 = COLOR_MSE(rgb2, source[j]);
            uint32_t t3 = COLOR_MSE(rgb3, source[j]);

            // get smaller distance of all colour combinations
            uint32_t m0 = MIN(t0, t1);
            uint32_t m1 = MIN(t2, t3);
            mse += MIN(m0, m1);
        }

        if (mse < bestMse)
        {
            bestMse = mse;
            bestIndex = i;
        }
    }

    return bestIndex;
}

Msx105Bitmap* convertBitmap(RgbBitmap* rgbBm)
{
    Msx105Bitmap* msxBm = (Msx105Bitmap*) std::calloc(1, sizeof(Msx105Bitmap) + 4 * (rgbBm->width / 8) * rgbBm->height);
    msxBm->width  = rgbBm->width / 8;
    msxBm->height = rgbBm->height / 8;

    for (uint32_t h = 0; h < rgbBm->height; h++)
    {
        std::cout << std::format("\rConverting line {}/{}", h + 1, 8 * msxBm->height);

        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            RgbColor* rgbBitmap = rgbBm->bitmap + h * rgbBm->width + w * 8;

            uint32_t idx = findBestMatch(rgbBitmap);

            // select which 4-colour combination of alternating (fg | bg) is more similar to the current pixel.
            RgbColor rgb0 = msxColorTable[idx][0].rgb;
            RgbColor rgb1 = msxColorTable[idx][1].rgb;
            RgbColor rgb2 = msxColorTable[idx][2].rgb;
            RgbColor rgb3 = msxColorTable[idx][3].rgb;

            uint8_t pattern0 = 0;
            uint8_t pattern1 = 0;

            for (uint32_t j = 0; j < 8; j++)
            {
                // shift left to prepare setting next bit
                pattern0 <<= 1;
                pattern1 <<= 1;

                // get distance of each possible colour from the 4-colour combo to the actual pixel colour
                // is this really necessary? Best match was already found by findBestMatch. Can't we use
                // second, third and fourth values?
                uint32_t t[4] = {
                    COLOR_MSE(rgb0, rgbBitmap[j]),
                    COLOR_MSE(rgb1, rgbBitmap[j]),
                    COLOR_MSE(rgb2, rgbBitmap[j]),
                    COLOR_MSE(rgb3, rgbBitmap[j])
                };

                // get smaller distance of all colour combinations again.
                uint8_t i = 0;
                for (int k = 1; k < 4; ++k) {
                    i = t[i] < t[k] ? i : k;
                }

                // set patterns accordingly
                pattern0 |= (i & 1);
                pattern1 |= (i & 2) >> 1;
            }

            // even frame combination: bg0 | fg0
            auto c0 = msxColorTable[idx][0].msx.c0 + 16 * msxColorTable[idx][1].msx.c0;
            msxBm->bitmap[h * msxBm->width + w].c0 = c0;
            // even frame pattern
            msxBm->bitmap[h * msxBm->width + w].p0 = pattern0;
            // odd frame combination: bg1 | fg1
            auto c1 = msxColorTable[idx][0].msx.c1 + 16 * msxColorTable[idx][2].msx.c1;
            msxBm->bitmap[h * msxBm->width + w].c1 = c1;
            // odd frame pattern
            msxBm->bitmap[h * msxBm->width + w].p1 = pattern1;
        }
    }

    std::cout << "\n" << std::flush;

    return msxBm;
}

void saveBitmap(const char* fileName, Msx105Bitmap* msxBm)
{
    FILE* f = fopen(fileName, "wb");
    if (f == NULL)
    {
        std::cout << std::format("ERROR: Failed opening file \"{}\"\n", fileName);
        return;
    }

    std::cout << std::format("Saving image: \"{}\"\n", fileName);

    uint8_t header[2] = { (uint8_t) msxBm->width, (uint8_t) msxBm->height } ;

    // Save header
    fwrite(header, 1, 2, f);

    // Save pattern for even image
    for (uint32_t h = 0; h < msxBm->height; h++)
    {
        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            for (uint32_t t = 0; t < 8; t++)
            {
                fwrite(&msxBm->bitmap[(h * 8 + t) * msxBm->width + w].p0, 1, 1, f);
            }
        }
    }

    // Save colors for even image
    for (uint32_t h = 0; h < msxBm->height; h++)
    {
        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            for (uint32_t t = 0; t < 8; t++)
            {
                fwrite(&msxBm->bitmap[(h * 8 + t) * msxBm->width + w].c0, 1, 1, f);
            }
        }
    }

    // Save pattern for odd image
    for (uint32_t h = 0; h < msxBm->height; h++)
    {
        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            for (uint32_t t = 0; t < 8; t++)
            {
                fwrite(&msxBm->bitmap[(h * 8 + t) * msxBm->width + w].p1, 1, 1, f);
            }
        }
    }

    // Save colors for odd image
    for (uint32_t h = 0; h < msxBm->height; h++)
    {
        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            for (uint32_t t = 0; t < 8; t++)
            {
                fwrite(&msxBm->bitmap[(h * 8 + t) * msxBm->width + w].c1, 1, 1, f);
            }
        }
    }

    fclose(f);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <filename.bmp>\n\n";
        return 1;
    }

    char* bmpFilename = argv[1];

    createColorTable();

    RgbBitmap* rgbBm = loadBitmap(bmpFilename);
    if (rgbBm == NULL)
    {
        return 2;
    }

    Msx105Bitmap* msxBm = convertBitmap(rgbBm);

    // Create filename for destination file (change extension to .si2)
    char msxFilename[512];
    strcpy(msxFilename, bmpFilename);

    char* ptr = msxFilename + strlen(msxFilename) - 1;

    while (*ptr != '.')
    {
        ptr--;
    }
    *ptr = 0;

    strcat(msxFilename, ".si2");

    saveBitmap(msxFilename, msxBm);

    return 0;
}
