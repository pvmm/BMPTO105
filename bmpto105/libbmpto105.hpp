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
#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <array>
#include <tuple>

#ifdef _USE_DEBUG_
#include <benchmarker.cpp>
#endif


struct RGBColor
{
	uint8_t r;
	uint8_t g;
	uint8_t b;

	std::tuple<uint8_t, uint8_t, uint8_t> rgb() {
		return std::make_tuple(r, g, b);
	}
};

struct RGBBitmap
{
	int width;
	int height;
	int channels;
	std::vector<uint8_t> data;
	std::span<const uint8_t> ref;

	// Constructor
	RGBBitmap() = default;

	RGBBitmap(int w, int h, int c, std::vector<uint8_t> d = {})
		: width(w), height(h), channels(c), data(d), ref(data.data(), w * h * c) {}

	RGBBitmap(int w, int h, int c, const uint8_t* data)
		: width(w), height(h), channels(c), data(), ref(data, w * h * c) {}
};

struct MSXBitmap_105
{
	uint32_t width;
	uint32_t height;

	struct
	{
		uint8_t c0;
		uint8_t p0;
		uint8_t c1;
		uint8_t p1;
	} bitmap[]; // grows dynamically

	// Constructor
	MSXBitmap_105(int w = 0, int h = 0)
		: width(w), height(h) {}
};

struct Color_105
{
	struct {
		uint8_t c0;
		uint8_t c1;
	} msx;
	RGBColor rgb;
};

class BmpTo105 {
public:
	// Constructor taking a span of 16 RGB tuples (as std::tuple)
	BmpTo105(const std::span<const std::tuple<uint8_t, uint8_t, uint8_t>>& palette_);

	// Constructor taking a span of 16 uint32_t color values
	BmpTo105(const std::span<const uint32_t>& palette_);

	// Default constructor (needed for inheritance)
	BmpTo105() = default;

	int initPalette(const std::span<const std::tuple<uint8_t, uint8_t, uint8_t>>& palette_);
	int initPalette(const std::span<const uint32_t>& palette_);

	const std::vector<RGBColor>& getPalette() const;

	MSXBitmap_105* convertImage(RGBBitmap& image);

protected:
	std::vector<RGBColor> palette;
	std::vector<std::array<Color_105, 4>> colorComboTable;

#ifdef _USE_DEBUG_
	// Measure performance
	Benchmarker benchmarker;
#endif

private:
	uint32_t findBestMatch(RGBColor* source);
};

int createColorCombo(const std::vector<RGBColor>& palette, std::vector<std::array<Color_105, 4>>& colorCombo);
