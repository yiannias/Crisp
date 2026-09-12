// QrTables.cpp — ISO/IEC 18004'ün sabit tabloları.
//
// TABLOLAR HESAPLANMAZ, YAZILIR. Hata düzeltme sözcüğü sayıları ve blok
// sayıları standartta tablo olarak verilir ve onları üreten bir formül yok:
// sürüm 14-L'nin 4 blok 30 sözcük olması bir tasarım kararı, bir hesabın
// sonucu değil. Toplam kod sözcüğü ve hizalama konumları ise formülle de
// bulunabiliyor; testler ikisini de bilerek karşılaştırıyor ki tek bir
// yazım hatası kendini ele versin.
#include "QrInternal.h"

namespace crisp {
namespace qr {
namespace {

// Blok başına hata düzeltme sözcüğü; [düzey][sürüm-1]. Düzey sırası L, M, Q, H.
constexpr uint8_t kEcPerBlock[4][40] = {
    // L
    {7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    // M
    {10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    // Q
    {13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    // H
    {17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};

// Blok sayısı; [düzey][sürüm-1].
constexpr uint8_t kBlocks[4][40] = {
    // L
    {1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8,
     8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    // M
    {1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    // Q
    {1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    // H
    {1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};

// Sürümün toplam kod sözcüğü (Tablo 9'un ilk sütunu).
constexpr uint16_t kTotalCodewords[40] = {
    26,   44,   70,   100,  134,  172,  196,  242,  292,  346,
    404,  466,  532,  581,  655,  733,  815,  901,  991,  1085,
    1156, 1258, 1364, 1474, 1588, 1706, 1828, 1921, 2051, 2185,
    2323, 2465, 2611, 2761, 2876, 3034, 3196, 3362, 3532, 3706,
};

// Hizalama deseni merkezleri (Ek E, Tablo E.1). En fazla yedi konum; 0 = yok.
// Sürüm 1'in hiç hizalama deseni yok.
constexpr uint8_t kAlignment[40][7] = {
    {0},                            // 1
    {6, 18},                        // 2
    {6, 22},                        // 3
    {6, 26},                        // 4
    {6, 30},                        // 5
    {6, 34},                        // 6
    {6, 22, 38},                    // 7
    {6, 24, 42},                    // 8
    {6, 26, 46},                    // 9
    {6, 28, 50},                    // 10
    {6, 30, 54},                    // 11
    {6, 32, 58},                    // 12
    {6, 34, 62},                    // 13
    {6, 26, 46, 66},                // 14
    {6, 26, 48, 70},                // 15
    {6, 26, 50, 74},                // 16
    {6, 30, 54, 78},                // 17
    {6, 30, 56, 82},                // 18
    {6, 30, 58, 86},                // 19
    {6, 34, 62, 90},                // 20
    {6, 28, 50, 72, 94},            // 21
    {6, 26, 50, 74, 98},            // 22
    {6, 30, 54, 78, 102},           // 23
    {6, 28, 54, 80, 106},           // 24
    {6, 32, 58, 84, 110},           // 25
    {6, 30, 58, 86, 114},           // 26
    {6, 34, 62, 90, 118},           // 27
    {6, 26, 50, 74, 98, 122},       // 28
    {6, 30, 54, 78, 102, 126},      // 29
    {6, 26, 52, 78, 104, 130},      // 30
    {6, 30, 56, 82, 108, 134},      // 31
    {6, 34, 60, 86, 112, 138},      // 32
    {6, 30, 58, 86, 114, 142},      // 33
    {6, 34, 62, 90, 118, 146},      // 34
    {6, 30, 54, 78, 102, 126, 150}, // 35
    {6, 24, 50, 76, 102, 128, 154}, // 36
    {6, 28, 54, 80, 106, 132, 158}, // 37
    {6, 32, 58, 84, 110, 136, 162}, // 38
    {6, 26, 54, 82, 110, 138, 166}, // 39
    {6, 30, 58, 86, 114, 142, 170}, // 40
};

[[nodiscard]] bool ValidVersion(int version) noexcept {
    return version >= kMinVersion && version <= kMaxVersion;
}

}  // namespace

int TotalCodewords(int version) noexcept {
    return ValidVersion(version) ? kTotalCodewords[version - 1] : 0;
}

EcParams EcParamsFor(int version, QrEcLevel ec) noexcept {
    EcParams params;
    if (!ValidVersion(version)) {
        return params;
    }
    const int level = static_cast<int>(ec);
    params.ecPerBlock = kEcPerBlock[level][version - 1];
    params.blocks = kBlocks[level][version - 1];
    params.totalCodewords = kTotalCodewords[version - 1];
    params.dataCodewords = params.totalCodewords - params.blocks * params.ecPerBlock;
    // Bloklar toplamı eşit bölemediğinde kalan sözcükler SON bloklara birer
    // birer dağılır: önce kısa bloklar gelir, sonra bir uzun olanlar.
    params.shortBlocks = params.blocks - params.totalCodewords % params.blocks;
    params.shortDataLen = params.totalCodewords / params.blocks - params.ecPerBlock;
    return params;
}

int ByteCountBits(int version) noexcept {
    return version <= 9 ? 8 : 16;
}

int ByteCapacity(int version, QrEcLevel ec) noexcept {
    if (!ValidVersion(version)) {
        return 0;
    }
    const int bits = EcParamsFor(version, ec).dataCodewords * 8 - 4 - ByteCountBits(version);
    return bits < 0 ? 0 : bits / 8;
}

std::vector<int> AlignmentPositions(int version) {
    std::vector<int> positions;
    if (!ValidVersion(version)) {
        return positions;
    }
    for (const uint8_t position : kAlignment[version - 1]) {
        if (position == 0) {
            break;
        }
        positions.push_back(position);
    }
    return positions;
}

}  // namespace qr
}  // namespace crisp
