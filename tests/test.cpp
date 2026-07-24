/*****************************************************************************
**
** Copyright (C) 2026 Pedro de Medeiros
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

//#define USE_DEBUG
//#define USE_CONSOLE

#include "../bmpto105/libbmpto105.hpp"

#define TILE_WIDTH 8
#define NIBBLE_SIZE 4

void saveMSXBitmap(const std::string& filename, const MSXBitmap_105* msx)
{
	FILE* f = fopen(filename.c_str(), "wb");
	if (f == NULL)
	{
		std::cout << std::format("ERROR: Failed opening file \"{}\"\n", filename);
		return;
	}

	CONSOLE(
		std::cout << std::format("Saving image: \"{}\"\n", filename);
	);

	uint8_t header[2] = { (uint8_t) msx->width, (uint8_t) msx->height } ;

	// Save header
	fwrite(header, 1, 2, f);

	// Save pattern for even image
	for (uint32_t h = 0; h < msx->height; h++)
	{
		for (uint32_t w = 0; w < msx->width; w++)
		{
			for (uint32_t t = 0; t < 8; t++)
			{
				fwrite(&msx->bitmap[(h * 8 + t) * msx->width + w].p0, 1, 1, f);
			}
		}
	}

	// Save colors for even image
	for (uint32_t h = 0; h < msx->height; h++)
	{
		for (uint32_t w = 0; w < msx->width; w++)
		{
			for (uint32_t t = 0; t < 8; t++)
			{
				fwrite(&msx->bitmap[(h * 8 + t) * msx->width + w].c0, 1, 1, f);
			}
		}
	}

	// Save pattern for odd image
	for (uint32_t h = 0; h < msx->height; h++)
	{
		for (uint32_t w = 0; w < msx->width; w++)
		{
			for (uint32_t t = 0; t < 8; t++)
			{
				fwrite(&msx->bitmap[(h * 8 + t) * msx->width + w].p1, 1, 1, f);
			}
		}
	}

	// Save colors for odd image
	for (uint32_t h = 0; h < msx->height; h++)
	{
		for (uint32_t w = 0; w < msx->width; w++)
		{
			for (uint32_t t = 0; t < 8; t++)
			{
				fwrite(&msx->bitmap[(h * 8 + t) * msx->width + w].c1, 1, 1, f);
			}
		}
	}

	fclose(f);
}

void saveBitmap(const std::string& filename, const MSXBitmap_105* msx, const std::vector<RGBColor>& palette)
{
	CONSOLE(
		std::cout << "Save " << msx->width << "x" << msx->height << " 105-colour bitmap\n";
	);
	const int width = msx->width * 8;
	const int height = msx->height * 8;
	const int channels = 3;
	const int stride = width * channels;
	std::vector<uint8_t> data(width * height * channels);

	for (int y = 0; y < height; ++y) {
		for (unsigned int x = 0; x < msx->width; ++x) {
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
				uint8_t r = ((p0 ? palette[fg0].r : palette[bg0].r) + (p1 ? palette[fg1].r : palette[bg1].r)) / 2;
				uint8_t g = ((p0 ? palette[fg0].g : palette[bg0].g) + (p1 ? palette[fg1].g : palette[bg1].g)) / 2;
				uint8_t b = ((p0 ? palette[fg0].b : palette[bg0].b) + (p1 ? palette[fg1].b : palette[bg1].b)) / 2;

				data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 0] = r;
				data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 1] = g;
				data[((y * width) + (x * TILE_WIDTH) + tx) * channels + 2] = b;
			}
		}
	}

	if (stbi_write_png(filename.c_str(), width, height, channels, data.data(), stride)) {
		CONSOLE(
			std::cout << "Image saved as '" << filename << "'\n";
		);
	} else {
		std::cerr << "Failed to save image!\n";
	}
}

bool loadImage(RGBBitmap& image, const std::string& filename)
{
	const uint8_t* data = stbi_load(filename.c_str(), &image.width, &image.height, &image.channels, 0);
	if (!data) {
		std::cerr << "Failed to load image.\n";
		return false;
	}

	CONSOLE_OFF(
		std::cout << "Image size: " << image.width << " x " << image.height << " (" << image.channels << " channels)\n";
	);

	image.ref = std::span<const uint8_t>(data, image.width * image.height * image.channels);
	return true;
}

int main(int argc, char* argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " <image filename>\n\n";
		return 1;
	}

	const std::array<uint32_t, 16> msxPalette = {
		0x000000, 0x000000, 0x24da24, 0x68ff68, 0x2424ff, 0x4868ff, 0xb62424, 0x48daff,
		0xff2424, 0xff6868, 0xdada24, 0xdada91, 0x249124, 0xda48b6, 0xb6b6b6, 0xffffff
	};

	std::string imgFilename = std::string(argv[1]);
	RGBBitmap image;
	if (!loadImage(image, imgFilename)) {
		return 2;
	}

	BmpTo105 engine(msxPalette);
	auto palette = engine.getPalette();
	MSXBitmap_105* msx = engine.convertImage(image);

	std::filesystem::path filePathMSX(imgFilename);

	filePathMSX.replace_extension(".si2");
	saveMSXBitmap(filePathMSX.string(), msx);

	filePathMSX.replace_extension(".105.png");
	saveBitmap(filePathMSX.string(), msx, palette);

	//benchmarker.print_results();

	stbi_image_free(const_cast<uint8_t*>(image.ref.data()));

	std::cout << "✅ execution successful!\n";
	return 0;
}
