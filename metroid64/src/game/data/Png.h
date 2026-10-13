#pragma once

// Minimal PNG reading and writing for the graphics resources (zlib for the compression). Pixels are the
// 2-bit values of NES tiles, 0-3.

#include <cstdint>
#include <string>
#include <vector>

namespace Game::Data {

// A PNG of `width` x `height` pixels (values 0-3, row by row): 2-bit indexed, with a palette of four
// greys (black, dark grey, light grey, white) so the pixel values are the palette indices.
std::string WritePng(int width, int height, const std::vector<uint8_t>& pixels);

// Reads a PNG (any bit depth and colour type, not interlaced) into pixel values 0-3:
// - indexed with at most 4 palette entries: the index;
// - anything else: the nearest of the four greys (0, 55, AA, FF; colours by the average of R, G and B);
//   fully transparent pixels (alpha below half) are 0.
bool ReadPng(const std::string& file, int& width, int& height, std::vector<uint8_t>& pixels, std::string& error);

} // namespace Game::Data
