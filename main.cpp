/*******************************************************************************************
 *
 *   raylib [textures] example - Loading Mugen SFF with 256-color palette lookup
 *
 ********************************************************************************************/
// win64:
//	@echo "Building for Win64"
//	g++ -o apps.exe main.cpp lodepng.cpp -lraylib -lgdi32 -lwinmm

#include "raylib.h"
#include "rlgl.h"
#include "lodepng.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <fstream>
#include <stdint.h>

// #ifdef _WIN32
//     #define WIN32_LEAN_AND_MEAN
//     #define NOGDI
//     #define NOUSER
//     #include <windows.h>
// #else
//     #include <dlfcn.h>
// #endif

class RGB {
public:
    uint8_t r, g, b;

    RGB() : r(0), g(0), b(0) {}
    RGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

class SffHeader {
public:
    uint8_t Ver3, Ver2, Ver1, Ver0;
    uint32_t FirstSpriteHeaderOffset;
    uint32_t FirstPaletteHeaderOffset;
    uint32_t NumberOfSprites;
    uint32_t NumberOfPalettes;

    SffHeader() : Ver3(0), Ver2(0), Ver1(0), Ver0(0),
                  FirstSpriteHeaderOffset(0), FirstPaletteHeaderOffset(0),
                  NumberOfSprites(0), NumberOfPalettes(0) {}
};

class Sprite {
public:
    uint16_t Group;
    uint16_t Number;
    uint16_t Size[2];
    int16_t Offset[2];
    int palidx;
    int rle;
    uint8_t coldepth;
    Texture2D texture;

    Sprite() : Group(0), Number(0), palidx(0), rle(0), coldepth(0) {
        Size[0] = Size[1] = 0;
        Offset[0] = Offset[1] = 0;
        texture = {0};
    }

    void CopyFrom(const Sprite& other) {
        Group = other.Group;
        Number = other.Number;
        Size[0] = other.Size[0];
        Size[1] = other.Size[1];
        Offset[0] = other.Offset[0];
        Offset[1] = other.Offset[1];
        palidx = other.palidx;
        rle = other.rle;
        coldepth = other.coldepth;
        texture = other.texture;
    }

    void Print() const {
        printf("Sprite: Group %d, Number %d, Size (%d,%d), Offset (%d,%d), palidx %d, rle %d, coldepth %d\n",
            Group, Number, Size[0], Size[1], Offset[0], Offset[1], palidx, -rle, coldepth);
    }

    bool IsPaletted() const {
        return (rle == -1 || rle == -2 || rle == -3 || rle == -4 || rle == -10);
    }

    bool IsRGBA() const {
        return (rle == -11 || rle == -12);
    }
};

class Palette {
public:
    Texture2D texture;

    Palette() : texture({0}) {}
    explicit Palette(Texture2D id) : texture(id) {}
    explicit Palette(const std::string& actFilename) : texture(GenerateFromACT(actFilename)) {}

private:
    static Texture2D GenerateFromACT(const std::string& filename) {
        Texture2D texture = {0};
        std::array<RGB, 256> pal_rgb;

        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            printf("Failed to open palette file: %s\n", filename.c_str());
            return texture;
        }

        file.read(reinterpret_cast<char*>(pal_rgb.data()), sizeof(RGB) * 256);
        if (!file) {
            printf("Failed to read palette data from file: %s\n", filename.c_str());
            return texture;
        }

        std::array<uint8_t, 256 * 4> pal_byte;
        for (int i = 0; i < 256; i++) {
            pal_byte[i * 4 + 0] = pal_rgb[255 - i].r;
            pal_byte[i * 4 + 1] = pal_rgb[255 - i].g;
            pal_byte[i * 4 + 2] = pal_rgb[255 - i].b;
            pal_byte[i * 4 + 3] = i ? 255 : 0;
        }

        texture.id = rlLoadTexture(pal_byte.data(), 256, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        texture.width = 256;
        texture.height = 1;
        texture.mipmaps = 1;
        texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return texture;
    }
};

class SffFile {
private:
    std::string filename_;
    SffHeader header_;
    std::vector<Sprite> sprites_;
    std::vector<Palette> palettes_;
    std::map<int, int> palette_usage_;
    std::map<int, int> compression_format_usage_;
    size_t numLinkedSprites_;

public:
    SffFile() : numLinkedSprites_(0) {}
    ~SffFile() { Clear(); }

    bool Load(const std::string& filename);
    void Clear();

    // Return const references to allow access without modification
    const std::vector<Sprite>& GetSprites() const { return sprites_; }
    const std::vector<Palette>& GetPalettes() const { return palettes_; }
    const SffHeader& GetHeader() const { return header_; }
    size_t GetLinkedSpriteCount() const { return numLinkedSprites_; }

    // Non-const accessors for when modification is needed
    std::vector<Sprite>& GetSprites() { return sprites_; }
    std::vector<Palette>& GetPalettes() { return palettes_; }

    const Sprite* GetSprite(size_t index) const {
        return (index < sprites_.size()) ? &sprites_[index] : nullptr;
    }

    Sprite* GetSprite(size_t index) {
        return (index < sprites_.size()) ? &sprites_[index] : nullptr;
    }

    const Palette* GetPalette(size_t index) const {
        return (index < palettes_.size()) ? &palettes_[index] : nullptr;
    }

    Palette* GetPalette(size_t index) {
        return (index < palettes_.size()) ? &palettes_[index] : nullptr;
    }

private:
    bool ReadHeader(FILE* file, uint32_t& lofs, uint32_t& tofs);
    bool ReadSpriteHeaderV1(Sprite& sprite, FILE* file, uint32_t& ofs, uint32_t& size, uint16_t& link);
    bool ReadSpriteHeaderV2(Sprite& sprite, FILE* file, uint32_t& ofs, uint32_t& size, uint32_t lofs, uint32_t tofs, uint16_t& link);

    std::unique_ptr<uint8_t[]> ReadSpriteDataV1(Sprite& sprite, FILE* file, uint64_t offset, uint32_t datasize,
                                               uint32_t nextSubheader, Sprite* prev, bool c00);
    std::unique_ptr<uint8_t[]> ReadSpriteDataV2(Sprite& sprite, FILE* file, uint64_t offset, uint32_t datasize);

    bool ReadPcxHeader(Sprite& sprite, FILE* file, uint64_t offset);

    std::unique_ptr<uint8_t[]> RlePcxDecode(const Sprite& s, const uint8_t* srcPx, size_t srcLen);
    std::unique_ptr<uint8_t[]> Rle8Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen);
    std::unique_ptr<uint8_t[]> Rle5Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen);
    std::unique_ptr<uint8_t[]> Lz5Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen);
    std::unique_ptr<uint8_t[]> PngDecode(Sprite& s, const uint8_t* srcPx, size_t srcLen);

    Texture2D GeneratePaletteTexture(const std::array<uint32_t, 256>& pal_rgba);
    Texture2D GeneratePaletteTexture(const std::array<RGB, 256>& pal_rgb);

    // Helper functions for reading integers with proper endianness
    uint16_t ReadU16LE(FILE* file) {
        uint8_t bytes[2];
        if (fread(bytes, 1, 2, file) != 2) {
            throw std::runtime_error("Error reading uint16");
        }
        return (bytes[1] << 8) | bytes[0];
    }

    int16_t ReadI16LE(FILE* file) {
        uint16_t val = ReadU16LE(file);
        return *reinterpret_cast<int16_t*>(&val);
    }

    uint32_t ReadU32LE(FILE* file) {
        uint8_t bytes[4];
        if (fread(bytes, 1, 4, file) != 4) {
            throw std::runtime_error("Error reading uint32");
        }
        return (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
    }

    int32_t ReadI32LE(FILE* file) {
        uint32_t val = ReadU32LE(file);
        return *reinterpret_cast<int32_t*>(&val);
    }
};

// Implementation of SffFile methods
bool SffFile::Load(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        printf("Error: cannot open file %s\n", filename.c_str());
        return false;
    }

    filename_ = filename;
    printf("Open file %s\n", filename.c_str());

    uint32_t lofs, tofs;
    if (!ReadHeader(file, lofs, tofs)) {
        printf("Error: reading header %s\n", filename.c_str());
        fclose(file);
        return false;
    }

    // Load palettes for SFF v2
    if (header_.Ver0 != 1) {
        std::map<std::array<int, 2>, int> uniquePals;
        palettes_.clear();
        palettes_.reserve(header_.NumberOfPalettes);

        for (uint32_t i = 0; i < header_.NumberOfPalettes; i++) {
            fseek(file, header_.FirstPaletteHeaderOffset + i * 16, SEEK_SET);

            std::array<int16_t, 3> gn;
            gn[0] = ReadU16LE(file); // group
            gn[1] = ReadU16LE(file); // number
            gn[2] = ReadU16LE(file); // colnumber

            uint16_t link = ReadU16LE(file);
            uint32_t ofs = ReadU32LE(file);
            uint32_t siz = ReadU32LE(file);

            std::array<int, 2> key = { gn[0], gn[1] };
            if (uniquePals.find(key) == uniquePals.end()) {
                fseek(file, lofs + ofs, SEEK_SET);
                std::array<uint32_t, 256> rgba;
                if (fread(rgba.data(), sizeof(uint32_t), 256, file) != 256) {
                    printf("Failed to read palette data: %s", filename.c_str());
                    fclose(file);
                    return false;
                }
                palettes_.emplace_back(GeneratePaletteTexture(rgba));
                uniquePals[key] = i;
            } else {
                printf("Palette %d(%d,%d) is not unique, using palette %d\n",
                       i, gn[0], gn[1], uniquePals[key]);
                palettes_.emplace_back(palettes_[uniquePals[key]].texture);
            }
        }
    }

    // Load sprites
    sprites_.clear();
    sprites_.resize(header_.NumberOfSprites);
    Sprite* prev = nullptr;
    numLinkedSprites_ = 0;

    long shofs = header_.FirstSpriteHeaderOffset;
    for (uint32_t i = 0; i < header_.NumberOfSprites; i++) {
        uint32_t xofs, size;
        uint16_t indexOfPrevious;

        fseek(file, shofs, SEEK_SET);
        bool success = false;

        switch (header_.Ver0) {
            case 1:
                success = ReadSpriteHeaderV1(sprites_[i], file, xofs, size, indexOfPrevious);
                break;
            case 2:
                success = ReadSpriteHeaderV2(sprites_[i], file, xofs, size, lofs, tofs, indexOfPrevious);
                break;
            default:
                printf("Unsupported SFF version: %d\n", header_.Ver0);
                fclose(file);
                return false;
        }

        if (!success) {
            fclose(file);
            return false;
        }

        if (size == 0) {
            numLinkedSprites_++;
            if (indexOfPrevious < i) {
                printf("Info: Sprite[%d] use prev Sprite[%d]\n", i, indexOfPrevious);
                sprites_[i].CopyFrom(sprites_[indexOfPrevious]);
            } else {
                printf("Warning: Sprite %d has no size\n", i);
                sprites_[i].palidx = 0;
            }
        } else {
            std::unique_ptr<uint8_t[]> data;
            bool character = true; // This should be determined properly

            switch (header_.Ver0) {
                case 1:
                    data = ReadSpriteDataV1(sprites_[i], file, shofs + 32, size, xofs, prev, character);
                    break;
                case 2:
                    data = ReadSpriteDataV2(sprites_[i], file, xofs, size);
                    break;
            }

            if (!data) {
                printf("Error reading SFFv%d sprite data for sprite %d\n", header_.Ver0, i);
                fclose(file);
                return false;
            }

            // Update usage statistics
            if (sprites_[i].IsPaletted()) {
                palette_usage_[sprites_[i].palidx]++;
            }
            compression_format_usage_[sprites_[i].rle]++;

            // Create texture
            int format = sprites_[i].IsRGBA() ?
                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 : PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;

            sprites_[i].texture.id = rlLoadTexture(data.get(), sprites_[i].Size[0],
                                                 sprites_[i].Size[1], format, 1);
            sprites_[i].texture.width = sprites_[i].Size[0];
            sprites_[i].texture.height = sprites_[i].Size[1];
            sprites_[i].texture.mipmaps = 1;
            sprites_[i].texture.format = format;

            // Update previous sprite reference
            if (sprites_[i].Group == 9000) {
                if (sprites_[i].Number == 0) {
                    prev = &sprites_[i];
                }
            } else {
                prev = &sprites_[i];
            }
        }

        // Update next header offset
        if (header_.Ver0 == 1) {
            shofs = xofs;
        } else {
            shofs += 28;
        }
    }

    // Update palette count for SFF v1
    if (header_.Ver0 == 1) {
        header_.NumberOfPalettes = palettes_.size();
    }

    fclose(file);
    return true;
}

void SffFile::Clear() {
    for (auto& palette : palettes_) {
        UnloadTexture(palette.texture);
    }
    for (auto& sprite : sprites_) {
        UnloadTexture(sprite.texture);
    }
    sprites_.clear();
    palettes_.clear();
    palette_usage_.clear();
    compression_format_usage_.clear();
    numLinkedSprites_ = 0;
}

bool SffFile::ReadHeader(FILE* file, uint32_t& lofs, uint32_t& tofs) {
    // Validate header by comparing 12 first bytes with "ElecbyteSpr\x0"
    char headerCheck[12];
    if (fread(headerCheck, 12, 1, file) != 1) {
        fprintf(stderr, "Error reading header check\n");
        return false;
    }

    if (memcmp(headerCheck, "ElecbyteSpr\0", 12) != 0) {
        fprintf(stderr, "Invalid SFF file [%s]\n", headerCheck);
        return false;
    }

    // Read versions in the header
    if (fread(&header_.Ver3, 1, 1, file) != 1) {
        fprintf(stderr, "Error reading version\n");
        return false;
    }
    if (fread(&header_.Ver2, 1, 1, file) != 1) {
        fprintf(stderr, "Error reading version\n");
        return false;
    }
    if (fread(&header_.Ver1, 1, 1, file) != 1) {
        fprintf(stderr, "Error reading version\n");
        return false;
    }
    if (fread(&header_.Ver0, 1, 1, file) != 1) {
        fprintf(stderr, "Error reading version\n");
        return false;
    }

    uint32_t dummy;
    if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) {
        fprintf(stderr, "Error reading dummy\n");
        return false;
    }

    if (header_.Ver0 == 2) {
        // Read additional header fields for version 2
        for (int i = 0; i < 4; i++) {
            if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) {
                fprintf(stderr, "Error reading dummy\n");
                return false;
            }
        }

        // Read FirstSpriteHeaderOffset
        if (fread(&header_.FirstSpriteHeaderOffset, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading FirstSpriteHeaderOffset\n");
            return false;
        }

        // Read NumberOfSprites
        if (fread(&header_.NumberOfSprites, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading NumberOfSprites\n");
            return false;
        }

        // Read FirstPaletteHeaderOffset
        if (fread(&header_.FirstPaletteHeaderOffset, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading FirstPaletteHeaderOffset\n");
            return false;
        }

        // Read NumberOfPalettes
        if (fread(&header_.NumberOfPalettes, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading NumberOfPalettes\n");
            return false;
        }

        // Read lofs
        if (fread(&lofs, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading lofs\n");
            return false;
        }

        if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading dummy\n");
            return false;
        }

        // Read tofs
        if (fread(&tofs, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading tofs\n");
            return false;
        }
    } else if (header_.Ver0 == 1) {
        // Read NumberOfSprites
        if (fread(&header_.NumberOfSprites, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading NumberOfSprites\n");
            return false;
        }

        // Read FirstSpriteHeaderOffset
        if (fread(&header_.FirstSpriteHeaderOffset, sizeof(uint32_t), 1, file) != 1) {
            fprintf(stderr, "Error reading FirstSpriteHeaderOffset\n");
            return false;
        }

        // Initialize v1-specific fields
        header_.FirstPaletteHeaderOffset = 0;
        header_.NumberOfPalettes = 0;
        lofs = 0;
        tofs = 0;
    } else {
        fprintf(stderr, "Unsupported SFF version: %d\n", header_.Ver0);
        return false;
    }

    printf("SFF Version: %d.%d.%d.%d\n", header_.Ver3, header_.Ver2, header_.Ver1, header_.Ver0);
    printf("Sprites: %u, Palettes: %u\n", header_.NumberOfSprites, header_.NumberOfPalettes);
    printf("FirstSpriteOffset: 0x%X, FirstPaletteOffset: 0x%X\n",
           header_.FirstSpriteHeaderOffset, header_.FirstPaletteHeaderOffset);

    if (header_.Ver0 == 2) {
        printf("LOFS: 0x%X, TOFS: 0x%X\n", lofs, tofs);
    }

    return true;
}

bool SffFile::ReadSpriteHeaderV1(Sprite& sprite, FILE* file, uint32_t& ofs, uint32_t& size, uint16_t& link) {
    // Read ofs and size
    ofs = ReadU32LE(file);
    size = ReadU32LE(file);

    // Read sprite offsets
    sprite.Offset[0] = ReadI16LE(file);
    sprite.Offset[1] = ReadI16LE(file);

    // Read sprite group and number
    sprite.Group = ReadU16LE(file);
    sprite.Number = ReadU16LE(file);

    // Read the link to the next sprite header
    link = ReadU16LE(file);

    // Initialize v1-specific fields
    sprite.rle = -1; // PCX format for v1
    sprite.coldepth = 8; // 8-bit for v1
    sprite.palidx = 0; // Will be set during data reading

    return true;
}

bool SffFile::ReadSpriteHeaderV2(Sprite& sprite, FILE* file, uint32_t& ofs, uint32_t& size, uint32_t lofs, uint32_t tofs, uint16_t& link) {
    // Read sprite header
    sprite.Group = ReadU16LE(file);
    sprite.Number = ReadU16LE(file);
    sprite.Size[0] = ReadU16LE(file);
    sprite.Size[1] = ReadU16LE(file);
    sprite.Offset[0] = ReadI16LE(file);
    sprite.Offset[1] = ReadI16LE(file);

    // Read the link to the next sprite header
    link = ReadU16LE(file);

    // Read format
    char format;
    if (fread(&format, sizeof(char), 1, file) != 1) {
        fprintf(stderr, "Error reading sprite format\n");
        return false;
    }
    sprite.rle = -format;

    // Read color depth
    if (fread(&sprite.coldepth, sizeof(uint8_t), 1, file) != 1) {
        fprintf(stderr, "Error reading color depth\n");
        return false;
    }

    // Read ofs and size
    ofs = ReadU32LE(file);
    size = ReadU32LE(file);

    // Read palette index
    uint16_t tmp = ReadU16LE(file);
    sprite.palidx = tmp;

    // Read location flag and adjust offset
    tmp = ReadU16LE(file);
    if ((tmp & 1) == 0) {
        ofs += lofs; // Local offset
    } else {
        ofs += tofs; // Global offset
    }

    return true;
}

std::unique_ptr<uint8_t[]> SffFile::ReadSpriteDataV1(Sprite& sprite, FILE* file, uint64_t offset, uint32_t datasize,
                                                   uint32_t nextSubheader, Sprite* prev, bool c00) {
    if (nextSubheader > offset) {
        // Ignore datasize except last
        datasize = nextSubheader - offset;
    }

    uint8_t ps;
    if (fread(&ps, sizeof(uint8_t), 1, file) != 1) {
        fprintf(stderr, "Error reading sprite ps data\n");
        return nullptr;
    }

    bool paletteSame = (ps != 0) && (prev != nullptr);

    if (!ReadPcxHeader(sprite, file, offset)) {
        fprintf(stderr, "Error reading sprite PCX header\n");
        return nullptr;
    }

    if (fseek(file, offset + 128, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to sprite data\n");
        return nullptr;
    }

    uint32_t palSize;
    if (c00 || paletteSame) {
        palSize = 0;
    } else {
        palSize = 768; // 256 colors * 3 bytes per color
    }

    if (datasize < 128 + palSize) {
        datasize = 128 + palSize;
    }

    size_t srcLen = datasize - (128 + palSize);
    auto srcPx = std::make_unique<uint8_t[]>(srcLen);
    if (!srcPx) {
        fprintf(stderr, "Error allocating memory for sprite data\n");
        return nullptr;
    }

    if (fread(srcPx.get(), srcLen, 1, file) != 1) {
        fprintf(stderr, "Error reading sprite PCX data pixel\n");
        return nullptr;
    }

    std::unique_ptr<uint8_t[]> px;

    if (paletteSame) {
        if (prev != nullptr) {
            sprite.palidx = prev->palidx;
        }
        if (sprite.palidx < 0) {
            fprintf(stderr, "Error: invalid prev palette index %d\n", (prev ? prev->palidx : -1));
            return nullptr;
        }
        px = RlePcxDecode(sprite, srcPx.get(), srcLen);
    } else {
        if (c00) {
            if (fseek(file, offset + datasize - 768, SEEK_SET) != 0) {
                fprintf(stderr, "Error seeking to palette data\n");
                return nullptr;
            }
        }

        std::array<RGB, 256> pal_rgb;
        if (fread(pal_rgb.data(), sizeof(RGB), 256, file) != 256) {
            fprintf(stderr, "Error reading palette rgb data\n");
            return nullptr;
        }

        palettes_.emplace_back(GeneratePaletteTexture(pal_rgb));
        sprite.palidx = static_cast<int>(palettes_.size() - 1);
        px = RlePcxDecode(sprite, srcPx.get(), srcLen);
    }

    return px;
}

std::unique_ptr<uint8_t[]> SffFile::ReadSpriteDataV2(Sprite& sprite, FILE* file, uint64_t offset, uint32_t datasize) {
    if (sprite.rle > 0) {
        return nullptr;
    }

    std::unique_ptr<uint8_t[]> px;

    if (sprite.rle == 0) {
        // Uncompressed data
        px = std::make_unique<uint8_t[]>(datasize);
        if (!px) {
            fprintf(stderr, "Error allocating memory for sprite data\n");
            return nullptr;
        }

        if (fseek(file, offset, SEEK_SET) != 0) {
            fprintf(stderr, "Error seeking to sprite data\n");
            return nullptr;
        }

        if (fread(px.get(), datasize, 1, file) != 1) {
            fprintf(stderr, "Error reading V2 uncompress sprite data\n");
            return nullptr;
        }
    } else {
        // Compressed data
        if (datasize < 4) {
            datasize = 4;
        }

        size_t srcLen = datasize - 4;
        auto srcPx = std::make_unique<uint8_t[]>(srcLen);
        if (!srcPx) {
            fprintf(stderr, "Error allocating memory for sprite data\n");
            return nullptr;
        }

        if (fseek(file, offset + 4, SEEK_SET) != 0) {
            fprintf(stderr, "Error seeking to compressed sprite data\n");
            return nullptr;
        }

        int rc = fread(srcPx.get(), srcLen, 1, file);
        if (rc != 1) {
            fprintf(stderr, "Error reading V2 RLE sprite data\n");
            return nullptr;
        }

        int format = -sprite.rle;
        switch (format) {
            case 2:
                px = Rle8Decode(sprite, srcPx.get(), srcLen);
                break;
            case 3:
                px = Rle5Decode(sprite, srcPx.get(), srcLen);
                break;
            case 4:
                px = Lz5Decode(sprite, srcPx.get(), srcLen);
                break;
            case 10:
            case 11:
            case 12:
                px = PngDecode(sprite, srcPx.get(), static_cast<uint32_t>(srcLen));
                break;
            default:
                fprintf(stderr, "Unknown compression format: %d\n", format);
                return nullptr;
        }
    }

    return px;
}

bool SffFile::ReadPcxHeader(Sprite& sprite, FILE* file, uint64_t offset) {
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to PCX header offset\n");
        return false;
    }

    uint16_t dummy = ReadU16LE(file);
    uint8_t encoding, bpp;

    if (fread(&encoding, sizeof(uint8_t), 1, file) != 1) {
        fprintf(stderr, "Error reading uint8_t encoding\n");
        return false;
    }
    if (fread(&bpp, sizeof(uint8_t), 1, file) != 1) {
        fprintf(stderr, "Error reading uint8_t bpp\n");
        return false;
    }

    if (bpp != 8) {
        fprintf(stderr, "Invalid PCX color depth: expected 8-bit, got %d\n", bpp);
        return false;
    }

    // Read rectangle coordinates
    uint16_t rect[4];
    for (int i = 0; i < 4; i++) {
        rect[i] = ReadU16LE(file);
    }

    if (fseek(file, offset + 66, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to bytes per line position\n");
        return false;
    }

    uint16_t bpl = ReadU16LE(file);

    sprite.Size[0] = rect[2] - rect[0] + 1;
    sprite.Size[1] = rect[3] - rect[1] + 1;
    sprite.rle = -1; // -1 for PCX

    return true;
}

std::unique_ptr<uint8_t[]> SffFile::RlePcxDecode(const Sprite& s, const uint8_t* srcPx, size_t srcLen) {
    if (srcLen == 0) {
        fprintf(stderr, "Warning: PCX data length is zero\n");
        return nullptr;
    }

    size_t dstLen = s.Size[0] * s.Size[1];
    auto dstPx = std::make_unique<uint8_t[]>(dstLen);
    if (!dstPx) {
        fprintf(stderr, "Error allocating memory for PCX decoded data dstLen=%zu srcLen=%zu (%dx%d)\n",
                dstLen, srcLen, s.Size[0], s.Size[1]);
        return nullptr;
    }

    size_t i = 0; // input pointer
    size_t j = 0; // output pointer

    while (i < srcLen && j < dstLen) {
        uint8_t byte = srcPx[i++];
        int count = 1;

        if ((byte & 0xC0) == 0xC0) { // RLE marker
            count = byte & 0x3F;
            if (i < srcLen) {
                byte = srcPx[i++];
            } else {
                fprintf(stderr, "Warning: RLE marker at end of data\n");
                break;
            }
        }

        while (count-- > 0 && j < dstLen) {
            dstPx[j++] = byte;
        }
    }

    if (j < dstLen) {
        fprintf(stderr, "Warning: decoded PCX data shorter than expected (%zu vs %zu)\n", j, dstLen);
        // Fill the remaining bytes with 0 (or a background color)
        memset(dstPx.get() + j, 0, dstLen - j);
    }

    return dstPx;
}

std::unique_ptr<uint8_t[]> SffFile::Rle8Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen) {
    if (srcLen == 0) {
        fprintf(stderr, "Warning RLE8 data length is zero\n");
        return nullptr;
    }

    size_t dstLen = s.Size[0] * s.Size[1];
    auto dstPx = std::make_unique<uint8_t[]>(dstLen);
    if (!dstPx) {
        fprintf(stderr, "Error allocating memory for RLE decoded data\n");
        return nullptr;
    }

    size_t i = 0, j = 0;
    // Decode the RLE data
    while (j < dstLen) {
        long n = 1;
        uint8_t d = srcPx[i];
        if (i < (srcLen - 1)) {
            i++;
        }
        if ((d & 0xc0) == 0x40) {
            n = d & 0x3f;
            d = srcPx[i];
            if (i < (srcLen - 1)) {
                i++;
            }
        }
        for (; n > 0; n--) {
            if (j < dstLen) {
                dstPx[j] = d;
                j++;
            }
        }
    }

    return dstPx;
}

std::unique_ptr<uint8_t[]> SffFile::Rle5Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen) {
    if (srcLen == 0) {
        fprintf(stderr, "Warning RLE5 data length is zero\n");
        return nullptr;
    }

    size_t dstLen = s.Size[0] * s.Size[1];
    auto dstPx = std::make_unique<uint8_t[]>(dstLen);
    if (!dstPx) {
        fprintf(stderr, "Error allocating memory for RLE decoded data\n");
        return nullptr;
    }

    size_t i = 0, j = 0;
    while (j < dstLen) {
        int rl = static_cast<int>(srcPx[i]);
        if (i < srcLen - 1) {
            i++;
        }
        int dl = static_cast<int>(srcPx[i] & 0x7f);
        uint8_t c = 0;
        if (srcPx[i] >> 7 != 0) {
            if (i < srcLen - 1) {
                i++;
            }
            c = srcPx[i];
        }
        if (i < srcLen - 1) {
            i++;
        }
        while (true) {
            if (j < dstLen) {
                dstPx[j] = c;
                j++;
            }
            rl--;
            if (rl < 0) {
                dl--;
                if (dl < 0) {
                    break;
                }
                c = srcPx[i] & 0x1f;
                rl = static_cast<int>(srcPx[i] >> 5);
                if (i < srcLen - 1) {
                    i++;
                }
            }
        }
    }

    return dstPx;
}

std::unique_ptr<uint8_t[]> SffFile::Lz5Decode(const Sprite& s, const uint8_t* srcPx, size_t srcLen) {
    if (srcLen == 0) {
        fprintf(stderr, "Warning LZ5 data length is zero\n");
        return nullptr;
    }

    size_t dstLen = s.Size[0] * s.Size[1];
    auto dstPx = std::make_unique<uint8_t[]>(dstLen);
    if (!dstPx) {
        fprintf(stderr, "Error allocating memory for LZ5 decoded data\n");
        return nullptr;
    }

    // Initialize output buffer to zeros
    memset(dstPx.get(), 0, dstLen);

    // Decode the LZ5 data
    size_t i = 0, j = 0;
    long n = 0;
    uint8_t ct = srcPx[i], cts = 0, rb = 0, rbc = 0;
    if (i < srcLen - 1) {
        i++;
    }

    while (j < dstLen) {
        int d = static_cast<int>(srcPx[i]);
        if (i < srcLen - 1) {
            i++;
        }

        if (ct & (1 << cts)) {
            if ((d & 0x3f) == 0) {
                d = (d << 2 | static_cast<int>(srcPx[i])) + 1;
                if (i < srcLen - 1) {
                    i++;
                }
                n = static_cast<int>(srcPx[i]) + 2;
                if (i < srcLen - 1) {
                    i++;
                }
            } else {
                rb |= static_cast<uint8_t>((d & 0xc0) >> rbc);
                rbc += 2;
                n = static_cast<int>(d & 0x3f);
                if (rbc < 8) {
                    d = static_cast<int>(srcPx[i]) + 1;
                    if (i < srcLen - 1) {
                        i++;
                    }
                } else {
                    d = static_cast<int>(rb) + 1;
                    rb = rbc = 0;
                }
            }
            for (;;) {
                if (j < dstLen && j >= d) {
                    dstPx[j] = dstPx[j - d];
                    j++;
                } else if (j < dstLen) {
                    // Handle case where we can't copy from previous data
                    dstPx[j++] = 0;
                }
                n--;
                if (n < 0) {
                    break;
                }
            }
        } else {
            if ((d & 0xe0) == 0) {
                n = static_cast<int>(srcPx[i]) + 8;
                if (i < srcLen - 1) {
                    i++;
                }
            } else {
                n = d >> 5;
                d &= 0x1f;
            }
            while (n-- > 0 && j < dstLen) {
                dstPx[j] = static_cast<uint8_t>(d);
                j++;
            }
        }
        cts++;
        if (cts >= 8) {
            ct = srcPx[i];
            cts = 0;
            if (i < srcLen - 1) {
                i++;
            }
        }
    }

    return dstPx;
}

std::unique_ptr<uint8_t[]> SffFile::PngDecode(Sprite& s, const uint8_t* data, size_t datasize) {
    lodepng::State state;
    unsigned int width = 0, height = 0;

    // Inspect PNG to get dimensions and format
    unsigned status = lodepng_inspect(&width, &height, &state, data, datasize);
    if (status) {
        fprintf(stderr, "Error inspecting PNG data: %s\n", lodepng_error_text(status));
        return nullptr;
    }

    // Configure decoding based on sprite format
    if (s.rle == -10) {  // Paletted PNG
        state.info_raw.colortype = LCT_PALETTE;
    } else {  // RGBA PNG
        state.info_raw.colortype = LCT_RGBA;
    }

    // Set bit depth
    if (state.info_png.color.bitdepth == 16) {
        state.info_raw.bitdepth = 16;
    } else {
        state.info_raw.bitdepth = 8;
    }

    // Decode PNG
    unsigned char* dstPx = nullptr;
    status = lodepng_decode(&dstPx, &width, &height, &state, data, datasize);

    if (status != 0) {
        fprintf(stderr, "Could not decode PNG image(%s)\n", lodepng_error_text(status));
        return nullptr;
    }

    // Update sprite dimensions
    s.Size[0] = static_cast<uint16_t>(width);
    s.Size[1] = static_cast<uint16_t>(height);

    // Copy to a properly allocated unique_ptr (safe approach)
    size_t pixelSize = (state.info_raw.colortype == LCT_RGBA) ? 4 : 1;
    size_t imageSize = width * height * pixelSize;

    auto result = std::make_unique<uint8_t[]>(imageSize);
    memcpy(result.get(), dstPx, imageSize);
    free(dstPx);  // Free the lodepng-allocated memory

    return result;
}

Texture2D SffFile::GeneratePaletteTexture(const std::array<uint32_t, 256>& pal_rgba) {
    Texture2D texture = {0};
    std::array<uint8_t, 256 * 4> pal_byte;

    // Convert the RGBA values into bytes (0-255 range for each channel)
    for (int i = 0; i < 256; i++) {
        pal_byte[i * 4 + 0] = (pal_rgba[i] >> 0) & 0xFF;  // R
        pal_byte[i * 4 + 1] = (pal_rgba[i] >> 8) & 0xFF;  // G
        pal_byte[i * 4 + 2] = (pal_rgba[i] >> 16) & 0xFF; // B
        pal_byte[i * 4 + 3] = (pal_rgba[i] >> 24) & 0xFF; // A
    }

    texture.id = rlLoadTexture(pal_byte.data(), 256, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    texture.width = 256;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    // CRITICAL: Set palette texture to NEAREST filtering
    rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);

    return texture;
}

Texture2D SffFile::GeneratePaletteTexture(const std::array<RGB, 256>& pal_rgb) {
    Texture2D texture = {0};
    std::array<uint8_t, 256 * 4> pal_byte;

    // Convert the RGB values into bytes (0-255 range for each channel) in reverse order
    for (int i = 0; i < 256; i++) {
        pal_byte[i * 4 + 0] = pal_rgb[255 - i].r; // R
        pal_byte[i * 4 + 1] = pal_rgb[255 - i].g; // G
        pal_byte[i * 4 + 2] = pal_rgb[255 - i].b; // B
        pal_byte[i * 4 + 3] = i ? 255 : 0;        // A (transparent for index 0)
    }

    texture.id = rlLoadTexture(pal_byte.data(), 256, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    texture.width = 256;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    // CRITICAL: Set palette texture to NEAREST filtering
    rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);

    return texture;
}

// Most efficient version - relies on palette texture having proper alpha (it is working)
// static const char *FRAGMENT_SHADER_SRC = "#version 330\n"
// "precision mediump float;\n"
// "in vec2 fragTexCoord;\n"
// "in vec4 fragColor;\n"
// "uniform sampler2D texture0;\n"
// "uniform sampler2D paletteTex;\n"
// "out vec4 finalColor;\n"
// "void main()\n"
// "{\n"
// "    vec4 texel = texture(texture0, fragTexCoord);\n"
// "    // Get palette index from grayscale texture (0-255 range)\n"
// "    float index = texel.r * 255.0;\n"
// "    \n"
// "    // Convert to normalized texture coordinate\n"
// "    float paletteCoord = (index + 0.5) / 256.0;\n"
// "    // Sample the palette texture - it should have alpha=0 for index 0\n"
// "    vec4 color = texture(paletteTex, vec2(paletteCoord, 0.5));\n"
// "    \n"
// "    // Use the palette's alpha channel directly\n"
// "    finalColor = vec4(color.rgb * fragColor.rgb, color.a * fragColor.a);\n"
// "}\n";

// Shader optimized for NEAREST filtering (now it's working)
// static const char *FRAGMENT_SHADER_SRC = "#version 330\n"
// "in vec2 fragTexCoord;\n"
// "in vec4 fragColor;\n"
// "uniform sampler2D texture0;    // Indexed sprite texture\n"
// "uniform sampler2D paletteTex;    // Palette texture\n"
// "out vec4 finalColor;\n"
// "void main()\n"
// "{\n"
// "    // Sample the indexed texture with NEAREST filtering\n"
// "    vec4 texel = texture(texture0, fragTexCoord);\n"
// "    \n"
// "    // Get palette index (0-255) - use proper rounding for NEAREST\n"
// "    int index = int(texel.r * 255.0 + 0.5);\n"
// "    \n"
// "    // Calculate EXACT texel center for palette texture with NEAREST filtering\n"
// "    float paletteCoord = (float(index) + 0.5) / 256.0;\n"
// "    \n"
// "    // Sample the palette texture with NEAREST filtering\n"
// "    vec4 color = texture(paletteTex, vec2(paletteCoord, 0.5));\n"
// "    \n"
// "    // Use palette alpha and apply vertex color\n"
// "    finalColor = vec4(color.rgb, color.a) * fragColor;\n"
// "}\n";

// Alternative: Use texelFetch for maximum precision with NEAREST filtering (now it's working)
// static const char *FRAGMENT_SHADER_SRC = "#version 330\n"
// "in vec2 fragTexCoord;\n"
// "in vec4 fragColor;\n"
// "uniform sampler2D texture0;    // Indexed sprite texture\n"
// "uniform sampler2D paletteTex;    // Palette texture\n"
// "out vec4 finalColor;\n"
// "void main()\n"
// "{\n"
// "    // Sample indexed texture\n"
// "    vec4 texel = texture(texture0, fragTexCoord);\n"
// "    \n"
// "    // Convert to integer index (0-255)\n"
// "    int index = int(texel.r * 255.0 + 0.5);\n"
// "    \n"
// "    // Use texelFetch for exact pixel access - perfect for NEAREST\n"
// "    vec4 color = texelFetch(paletteTex, ivec2(index, 0), 0);\n"
// "    \n"
// "    // Output final color\n"
// "    finalColor = vec4(color.rgb, color.a) * fragColor;\n"
// "}\n";

// Helper function to create the common shader body
std::string GetPaletteShaderBody(const std::string& textureFunc, const std::string& outputVar) {
    return
        "uniform sampler2D texture0;        \n"  // Indexed sprite texture
        "uniform sampler2D paletteTex;      \n"  // Palette texture
        "uniform vec4 colDiffuse;           \n"
        "void main()                        \n"
        "{                                  \n"
        "    vec4 texelColor = " + textureFunc + "(texture0, fragTexCoord); \n"
        "    float index = texelColor.r * 255.0; \n"
        "    float paletteCoord = (index + 0.5) / 256.0; \n"
        "    vec4 paletteColor = " + textureFunc + "(paletteTex, vec2(paletteCoord, 0.5)); \n"
        "    " + outputVar + " = vec4(paletteColor.rgb, paletteColor.a) * colDiffuse * fragColor; \n"
        "}                                  \n";
}

// Function to get the complete fragment shader based on API
std::string GetPaletteFragmentShader() {
    std::string version, precision, input, output, textureFunc, outputVar;

#if defined(GRAPHICS_API_OPENGL_21)
    version = "#version 120";
    input = "varying vec2 fragTexCoord;\nvarying vec4 fragColor;";
    textureFunc = "texture2D";
    outputVar = "gl_FragColor";
#elif defined(GRAPHICS_API_OPENGL_33)
    version = "#version 330";
    input = "in vec2 fragTexCoord;\nin vec4 fragColor;";
    output = "out vec4 finalColor;";
    textureFunc = "texture";
    outputVar = "finalColor";
#endif

#if defined(GRAPHICS_API_OPENGL_ES3)
    version = "#version 300 es";
    precision = "precision mediump float;\nprecision mediump sampler2D;";
    input = "in vec2 fragTexCoord;\nin vec4 fragColor;";
    output = "out vec4 finalColor;";
    textureFunc = "texture";
    outputVar = "finalColor";
#elif defined(GRAPHICS_API_OPENGL_ES2)
    version = "#version 100";
    precision = "precision mediump float;";
    input = "varying vec2 fragTexCoord;\nvarying vec4 fragColor;";
    textureFunc = "texture2D";
    outputVar = "gl_FragColor";
#endif

    return version + "\n" +
           precision + (precision.empty() ? "" : "\n") +
           input + "\n" +
           output + (output.empty() ? "" : "\n") +
           GetPaletteShaderBody(textureFunc, outputVar);
}

// Function to get the complete vertex shader based on API
std::string GetPaletteVertexShader() {
    std::string version, precision, attributes, varyings;

#if defined(GRAPHICS_API_OPENGL_21)
    version = "#version 120";
    attributes = "attribute vec3 vertexPosition;\nattribute vec2 vertexTexCoord;\nattribute vec4 vertexColor;";
    varyings = "varying vec2 fragTexCoord;\nvarying vec4 fragColor;";
#elif defined(GRAPHICS_API_OPENGL_33)
    version = "#version 330";
    attributes = "in vec3 vertexPosition;\nin vec2 vertexTexCoord;\nin vec4 vertexColor;";
    varyings = "out vec2 fragTexCoord;\nout vec4 fragColor;";
#endif

#if defined(GRAPHICS_API_OPENGL_ES3)
    version = "#version 300 es";
    precision = "precision mediump float;";
    attributes = "in vec3 vertexPosition;\nin vec2 vertexTexCoord;\nin vec4 vertexColor;";
    varyings = "out vec2 fragTexCoord;\nout vec4 fragColor;";
#elif defined(GRAPHICS_API_OPENGL_ES2)
    version = "#version 100";
    precision = "precision mediump float;";
    attributes = "attribute vec3 vertexPosition;\nattribute vec2 vertexTexCoord;\nattribute vec4 vertexColor;";
    varyings = "varying vec2 fragTexCoord;\nvarying vec4 fragColor;";
#endif

    return version + "\n" +
           precision + (precision.empty() ? "" : "\n") +
           attributes + "\n" +
           varyings + "\n" +
           "uniform mat4 mvp;\n"
           "void main()\n"
           "{\n"
           "    fragTexCoord = vertexTexCoord;\n"
           "    fragColor = vertexColor;\n"
           "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
           "}";
}

// Main program
int main(int argc, char *argv[]) {
    const int screenWidth = 640;
    const int screenHeight = 480;
    int sprite_no = 1;

    InitWindow(screenWidth, screenHeight, "MugenX - C++ Version");

    SffFile sff;
    if (argc == 3) {
        if (!sff.Load(argv[1])) {
            printf("Failed to load Mugen Sprite %s\n", argv[1]);
            return 1;
        }
        sprite_no = atoi(argv[2]);
    } else {
        printf("%s [sff] [no]\n", argv[0]);
        return 1;
    }

    // Use sprites and palettes - FIXED: Use non-const accessors or const references
    auto& sprites = sff.GetSprites();  // This returns non-const reference
    auto& palettes = sff.GetPalettes(); // This returns non-const reference

    if (sprites.empty() || palettes.empty()) {
        printf("No sprites or palettes loaded\n");
        return 1;
    }

    // Option 1: Use non-const references (if you need to modify them)
    // Sprite& currentSprite = sprites[6];  // Example sprite
    // Palette& currentPalette = palettes[0]; // Example palette

    // Option 2: Use const references (if you only need read access)
    const Sprite& currentSprite = sprites[sprite_no];
    const Palette& currentPalette = palettes[currentSprite.palidx];

    // Load shader
    // Shader shader = LoadShaderFromMemory(0, FRAGMENT_SHADER_SRC);
    Shader shader = LoadShaderFromMemory(GetPaletteVertexShader().c_str(),
                                        GetPaletteFragmentShader().c_str());

    int paletteTexLoc = GetShaderLocation(shader, "paletteTex");
    SetShaderValueTexture(shader, paletteTexLoc, currentPalette.texture);
    SetTargetFPS(60);

    // Force shader initialization by drawing a dummy texture
    // This ensures texture units are properly bound before using our custom shader
    Texture2D dummy_texture;
    DrawTexture(dummy_texture, 0, 0, WHITE);

    while (!WindowShouldClose()) {
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            break;
        }

        BeginDrawing();
        ClearBackground(Color{30,30,30,255});

        if (currentSprite.IsPaletted()) {
            BeginShaderMode(shader);
            DrawTexture(currentSprite.texture, 320, 240, WHITE);
            EndShaderMode();
        } else {
            DrawTexture(currentSprite.texture, 320, 240, WHITE);
        }

        DrawFPS(550, 10);
        EndDrawing();
    }

    UnloadShader(shader);
    CloseWindow();

    return 0;
}