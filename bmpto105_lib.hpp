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

// data types

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
    uint8_t* data; // (r, g, b) data
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
};

struct Color105
{
    struct {
        uint8_t c0;
        uint8_t c1;
    } msx;
    RGBColor rgb;
};
