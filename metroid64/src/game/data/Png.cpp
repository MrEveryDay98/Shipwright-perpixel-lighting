#include "game/data/Png.h"

#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace Game::Data {

namespace {

const uint8_t kSignature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

void Put32(std::string& s, uint32_t v) {
    s += (char)(v >> 24);
    s += (char)(v >> 16);
    s += (char)(v >> 8);
    s += (char)v;
}

uint32_t Get32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void Chunk(std::string& out, const char* type, const std::string& data) {
    Put32(out, (uint32_t)data.size());
    std::string body = std::string(type, 4) + data;
    out += body;
    Put32(out, (uint32_t)crc32(0, (const Bytef*)body.data(), (uInt)body.size()));
}

int Paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

bool Inflate(const std::string& in, std::vector<uint8_t>& out) {
    z_stream z = {};
    if (inflateInit(&z) != Z_OK) {
        return false;
    }
    z.next_in = (Bytef*)in.data();
    z.avail_in = (uInt)in.size();
    z.next_out = out.data();
    z.avail_out = (uInt)out.size();
    int r = inflate(&z, Z_FINISH);
    bool ok = (r == Z_STREAM_END || r == Z_BUF_ERROR) && z.avail_out == 0;
    inflateEnd(&z);
    return ok;
}

} // namespace

std::string WritePng(int width, int height, const std::vector<uint8_t>& pixels) {
    std::string out((const char*)kSignature, 8);
    std::string ihdr;
    Put32(ihdr, (uint32_t)width);
    Put32(ihdr, (uint32_t)height);
    ihdr += (char)2; // bit depth
    ihdr += (char)3; // indexed
    ihdr += std::string(3, '\0');
    Chunk(out, "IHDR", ihdr);
    Chunk(out, "PLTE", std::string("\x00\x00\x00\x55\x55\x55\xAA\xAA\xAA\xFF\xFF\xFF", 12));
    size_t rowBytes = ((size_t)width * 2 + 7) / 8;
    std::vector<uint8_t> raw;
    raw.reserve((rowBytes + 1) * height);
    for (int y = 0; y < height; y++) {
        raw.push_back(0); // filter: none
        for (size_t b = 0; b < rowBytes; b++) {
            uint8_t v = 0;
            for (int k = 0; k < 4; k++) {
                size_t x = b * 4 + k;
                uint8_t p = x < (size_t)width ? (pixels[(size_t)y * width + x] & 3) : 0;
                v |= (uint8_t)(p << (6 - 2 * k));
            }
            raw.push_back(v);
        }
    }
    uLongf size = compressBound((uLong)raw.size());
    std::string idat(size, '\0');
    compress2((Bytef*)idat.data(), &size, raw.data(), (uLong)raw.size(), 9);
    idat.resize(size);
    Chunk(out, "IDAT", idat);
    Chunk(out, "IEND", "");
    return out;
}

bool ReadPng(const std::string& file, int& width, int& height, std::vector<uint8_t>& pixels, std::string& error) {
    const uint8_t* p = (const uint8_t*)file.data();
    size_t n = file.size();
    if (n < 8 || memcmp(p, kSignature, 8) != 0) {
        error = "not a PNG";
        return false;
    }
    int depth = 0, color = -1, interlace = 0;
    width = height = 0;
    uint8_t palette[256][4] = {};
    int paletteSize = 0;
    int transparent = -1; // greyscale: the transparent grey level (tRNS)
    std::string idat;
    for (size_t at = 8; at + 12 <= n;) {
        uint32_t len = Get32(p + at);
        if (len > n - at - 12) {
            error = "truncated PNG";
            return false;
        }
        const char* type = (const char*)p + at + 4;
        const uint8_t* d = p + at + 8;
        if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            width = (int)Get32(d);
            height = (int)Get32(d + 4);
            depth = d[8];
            color = d[9];
            interlace = d[12];
        } else if (memcmp(type, "PLTE", 4) == 0) {
            paletteSize = (int)std::min<uint32_t>(len / 3, 256);
            for (int i = 0; i < paletteSize; i++) {
                palette[i][0] = d[i * 3];
                palette[i][1] = d[i * 3 + 1];
                palette[i][2] = d[i * 3 + 2];
                palette[i][3] = 0xFF;
            }
        } else if (memcmp(type, "tRNS", 4) == 0) {
            if (color == 3) {
                for (uint32_t i = 0; i < len && i < 256; i++) {
                    palette[i][3] = d[i];
                }
            } else if (color == 0 && len >= 2) {
                transparent = (d[0] << 8) | d[1];
            }
        } else if (memcmp(type, "IDAT", 4) == 0) {
            idat.append((const char*)d, len);
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        at += 12 + len;
    }
    int channels = 0;
    switch (color) {
        case 0:
        case 3:
            channels = 1;
            break;
        case 2:
            channels = 3;
            break;
        case 4:
            channels = 2;
            break;
        case 6:
            channels = 4;
            break;
        default:
            error = "PNG without a header";
            return false;
    }
    bool depthOk = depth == 8 || (depth == 16 && color != 3) ||
                   ((depth == 1 || depth == 2 || depth == 4) && (color == 0 || color == 3));
    if (!depthOk || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        error = "unsupported PNG format";
        return false;
    }
    if (interlace != 0) {
        error = "interlaced PNGs are not supported (save it without interlacing)";
        return false;
    }
    size_t bitsPerPixel = (size_t)channels * depth;
    size_t rowBytes = ((size_t)width * bitsPerPixel + 7) / 8;
    size_t bpp = std::max<size_t>(1, bitsPerPixel / 8);
    std::vector<uint8_t> raw((rowBytes + 1) * height);
    if (!Inflate(idat, raw)) {
        error = "bad PNG image data";
        return false;
    }
    // Undo the row filters in place.
    for (int y = 0; y < height; y++) {
        uint8_t* row = &raw[(rowBytes + 1) * y + 1];
        const uint8_t* prev = y > 0 ? &raw[(rowBytes + 1) * (y - 1) + 1] : nullptr;
        uint8_t filter = row[-1];
        for (size_t i = 0; i < rowBytes; i++) {
            int a = i >= bpp ? row[i - bpp] : 0;
            int b = prev ? prev[i] : 0;
            int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            switch (filter) {
                case 0:
                    break;
                case 1:
                    row[i] = (uint8_t)(row[i] + a);
                    break;
                case 2:
                    row[i] = (uint8_t)(row[i] + b);
                    break;
                case 3:
                    row[i] = (uint8_t)(row[i] + ((a + b) >> 1));
                    break;
                case 4:
                    row[i] = (uint8_t)(row[i] + Paeth(a, b, c));
                    break;
                default:
                    error = "bad PNG row filter";
                    return false;
            }
        }
    }
    auto sample = [&](const uint8_t* row, size_t index) -> unsigned {
        if (depth == 16) {
            return (row[index * 2] << 8) | row[index * 2 + 1];
        }
        if (depth == 8) {
            return row[index];
        }
        size_t bit = index * depth;
        return (row[bit / 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1);
    };
    unsigned maxSample = (1u << depth) - 1;
    auto grey = [&](unsigned v) { return (int)(v * 255 / maxSample); };
    auto level = [](int g) { return (uint8_t)((g + 42) / 85); };
    pixels.assign((size_t)width * height, 0);
    for (int y = 0; y < height; y++) {
        const uint8_t* row = &raw[(rowBytes + 1) * y + 1];
        for (int x = 0; x < width; x++) {
            uint8_t& out = pixels[(size_t)y * width + x];
            size_t s = (size_t)x * channels;
            if (color == 3) {
                unsigned i = sample(row, s);
                if (paletteSize <= 4) {
                    out = (uint8_t)(i & 3);
                } else if (palette[i][3] < 0x80) {
                    out = 0;
                } else {
                    out = level((palette[i][0] + palette[i][1] + palette[i][2]) / 3);
                }
            } else if (color == 0) {
                unsigned v = sample(row, s);
                out = (int)v == transparent ? 0 : level(grey(v));
            } else if (color == 4) {
                out = grey(sample(row, s + 1)) < 0x80 ? 0 : level(grey(sample(row, s)));
            } else {
                int g = (grey(sample(row, s)) + grey(sample(row, s + 1)) + grey(sample(row, s + 2))) / 3;
                out = (color == 6 && grey(sample(row, s + 3)) < 0x80) ? 0 : level(g);
            }
        }
    }
    return true;
}

} // namespace Game::Data
