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
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


// Measure performance
#include "timer.cpp"
Benchmarker benchmarker;

#define TILE_WIDTH 8
#define NIBBLE_SIZE 4
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct RgbColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct RgbBitmap
{
    int width;
    int height;
    int channels;
    uint8_t* data;
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
        msxPalette[i].r = (uint8_t) ((defaultPalette[i] >> 16) & 0xff);
        msxPalette[i].g = (uint8_t) ((defaultPalette[i] >>  8) & 0xff);
        msxPalette[i].b = (uint8_t) ((defaultPalette[i] >>  0) & 0xff);
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
                    msxColorTable[i][0].rgb.r  = ((int)msxPalette[bg0].r + msxPalette[bg1].r) / 2;
                    msxColorTable[i][0].rgb.g  = ((int)msxPalette[bg0].g + msxPalette[bg1].g) / 2;
                    msxColorTable[i][0].rgb.b  = ((int)msxPalette[bg0].b + msxPalette[bg1].b) / 2;

                    // 1: even frame fg (fg0) and odd frame bg (bg1)
                    msxColorTable[i][1].msx.c0 = fg0;
                    msxColorTable[i][1].msx.c1 = bg1;
                    msxColorTable[i][1].rgb.r  = ((int)msxPalette[fg0].r + msxPalette[bg1].r) / 2;
                    msxColorTable[i][1].rgb.g  = ((int)msxPalette[fg0].g + msxPalette[bg1].g) / 2;
                    msxColorTable[i][1].rgb.b  = ((int)msxPalette[fg0].b + msxPalette[bg1].b) / 2;

                    // 2: even frame bg (bg0) and odd frame fg (fg1)
                    msxColorTable[i][2].msx.c0 = bg0;
                    msxColorTable[i][2].msx.c1 = fg1;
                    msxColorTable[i][2].rgb.r  = ((int)msxPalette[bg0].r + msxPalette[fg1].r) / 2;
                    msxColorTable[i][2].rgb.g  = ((int)msxPalette[bg0].g + msxPalette[fg1].g) / 2;
                    msxColorTable[i][2].rgb.b  = ((int)msxPalette[bg0].b + msxPalette[fg1].b) / 2;

                    // 3: even frame fg (fg0) and odd frame fg (fg1)
                    msxColorTable[i][3].msx.c0 = fg0;
                    msxColorTable[i][3].msx.c1 = fg1;
                    msxColorTable[i][3].rgb.r  = ((int)msxPalette[fg0].r + msxPalette[fg1].r) / 2;
                    msxColorTable[i][3].rgb.g  = ((int)msxPalette[fg0].g + msxPalette[fg1].g) / 2;
                    msxColorTable[i][3].rgb.b  = ((int)msxPalette[fg0].b + msxPalette[fg1].b) / 2;

                    msxColorTableSize++;
                }
            }
        }
    }
}

inline uint32_t COLOR_MSE(const RgbColor& c1, const RgbColor& c2) {
    int dr = c1.r - c2.r;
    int dg = c1.g - c2.g;
    int db = c1.b - c2.b;
    return dr*dr + dg*dg + db*db;
}

// find the 4-colour combination in msxColorTable that best matches all colors in a 8x1 "tile"
uint32_t findBestMatch(RgbColor* source)
{
    benchmarker.start("Find best match");
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

    benchmarker.stop();
    return bestIndex;
}

Msx105Bitmap* convertImage(RgbBitmap& img)
{
    Msx105Bitmap* msxBm = (Msx105Bitmap*) std::calloc(1, sizeof(Msx105Bitmap) + 4 * (img.width / 8) * img.height);
    msxBm->width  = img.width / 8;
    msxBm->height = img.height / 8;

    for (uint32_t h = 0; h < img.height; h++)
    {
        std::cout << std::format("\rConverting line {}/{}", h + 1, 8 * msxBm->height);

        for (uint32_t w = 0; w < msxBm->width; w++)
        {
            RgbColor* ptr = (RgbColor*) (img.data + (h * img.width + w * 8) * img.channels);

            uint32_t idx = findBestMatch(ptr);

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
                    COLOR_MSE(rgb0, ptr[j]),
                    COLOR_MSE(rgb1, ptr[j]),
                    COLOR_MSE(rgb2, ptr[j]),
                    COLOR_MSE(rgb3, ptr[j])
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

void saveBitmapMSX(const std::string& filename, Msx105Bitmap* msxBm)
{
    FILE* f = fopen(filename.c_str(), "wb");
    if (f == NULL)
    {
        std::cout << std::format("ERROR: Failed opening file \"{}\"\n", filename);
        return;
    }

    std::cout << std::format("Saving image: \"{}\"\n", filename);

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

void saveBitmap105(const std::string& filename, Msx105Bitmap* msx)
{
    std::cout << "saveBitmap105 " << msx->width << ", " << msx->height << "\n";
    const int width = msx->width * 8;
    const int height = msx->height * 8;
    const int channels = 3;
    const int stride = width * channels;
    std::vector<uint8_t> data(width * height * channels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < msx->width; ++x) {
            for (int tx = 0; tx < 8; ++tx) {
                uint8_t bit = 1 << ((TILE_WIDTH - 1) - tx);
                uint8_t c0 = msx->bitmap[y * msx->width + x].c0;
                uint8_t fg0 = (c0 & 0xf0) >> NIBBLE_SIZE;
                uint8_t bg0 = (c0 & 0x0f);
                uint8_t p0 = bit & msx->bitmap[y * msx->width + x].p0;
                uint8_t c1 = msx->bitmap[y * msx->width + x].c1;
                uint8_t fg1 = (c1 & 0xf0) >> NIBBLE_SIZE;
                uint8_t bg1 = (c1 & 0x0f);
                uint8_t p1 = bit & msx->bitmap[y * msx->width + x].p1;
                uint8_t r = ((p0 ? msxPalette[fg0].r : msxPalette[bg0].r) + (p1 ? msxPalette[fg1].r : msxPalette[bg1].r)) / 2;
                uint8_t g = ((p0 ? msxPalette[fg0].g : msxPalette[bg0].g) + (p1 ? msxPalette[fg1].g : msxPalette[bg1].g)) / 2;
                uint8_t b = ((p0 ? msxPalette[fg0].b : msxPalette[bg0].b) + (p1 ? msxPalette[fg1].b : msxPalette[bg1].b)) / 2;

                data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 0] = r;
                data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 1] = g;
                data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 2] = b;
            }
        }
    }

    if (stbi_write_png(filename.c_str(), width, height, channels, data.data(), stride)) {
        std::cout << "Image saved: " << filename << std::endl;
    } else {
        std::cerr << "Failed to save image!" << std::endl;
    }
}

bool loadImage(RgbBitmap& image, const std::string& filename)
{
    image.data = stbi_load(filename.c_str(), &image.width, &image.height, &image.channels, 0);

    if (!image.data) {
        std::cerr << "Failed to load image.\n" << std::endl;
        return false;
    }

    std::cout << "Image size: " << image.width << " x " << image.height << " (" << image.channels << " channels)\n";
    return true;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <image filename>\n\n";
        return 1;
    }

    std::string imgFilename = std::string(argv[1]);

    createColorTable();

    RgbBitmap image;
    if (!loadImage(image, imgFilename)) {
        return 2;
    }

    Msx105Bitmap* msxBm = convertImage(image);

    std::filesystem::path filePathMSX(imgFilename);

    filePathMSX.replace_extension(".si2");
    saveBitmapMSX(filePathMSX.string(), msxBm);

    filePathMSX.replace_extension(".105.png");
    saveBitmap105(filePathMSX.string(), msxBm);

    benchmarker.print_results();
    return 0;
}
