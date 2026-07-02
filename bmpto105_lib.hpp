/*****************************************************************************
**
** Copyright (C) 2006 Daniel Vik, 2026 Pedro de Medeiros
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
#include <cstdint>
#include <vector>
#include <array>

struct RGBColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct RGBBitmap
{
    int width;
    int height;
    int channels;
    std::vector<uint8_t> data;  // Using vector for automatic memory management

    // Constructor
    RGBBitmap(int w = 0, int h = 0, int c = 3)
        : width(w), height(h), channels(c), data(w * h * c, 0) {}

    RGBBitmap(int w, int h, int c, const std::vector<uint8_t>& d)
        : width(w), height(h), channels(c), data(d) {}
};

struct MSX105Bitmap
{
    uint32_t width;
    uint32_t height;
    struct
    {
        uint8_t c0;
        uint8_t p0;
        uint8_t c1;
        uint8_t p1;
    } bitmap[]; // grows dinamically

    // Constructor
    MSX105Bitmap(int w = 0, int h = 0)
        : width(w), height(h) {}
};

struct Color105
{
    struct {
        uint8_t c0;
        uint8_t c1;
    } msx;
    RGBColor rgb;
};

class BmpTo105 {
public:
        BmpTo105(const std::array<uint32_t, 16>& palette_);

        int initPalette(const std::array<uint32_t, 16>& palette_);

        const std::vector<RGBColor>& getPalette();

        MSX105Bitmap* convertImage(RGBBitmap& image);

private:
        uint32_t findBestMatch(RGBColor* source);

        std::vector<RGBColor> palette;
        std::vector<std::array<Color105, 4>> colorComboTable;

#ifdef USE_DEBUG
        // Measure performance
        Benchmarker benchmarker;
#endif
};

