#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "slope.h"

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i32 = int32_t;

template <typename T>
std::vector<T> LoadBin(const std::filesystem::path &path) {
    std::basic_ifstream<T> file{path, std::ios::binary};
    return {std::istreambuf_iterator<T>{file}, {}};
}

struct membuf : std::streambuf {
    membuf(char *begin, char *end) { this->setg(begin, begin, end); }
};

struct Span {
    bool exists;
    u8 start, end;
};

struct Line {
    std::array<Span, 192> spans;
};

struct Data {
    u8 type;
    u16 minX, maxX;
    u8 minY, maxY;
    std::array<std::array<Line, 256 + 1>, 192 + 1> lines;
};

std::unique_ptr<Data> readFile(std::filesystem::path path) {
    if (!std::filesystem::is_regular_file(path)) {
        std::cout << path.string() << " does not exist or is not a file.\n";
        return nullptr;
    }

    std::cout << "Loading " << path.string() << "... ";
    std::vector<char> buffer = LoadBin<char>(path);
    membuf mbuf(buffer.data(), buffer.data() + buffer.size());
    std::istream in{&mbuf};

    auto pData = std::make_unique<Data>();
    auto &lines = pData->lines;

    in.read((char *)&pData->type, 1);
    in.read((char *)&pData->minX, sizeof(pData->minX));
    in.read((char *)&pData->maxX, sizeof(pData->maxX));
    in.read((char *)&pData->minY, sizeof(pData->minY));
    in.read((char *)&pData->maxY, sizeof(pData->maxY));

    switch (pData->type) {
    case 0: std::cout << "Top left"; break;
    case 1: std::cout << "Bottom left"; break;
    case 2: std::cout << "Top right"; break;
    case 3: std::cout << "Bottom right"; break;
    default: std::cout << "Invalid type (" << (int)pData->type << ")"; return nullptr;
    }
    std::cout << ", " << pData->minX << "x" << (int)pData->minY << " to " << pData->maxX << "x" << (int)pData->maxY;

    u8 coords[2];
    int prevX = 0;
    int prevY = 0;
    for (int y = pData->minY; y <= pData->maxY; y++) {
        for (int x = pData->minX; x <= pData->maxX; x++) {
            in.read((char *)coords, sizeof(coords));
            if (coords[0] != (u8)prevX || coords[1] != (u8)prevY) {
                std::cout << " -- Invalid file\n";
                return nullptr;
            }
            int startY = (pData->type & 2) ? prevY : 0;
            int endY = (pData->type & 2) ? 191 : prevY;
            if (startY >= 192) startY = 191;
            if (endY >= 192) endY = 191;
            for (int checkY = startY; checkY <= endY; checkY++) {
                Span &span = lines[prevY][prevX].spans[checkY];
                in.read((char *)&span.exists, 1);
                in.read((char *)&span.start, 1);
                in.read((char *)&span.end, 1);
            }
            prevX = x;
            prevY = y;
        }
    }

    int startY = (pData->type & 2) ? std::min((int)pData->minY, 191) : 0;
    int endY = (pData->type & 2) ? 191 : std::min((int)pData->maxY, 191);
    for (int y = startY; y <= endY; y++) {
        Span &span = lines[prevY][prevX].spans[y];
        in.read((char *)&span.exists, 1);
        in.read((char *)&span.start, 1);
        in.read((char *)&span.end, 1);
    }

    std::cout << " -- OK\n";

    return std::move(pData);
}

// Converts the raw screen capture taken from the NDS into a TGA file
void convertScreenCap(std::filesystem::path binPath, std::filesystem::path tgaPath) {
    std::ifstream in{binPath, std::ios::binary};
    std::ofstream out{tgaPath, std::ios::binary | std::ios::trunc};

    u8 tga[18];
    std::fill_n(tga, 18, 0);
    tga[2] = 2;                               // uncompressed truecolor
    *reinterpret_cast<u16 *>(&tga[12]) = 256; // width
    *reinterpret_cast<u16 *>(&tga[14]) = 192; // height
    tga[16] = 24;                             // bits per pixel
    tga[17] = 32;                             // image descriptor: top to bottom, left to right

    out.write((char *)tga, sizeof(tga));

    u16 clr;
    for (size_t y = 0; y < 192; y++) {
        for (size_t x = 0; x < 256; x++) {
            in.read((char *)&clr, sizeof(clr));
            u8 r5 = (clr >> 0) & 0x1F;
            u8 g5 = (clr >> 5) & 0x1F;
            u8 b5 = (clr >> 10) & 0x1F;
            u8 r8 = (r5 << 3) | (r5 >> 2);
            u8 g8 = (g5 << 3) | (g5 >> 2);
            u8 b8 = (b5 << 3) | (b5 >> 2);
            out.write((char *)&b8, sizeof(b8));
            out.write((char *)&g8, sizeof(g8));
            out.write((char *)&r8, sizeof(r8));
        }
    }
}

// Lists all unique colors present in the raw screen capture taken from the NDS
void uniqueColors(std::filesystem::path binPath) {
    std::ifstream in{binPath, std::ios::binary};
    std::unordered_set<u16> clrs;

    u16 clr;
    for (size_t y = 0; y < 192; y++) {
        for (size_t x = 0; x < 256; x++) {
            in.read((char *)&clr, sizeof(clr));
            if (clrs.insert(clr).second) {
                int r5 = (clr >> 0) & 0x1F;
                int g5 = (clr >> 5) & 0x1F;
                int b5 = (clr >> 10) & 0x1F;
                int r8 = (r5 << 3) | (r5 >> 2);
                int g8 = (g5 << 3) | (g5 >> 2);
                int b8 = (b5 << 3) | (b5 >> 2);
                std::cout << " " << std::hex << clr;
                std::cout << std::dec << "  (" << r5 << ", " << g5 << ", " << b5 << ") --> (" << r8 << ", " << g8
                          << ", " << b8 << ")\n";
            }
        }
    }
    std::cout << "\n";
}

// Writes a series of TGA files with a rendering of every scanline captured from the NDS in the given data file
void writeImages(Data &data, std::filesystem::path outDir) {
    auto &lines = data.lines;

    u8 tga[18];
    std::fill_n(tga, 18, 0);
    tga[2] = 3;                               // uncompressed greyscale
    *reinterpret_cast<u16 *>(&tga[12]) = 256; // width
    *reinterpret_cast<u16 *>(&tga[14]) = 192; // height
    tga[16] = 8;                              // bits per pixel
    tga[17] = 32;                             // image descriptor: top to bottom, left to right

    for (size_t sizeY = data.minY; sizeY <= data.maxY; sizeY++) {
        for (size_t sizeX = data.minX; sizeX <= data.maxX; sizeX++) {
            u8 pixels[192][256];
            memset(pixels, 0, 256 * 192);
            for (size_t y = 0; y < 192; y++) {
                Span &span = lines[sizeY][sizeX].spans[y];
                if (span.exists) {
                    for (size_t x = span.start; x <= span.end; x++) {
                        pixels[y][x] = 255;
                    }
                }
            }

            std::string name;
            switch (data.type) {
            case 0: name = "TL"; break;
            case 1: name = "TR"; break;
            case 2: name = "BL"; break;
            case 3: name = "BR"; break;
            }

            std::ostringstream filename;
            filename << name << "-" << sizeX << "x" << sizeY << ".tga";

            std::filesystem::create_directories(outDir);
            std::ofstream out{outDir / filename.str(), std::ios::binary | std::ios::trunc};
            out.write((char *)tga, sizeof(tga));
            out.write((char *)pixels, 256 * 192);
        }
    }
}

void testSlope(const Data &data, i32 testX, i32 testY, i32 x0, i32 y0, i32 x1, i32 y1, bool &mismatch) {
    // Always rasterize top to bottom
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    // Y0 coinciding with Y1 is equivalent to Y0 and Y1 being 1 pixel apart
    if (y0 == y1) y1++;

    // Helper function that prints the mismatch message on the first occurrence of a mismatch
    auto foundMismatch = [&] {
        if (!mismatch) {
            mismatch = true;
            std::cout << "found mismatch\n";
        }
    };

    // Create and configure the slope
    Slope slope;
    slope.Setup(x0, y0, x1, y1);

    for (i32 y = y0; y < y1; y++) {
        // Get span for the current scanline
        i32 startX = slope.FracXStart(y);
        i32 endX = slope.FracXEnd(y);
        i32 startScrX = slope.XStart(y);
        i32 endScrX = slope.XEnd(y);

        // Spans are reversed when the slope is negative
        if (slope.IsNegative()) {
            std::swap(startX, endX);
            std::swap(startScrX, endScrX);
        }

        // Skip scanlines out of view
        if (startScrX >= 256) continue;
        if (y == 192) break;

        // Compare generated spans with those captured from hardware
        const Span &span = data.lines[testY][testX].spans[y];
        if (!span.exists) {
            foundMismatch();

            // clang-format off
            std::cout << std::setw(3) << testX << "x" << std::setw(3) << testY << " Y=" << std::setw(3) << y << ": span doesn't exist\n";
            // clang-format on
        } else if (span.start != startScrX || span.end != endScrX) {
            foundMismatch();

            // clang-format off
            std::cout << std::setw(3) << testX << "x" << std::setw(3) << testY << " Y=" << std::setw(3) << y << ": ";
                    
            std::cout << std::setw(3) << startScrX << ".." << std::setw(3) << endScrX;
            std::cout << "  !=  ";
            std::cout << std::setw(3) << (u32)span.start << ".." << std::setw(3) << (u32)span.end;
                    
            std::cout << "  (" << std::showpos << (i32)(startScrX - span.start) << ".." << (i32)(endScrX - span.end) << ")" << std::noshowpos;

            std::cout << "  raw X = " << std::setw(10) << endX << "  lastX = " << std::setw(10) << startX;
            std::cout << "  masked X = " << std::setw(10) << (endX % Slope::kOne) << "  lastX = " << std::setw(10) << (startX % Slope::kOne);
            std::cout << "  inc = " << std::setw(10) << slope.DX();
            std::cout << "\n";
            // clang-format on
        }
    }
}

void testSlopes(Data &data, i32 x0, i32 y0, const char *name) {
    std::cout << "Testing " << name << " slopes... ";

    bool mismatch = false;
    for (i32 y1 = 0; y1 <= 192; y1++) {
        for (i32 x1 = 0; x1 <= 256; x1++) {
            testSlope(data, x1, y1, x0, y0, x1, y1, mismatch);
        }
    }
    if (!mismatch) {
        std::cout << "OK!\n";
    }
}

void test(Data &data) {
    switch (data.type) {
    case 0: testSlopes(data, 0, 0, "top left"); break;
    case 1: testSlopes(data, 256, 0, "top right"); break;
    case 2: testSlopes(data, 0, 192, "bottom left"); break;
    case 3: testSlopes(data, 256, 192, "bottom right"); break;
    }
}

int main() {
    // convertScreenCap("data/screencap.bin", "data/screencap.tga");
    // uniqueColors("data/screencap.bin");

    auto dataTL = readFile("data/TL.bin");
    auto dataTR = readFile("data/TR.bin");
    auto dataBL = readFile("data/BL.bin");
    auto dataBR = readFile("data/BR.bin");

    if (dataTL) test(*dataTL);
    if (dataTR) test(*dataTR);
    if (dataBL) test(*dataBL);
    if (dataBR) test(*dataBR);

    // if (dataTL) writeImages(*dataTL, "C:/temp/TL");
    // if (dataTR) writeImages(*dataTR, "C:/temp/TR");
    // if (dataBL) writeImages(*dataBL, "C:/temp/BL");
    // if (dataBR) writeImages(*dataBR, "C:/temp/BR");

    return 0;
}
