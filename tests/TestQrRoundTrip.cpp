// TestQrRoundTrip.cpp — Matrisin bağımsız bir çözücüyle geri okunması.
//
// TARAYICI YOK, O YÜZDEN ÇÖZÜCÜ YARISI BURADA. Kodun okunup okunmadığını
// söyleyecek bir telefon test koşucusunda yok; onun yerine matris standarda
// göre BAĞIMSIZ bir yoldan geri okunuyor: işlev modülleri, zikzak, maske ve
// biçim bilgisi burada kodlayıcıdan habersiz ikinci kez yazıldı. Kodlayıcının
// kod sözcükleriyle geri okunanlar bire bir tutuyorsa yerleşim doğru; kod
// sözcükleri de bilinen Reed-Solomon vektörleriyle tutuyorsa içerik doğru.
#include "TestFramework.h"

#include "QrCode.h"
#include "QrInternal.h"

#include <string>
#include <vector>

using namespace crisp;

namespace {

// --- Bağımsız çözücü yardımcıları -------------------------------------------

// İşlev modülü haritası, standardın tarifinden: bulucu+ayırıcı+biçim
// alanları (üç köşede 9x9), zamanlama çizgileri, hizalama desenleri, sürüm
// bilgisi blokları, karanlık modül.
[[nodiscard]] std::vector<uint8_t> FunctionMap(int version) {
    const int size = 17 + 4 * version;
    std::vector<uint8_t> map(static_cast<size_t>(size) * static_cast<size_t>(size), 0);
    auto mark = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < size && y < size) {
            map[static_cast<size_t>(y) * static_cast<size_t>(size) +
                static_cast<size_t>(x)] = 1;
        }
    };
    // Sol üst 9x9; sağ üst 8 sütun x 9 satır (bulucu, ayırıcı, 8. satırdaki
    // biçim kopyası); sol alt 9 sütun x 8 satır (8. sütundaki kopya ve
    // karanlık modül).
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            mark(i, j);
            if (i < 8) {
                mark(size - 1 - i, j);
            }
            if (j < 8) {
                mark(i, size - 1 - j);
            }
        }
    }
    for (int i = 0; i < size; ++i) {
        mark(6, i);
        mark(i, 6);
    }
    const std::vector<int> positions = qr::AlignmentPositions(version);
    const size_t count = positions.size();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            if ((i == 0 && j == 0) || (i == 0 && j == count - 1) ||
                (i == count - 1 && j == 0)) {
                continue;
            }
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    mark(positions[j] + dx, positions[i] + dy);
                }
            }
        }
    }
    if (version >= 7) {
        for (int a = 0; a < 6; ++a) {
            for (int b = size - 11; b < size - 8; ++b) {
                mark(b, a);
                mark(a, b);
            }
        }
    }
    return map;
}

// Maske formülleri, standardın tablosundan (i = satır, j = sütun).
[[nodiscard]] bool MaskFormula(int mask, int i, int j) {
    switch (mask) {
        case 0: return (i + j) % 2 == 0;
        case 1: return i % 2 == 0;
        case 2: return j % 3 == 0;
        case 3: return (i + j) % 3 == 0;
        case 4: return (i / 2 + j / 3) % 2 == 0;
        case 5: return (i * j) % 2 + (i * j) % 3 == 0;
        case 6: return ((i * j) % 2 + (i * j) % 3) % 2 == 0;
        default: return ((i + j) % 2 + (i * j) % 3) % 2 == 0;
    }
}

// Zikzak yürüyüşü: matristen veri bitlerini toplar, maskeyi kaldırır.
[[nodiscard]] std::vector<uint8_t> ReadCodewords(const QrCode& code, int mask,
                                                 const std::vector<uint8_t>& function,
                                                 size_t count) {
    std::vector<uint8_t> out(count, 0);
    size_t bit = 0;
    const int size = code.size;
    for (int right = size - 1; right >= 1 && bit < count * 8; right -= 2) {
        if (right == 6) {
            right = 5;
        }
        const bool upward = ((right + 1) & 2) == 0;
        for (int step = 0; step < size; ++step) {
            const int y = upward ? size - 1 - step : step;
            for (int j = 0; j < 2 && bit < count * 8; ++j) {
                const int x = right - j;
                if (function[static_cast<size_t>(y) * static_cast<size_t>(size) +
                             static_cast<size_t>(x)] != 0) {
                    continue;
                }
                bool dark = code.Module(x, y);
                if (MaskFormula(mask, y, x)) {
                    dark = !dark;
                }
                if (dark) {
                    out[bit / 8] = static_cast<uint8_t>(out[bit / 8] | (0x80u >> (bit % 8)));
                }
                ++bit;
            }
        }
    }
    return out;
}

// Biçim bilgisini iki kopyasından da okur; -1 = tutmuyor.
[[nodiscard]] int ReadFormat(const QrCode& code, uint32_t& bitsOut) {
    const int size = code.size;
    uint32_t first = 0;
    uint32_t second = 0;
    for (int i = 0; i <= 5; ++i) {
        first |= (code.Module(8, i) ? 1u : 0u) << i;
    }
    first |= (code.Module(8, 7) ? 1u : 0u) << 6;
    first |= (code.Module(8, 8) ? 1u : 0u) << 7;
    first |= (code.Module(7, 8) ? 1u : 0u) << 8;
    for (int i = 9; i < 15; ++i) {
        first |= (code.Module(14 - i, 8) ? 1u : 0u) << i;
    }
    for (int i = 0; i < 8; ++i) {
        second |= (code.Module(size - 1 - i, 8) ? 1u : 0u) << i;
    }
    for (int i = 8; i < 15; ++i) {
        second |= (code.Module(8, size - 15 + i) ? 1u : 0u) << i;
    }
    bitsOut = first;
    return first == second ? 0 : -1;
}

// BCH(15,5) kalanı; geçerli bir biçim sözcüğünde sıfır.
[[nodiscard]] uint32_t Bch15Remainder(uint32_t word) {
    uint32_t value = word;
    for (int i = 14; i >= 10; --i) {
        if ((value >> i) & 1u) {
            value ^= 0x537u << (i - 10);
        }
    }
    return value;
}

// BCH(18,6) kalanı; geçerli bir sürüm sözcüğünde sıfır.
[[nodiscard]] uint32_t Bch18Remainder(uint32_t word) {
    uint32_t value = word;
    for (int i = 17; i >= 12; --i) {
        if ((value >> i) & 1u) {
            value ^= 0x1F25u << (i - 12);
        }
    }
    return value;
}

[[nodiscard]] bool FinderAt(const QrCode& code, int left, int top) {
    for (int dy = 0; dy < 7; ++dy) {
        for (int dx = 0; dx < 7; ++dx) {
            const bool edge = dx == 0 || dy == 0 || dx == 6 || dy == 6;
            const bool core = dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4;
            if (code.Module(left + dx, top + dy) != (edge || core)) {
                return false;
            }
        }
    }
    return true;
}

// Kodlar ve tam gidiş-dönüşü yapar: yerleşim, biçim, bloklar, RS.
void RoundTrip(const std::string& text, QrEcLevel ec, int expectVersion) {
    QrCode code;
    qr::EncodeInfo info;
    CHECK(qr::EncodeDetailed(text, code, ec, info));
    CHECK_EQ(code.version, expectVersion);
    CHECK_EQ(code.size, 17 + 4 * expectVersion);
    CHECK(info.mask >= 0 && info.mask < 8);

    // İşlev haritası kodlayıcınınkiyle aynı olmalı.
    const std::vector<uint8_t> function = FunctionMap(code.version);
    CHECK(function == info.function);

    // Biçim bilgisi: iki kopya aynı, BCH geçerli, maske ve düzey tutuyor.
    uint32_t format = 0;
    CHECK_EQ(ReadFormat(code, format), 0);
    CHECK_EQ(Bch15Remainder(format ^ 0x5412u), 0);
    CHECK_EQ(static_cast<int>((format ^ 0x5412u) >> 10) & 7, info.mask);
    static constexpr int kLevelBits[4] = {1, 0, 3, 2};
    CHECK_EQ(static_cast<int>((format ^ 0x5412u) >> 13) & 3,
             kLevelBits[static_cast<int>(ec)]);

    // Yerleşim: matristen okunan kod sözcükleri kodlayıcının yazdıklarıyla
    // bire bir.
    const qr::EcParams params = qr::EcParamsFor(code.version, ec);
    CHECK_EQ(info.codewords.size(), params.totalCodewords);
    const std::vector<uint8_t> read =
        ReadCodewords(code, info.mask, function, info.codewords.size());
    CHECK(read == info.codewords);

    // Serpiştirmeyi geri al: bloklar, veri ve hata düzeltme.
    const size_t blocks = static_cast<size_t>(params.blocks);
    std::vector<std::vector<uint8_t>> data(blocks);
    std::vector<std::vector<uint8_t>> ecw(blocks);
    size_t at = 0;
    for (size_t i = 0; i <= static_cast<size_t>(params.shortDataLen); ++i) {
        for (size_t b = 0; b < blocks; ++b) {
            const size_t length = static_cast<size_t>(params.shortDataLen) +
                                  (b < static_cast<size_t>(params.shortBlocks) ? 0u : 1u);
            if (i < length) {
                data[b].push_back(read[at++]);
            }
        }
    }
    for (int i = 0; i < params.ecPerBlock; ++i) {
        for (size_t b = 0; b < blocks; ++b) {
            ecw[b].push_back(read[at++]);
        }
    }
    CHECK_EQ(at, read.size());

    // Her bloğun hata düzeltmesi verisinden yeniden hesaplanınca tutmalı.
    const std::vector<uint8_t> divisor = qr::ReedSolomonDivisor(params.ecPerBlock);
    std::vector<uint8_t> payload;
    for (size_t b = 0; b < blocks; ++b) {
        CHECK(qr::ReedSolomonRemainder(data[b], divisor) == ecw[b]);
        payload.insert(payload.end(), data[b].begin(), data[b].end());
    }

    // Veri bit akışı: kip 0100, sayaç, baytlar.
    CHECK_EQ(payload.size(), params.dataCodewords);
    const int countBits = qr::ByteCountBits(code.version);
    std::vector<uint8_t> expectBits;
    auto append = [&](uint32_t value, int bits) {
        for (int i = bits - 1; i >= 0; --i) {
            expectBits.push_back(static_cast<uint8_t>((value >> i) & 1u));
        }
    };
    append(0x4, 4);
    append(static_cast<uint32_t>(text.size()), countBits);
    for (const char ch : text) {
        append(static_cast<uint8_t>(ch), 8);
    }
    bool prefixOk = expectBits.size() <= payload.size() * 8;
    for (size_t i = 0; prefixOk && i < expectBits.size(); ++i) {
        prefixOk = ((payload[i / 8] >> (7 - i % 8)) & 1u) == expectBits[i];
    }
    CHECK(prefixOk);

    // Sonlandırıcıdan sonra dolgu 0xEC/0x11 dönüşümlü olmalı.
    const size_t headerBytes = (expectBits.size() + 4 + 7) / 8;
    for (size_t i = headerBytes; i < payload.size(); ++i) {
        CHECK_EQ(payload[i], (i - headerBytes) % 2 == 0 ? 0xEC : 0x11);
    }

    // Bulucular, zamanlama, karanlık modül.
    CHECK(FinderAt(code, 0, 0));
    CHECK(FinderAt(code, code.size - 7, 0));
    CHECK(FinderAt(code, 0, code.size - 7));
    for (int i = 8; i < code.size - 8; ++i) {
        CHECK_EQ(code.Module(i, 6) ? 1 : 0, i % 2 == 0 ? 1 : 0);
        CHECK_EQ(code.Module(6, i) ? 1 : 0, i % 2 == 0 ? 1 : 0);
    }
    CHECK(code.Module(8, code.size - 8));

    // Seçilen maske diğerlerinden daha kötü olmamalı.
    const int chosen = qr::PenaltyScore(code);
    for (int mask = 0; mask < 8; ++mask) {
        QrCode alt;
        alt.version = code.version;
        alt.size = code.size;
        alt.modules = code.modules;
        qr::ApplyMask(alt, function, info.mask);
        qr::ApplyMask(alt, function, mask);
        qr::DrawFormatBits(alt, ec, mask);
        CHECK(chosen <= qr::PenaltyScore(alt));
    }
}

}  // namespace

// --- Biçim ve sürüm bilgisi ---------------------------------------------------

CRISP_TEST(QrCode, Bicim_bilgisi) {
    CHECK_EQ(qr::FormatBits(QrEcLevel::M, 0), 0x5412);
    CHECK_EQ(qr::FormatBits(QrEcLevel::L, 0), 0x77C4);
    CHECK_EQ(qr::FormatBits(QrEcLevel::H, 0), 0x1689);
    CHECK_EQ(qr::FormatBits(QrEcLevel::Q, 0), 0x355F);
    CHECK_EQ(qr::FormatBits(QrEcLevel::L, 7), 0x6976);
    CHECK_EQ(qr::FormatBits(QrEcLevel::M, 7), 0x4AA0);
    for (int level = 0; level < 4; ++level) {
        for (int mask = 0; mask < 8; ++mask) {
            const uint32_t bits = qr::FormatBits(static_cast<QrEcLevel>(level), mask);
            CHECK(bits < (1u << 15));
            CHECK_EQ(Bch15Remainder(bits ^ 0x5412u), 0);
        }
    }
}

CRISP_TEST(QrCode, Surum_bilgisi) {
    CHECK_EQ(qr::VersionBits(7), 0x07C94);
    CHECK_EQ(qr::VersionBits(8), 0x085BC);
    CHECK_EQ(qr::VersionBits(9), 0x09A99);
    CHECK_EQ(qr::VersionBits(10), 0x0A4D3);
    CHECK_EQ(qr::VersionBits(40), 0x28C69);
    for (int version = 7; version <= 40; ++version) {
        const uint32_t bits = qr::VersionBits(version);
        CHECK_EQ(bits >> 12, version);
        CHECK_EQ(Bch18Remainder(bits), 0);
    }
}

// --- Gidiş-dönüş ---------------------------------------------------------------

CRISP_TEST(QrCode, Gidis_donus_surum_1) {
    RoundTrip("HELLO WORLD", QrEcLevel::M, 1);
    RoundTrip("https://x.io/a", QrEcLevel::M, 1);
    RoundTrip("", QrEcLevel::M, 1);
    RoundTrip("abcdefg", QrEcLevel::H, 1);
}

CRISP_TEST(QrCode, Gidis_donus_surum_3_ve_4) {
    RoundTrip("https://i.imgur.com/abcdefghijk.png?x=1", QrEcLevel::M, 3);
    RoundTrip("https://catbox.moe/c/abcd", QrEcLevel::H, 4);   // 4 blok
}

CRISP_TEST(QrCode, Gidis_donus_surum_7) {
    // Sürüm bilgisi bloklarını ve çok bloklu serpiştirmeyi kullanır.
    const std::string text(120, 'q');
    RoundTrip(text, QrEcLevel::M, 7);
    RoundTrip("https://example.org/" + std::string(110, 'z') + "?k=v&x=y#frag",
              QrEcLevel::L, 7);
}

CRISP_TEST(QrCode, Gidis_donus_surum_10) {
    // 16 bitlik sayaç, kısa ve uzun bloklar (5 blok, 216 veri).
    std::string text;
    for (int i = 0; i < 210; ++i) {
        text.push_back(static_cast<char>('A' + i % 26));
    }
    RoundTrip(text, QrEcLevel::M, 10);
}

CRISP_TEST(QrCode, Gidis_donus_buyuk_surumler) {
    RoundTrip(std::string(1000, 'w'), QrEcLevel::Q, 31);
    RoundTrip(std::string(2900, 'w'), QrEcLevel::L, 40);
}

CRISP_TEST(QrCode, Utf8_baytlar_oldugu_gibi) {
    const std::string text = "https://ex.io/\xC3\xA7\xC4\xB1";   // 18 bayt
    RoundTrip(text, QrEcLevel::M, 2);
}
