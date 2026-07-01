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
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <format>
#include <filesystem>
#include <vector>

#include "bmpto105_lib.hpp"

// special macros
#ifdef USE_DEBUG
#include "timer.cpp"
#define DEBUG(x) do { x; } while (0)
#else
#define DEBUG(x)
#endif
#ifdef USE_CONSOLE
#define CONSOLE(x) do { x; } while (0)
#else
#define CONSOLE(x)
#endif
#define CONSOLE_OFF(x)

// constants and useful macros
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MAKE_COLOR105(c0, c1, pal) \
    Color105{ \
        c0, c1, \
        static_cast<uint8_t>((pal[c0].r + pal[c1].r) / 2), \
        static_cast<uint8_t>((pal[c0].g + pal[c1].g) / 2), \
        static_cast<uint8_t>((pal[c0].b + pal[c1].b) / 2)  \
    }

static void printColorCombo(std::array<Color105, 4>& colors)
{
	std::cerr << "color combo: "
			<< (int)colors[0].rgb.r << "," << (int)colors[0].rgb.g << "," << (int)colors[0].rgb.b << ", "
			<< (int)colors[1].rgb.r << "," << (int)colors[1].rgb.g << "," << (int)colors[1].rgb.b << ", "
			<< (int)colors[2].rgb.r << "," << (int)colors[2].rgb.g << "," << (int)colors[2].rgb.b << ", "
			<< (int)colors[3].rgb.r << "," << (int)colors[3].rgb.g << "," << (int)colors[3].rgb.b << "\n";
}

static void printRGBColor(RGBColor& color)
{
	std::cerr << "color: " << (int)color.r << "," << (int)color.g << "," << (int)color.b << "\n";
}

static int createColorCombo(std::vector<RGBColor>& palette, std::vector<std::array<Color105, 4>>& colorCombo)
{
	colorCombo.reserve(6020);

	for (uint8_t bg0 = 1; bg0 < 16; bg0++)
	{
		for (uint8_t fg0 = bg0 + 1; fg0 < 16; fg0++)
		{
			for (uint8_t bg1 = bg0; bg1 < 16; bg1++)
			{
				for (uint8_t fg1 = bg1 + 1; fg1 < 16; fg1++)
				{
					// create 4-colour combinations
					colorCombo.push_back({
						// 0: even frame bg (bg0) and odd frame bg (bg1)
						MAKE_COLOR105(bg0, bg1, palette),
						// 1: even frame fg (fg0) and odd frame bg (bg1)
						MAKE_COLOR105(fg0, bg1, palette),
						// 2: even frame bg (bg0) and odd frame fg (fg1)
						MAKE_COLOR105(bg0, fg1, palette),
						// 3: even frame fg (fg0) and odd frame fg (fg1)
						MAKE_COLOR105(fg0, fg1, palette)
					});

					CONSOLE_OFF(
						auto colors = colorCombo[colorCombo.size() - 1];
						printColorCombo(colors);
					);
				}

			}
		}
	}

	return colorCombo.size();
}


static inline uint32_t COLOR_MSE(const RGBColor& c1, const RGBColor& c2) {
	int dr = c1.r - c2.r;
	int dg = c1.g - c2.g;
	int db = c1.b - c2.b;
	return dr*dr + dg*dg + db*db;
}

class ModuleEngine {
public:
	ModuleEngine(const std::array<uint32_t, 16>& palette_)
	{
			int paletteSize = initPalette(palette_);
			int colorComboSize = createColorCombo(palette, colorComboTable);

			CONSOLE_OFF(
				std::cout << "palette size = " << paletteSize << "\n";
				std::cout << "color table size = " << colorComboSize << "\n";
			);

			(void)paletteSize;
			(void)colorComboSize;
	}

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

int ModuleEngine::initPalette(const std::array<uint32_t, 16>& palette_)
{
	palette.reserve(palette_.size());
	for (unsigned int i = 0; i < palette_.size(); i++)
	{
		palette.push_back({
			(uint8_t) ((palette_[i] >> 16) & 0xff), // r
			(uint8_t) ((palette_[i] >>  8) & 0xff), // g
			(uint8_t) ((palette_[i] >>  0) & 0xff)  // b
		});
	}
	return palette.size();
}

const std::vector<RGBColor>& ModuleEngine::getPalette()
{
	return palette;
}

// find the 4-colour combination in msxColorTable that best matches all colors in a 8x1 "tile"
uint32_t ModuleEngine::findBestMatch(RGBColor* source)
{
	DEBUG(
		benchmarker.start("Find best match");
	);

	uint32_t bestIndex = 0;
	uint32_t bestMse   = 0xffffffff;

	// Search all 4-colours combo for one that best matches 8x1 "tile"
	for (uint32_t i = 0; i < colorComboTable.size(); i++)
	{
		uint32_t mse = 0;

		RGBColor rgb0 = colorComboTable[i][0].rgb; // combo bg0 | bg1
		RGBColor rgb1 = colorComboTable[i][1].rgb; // combo fg0 | bg1
		RGBColor rgb2 = colorComboTable[i][2].rgb; // combo fg1 | bg0
		RGBColor rgb3 = colorComboTable[i][3].rgb; // combo fg0 | fg1

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

	DEBUG(
		benchmarker.stop();
	);

	return bestIndex;
}

MSX105Bitmap* ModuleEngine::convertImage(RGBBitmap& img)
{
	MSX105Bitmap* msx = (MSX105Bitmap*) calloc(1, sizeof(MSX105Bitmap) + 4 * (img.width / 8) * img.height);
	msx->width  = img.width / 8;
	msx->height = img.height / 8;

	for (int h = 0; h < img.height; h++)
	{
		CONSOLE_OFF(
			std::cout << std::format("\rConverting line {}/{}", h + 1, 8 * msx->height) << std::flush;
		);

		for (uint32_t w = 0; w < msx->width; w++)
		{
			RGBColor* ptr = (RGBColor*) (img.data + (h * img.width + w * 8) * img.channels);

			uint32_t idx = findBestMatch(ptr);
			CONSOLE(std::cerr << "findBestMatch " << idx << "\n";);

			// select which 4-colour combination of alternating (fg | bg) is more similar to the current pixel.
			RGBColor rgb0 = colorComboTable[idx][0].rgb;
			RGBColor rgb1 = colorComboTable[idx][1].rgb;
			RGBColor rgb2 = colorComboTable[idx][2].rgb;
			RGBColor rgb3 = colorComboTable[idx][3].rgb;

			uint8_t pattern0 = 0;
			uint8_t pattern1 = 0;

			for (int i = 0, j = 0; j < 8; j++, i = 0)
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
				for (int k = 1; k < 4; ++k) {
					i = t[i] < t[k] ? i : k;
				}

				// set patterns accordingly
				pattern0 |= (i & 1);
				pattern1 |= (i & 2) >> 1;
			}

			// even frame combination: bg0 | fg0
			auto c0 = colorComboTable[idx][0].msx.c0 + 16 * colorComboTable[idx][1].msx.c0;
			msx->bitmap[h * msx->width + w].c0 = c0;
			// even frame pattern
			msx->bitmap[h * msx->width + w].p0 = pattern0;
			// odd frame combination: bg1 | fg1
			auto c1 = colorComboTable[idx][0].msx.c1 + 16 * colorComboTable[idx][2].msx.c1;
			msx->bitmap[h * msx->width + w].c1 = c1;
			// odd frame pattern
			msx->bitmap[h * msx->width + w].p1 = pattern1;

			CONSOLE(
				std::cerr << "data "
					<< (int)pattern0 << "," << (int)pattern1 << ","
					<< (int)c0 << "," << (int)c1 << "\n";
			);
		}
	}

	CONSOLE_OFF(
		std::cout << "\n" << std::flush;
	);

	return msx;
}

