// QrEncodeData.cpp — Metinden kod sözcüklerine: bit akışı, Reed-Solomon,
// serpiştirme.
//
// Buradaki hiçbir şey matrise dokunmaz; çıktı bir bayt dizisi ve o dizi
// QrCode.cpp'de zikzakla yerleştirilir. Ayrım, testlerin matristen geri
// okudukları bitleri buradaki diziyle bire bir karşılaştırabilmesi için.
#include "QrInternal.h"

#include <cstddef>
#include <string>
#include <utility>

namespace crisp {
namespace qr {
namespace {

// Bit bit yazan küçük bir tampon. Kod sözcükleri bayt sınırına hizalı
// olmadığı için (kip 4 bit, sayaç 8 ya da 16, sonlandırıcı 0-4) bayt bayt
// yazmak olmuyor.
class BitBuffer {
public:
    void Append(uint32_t value, int bits) {
        for (int i = bits - 1; i >= 0; --i) {
            m_bits.push_back(static_cast<uint8_t>((value >> i) & 1u));
        }
    }
    [[nodiscard]] size_t Size() const noexcept { return m_bits.size(); }

    // Baytlara paketler; son bayt eksikse sıfırla tamamlanır.
    [[nodiscard]] std::vector<uint8_t> Bytes() const {
        std::vector<uint8_t> bytes((m_bits.size() + 7) / 8, 0);
        for (size_t i = 0; i < m_bits.size(); ++i) {
            bytes[i / 8] = static_cast<uint8_t>(bytes[i / 8] | (m_bits[i] << (7 - i % 8)));
        }
        return bytes;
    }

private:
    std::vector<uint8_t> m_bits;
};

}  // namespace

uint8_t GfMultiply(uint8_t a, uint8_t b) noexcept {
    // Rus köylü çarpımı; tablo yok. Sürüm 40'ta bile birkaç yüz bin çarpım
    // var ve bir kez, arka planda koşuyor: tablo hazırlamak kazandırmaz.
    unsigned result = 0;
    unsigned x = a;
    for (int i = 7; i >= 0; --i) {
        result = (result << 1) ^ ((result >> 7) * 0x11Du);
        if (((b >> i) & 1u) != 0) {
            result ^= x;
        }
    }
    return static_cast<uint8_t>(result & 0xFFu);
}

std::vector<uint8_t> ReedSolomonDivisor(int degree) {
    std::vector<uint8_t> result(static_cast<size_t>(degree), 0);
    if (degree <= 0) {
        return result;
    }
    // Başlangıç polinomu 1 (x^0). Her adımda (x - α^i) ile çarpılır; sıra
    // en yüksek dereceden en düşüğe, baş katsayı 1 olduğu için saklanmıyor.
    result.back() = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (size_t j = 0; j < result.size(); ++j) {
            result[j] = GfMultiply(result[j], root);
            if (j + 1 < result.size()) {
                result[j] ^= result[j + 1];
            }
        }
        root = GfMultiply(root, 0x02);
    }
    return result;
}

std::vector<uint8_t> ReedSolomonRemainder(const std::vector<uint8_t>& data,
                                          const std::vector<uint8_t>& divisor) {
    std::vector<uint8_t> result(divisor.size(), 0);
    for (const uint8_t byte : data) {
        const uint8_t factor = static_cast<uint8_t>(byte ^ result.front());
        result.erase(result.begin());
        result.push_back(0);
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] ^= GfMultiply(divisor[i], factor);
        }
    }
    return result;
}

std::vector<uint8_t> BuildDataCodewords(const std::string& text, int version,
                                        QrEcLevel ec) {
    const EcParams params = EcParamsFor(version, ec);
    if (params.dataCodewords == 0 ||
        text.size() > static_cast<size_t>(ByteCapacity(version, ec))) {
        return {};
    }
    const size_t capacityBits = static_cast<size_t>(params.dataCodewords) * 8;

    BitBuffer bits;
    bits.Append(0x4, 4);                                                 // bayt kipi
    bits.Append(static_cast<uint32_t>(text.size()), ByteCountBits(version));
    for (const char ch : text) {
        bits.Append(static_cast<uint8_t>(ch), 8);
    }

    // Sonlandırıcı: en fazla 4 sıfır, yer kalmadıysa daha az. Ardından bayt
    // sınırına kadar sıfır, sonra dönüşümlü 0xEC/0x11 dolgusu — standart
    // bu iki deseni bilerek seçmiş: modül dağılımını dengeliyorlar.
    size_t terminator = capacityBits - bits.Size();
    if (terminator > 4) {
        terminator = 4;
    }
    bits.Append(0, static_cast<int>(terminator));
    if (bits.Size() % 8 != 0) {
        bits.Append(0, static_cast<int>(8 - bits.Size() % 8));
    }
    for (uint8_t pad = 0xEC; bits.Size() < capacityBits; pad ^= 0xEC ^ 0x11) {
        bits.Append(pad, 8);
    }
    return bits.Bytes();
}

std::vector<uint8_t> InterleaveBlocks(const std::vector<uint8_t>& data,
                                      const EcParams& params) {
    std::vector<uint8_t> result;
    if (params.blocks <= 0 ||
        data.size() != static_cast<size_t>(params.dataCodewords)) {
        return result;
    }

    // Bloklara böl ve her birine hata düzeltme ekle. Kısa bloklar önce
    // gelir; uzun bloklar bir fazla veri sözcüğü taşır.
    const std::vector<uint8_t> divisor = ReedSolomonDivisor(params.ecPerBlock);
    std::vector<std::vector<uint8_t>> dataBlocks;
    std::vector<std::vector<uint8_t>> ecBlocks;
    size_t offset = 0;
    for (int b = 0; b < params.blocks; ++b) {
        const size_t length = static_cast<size_t>(params.shortDataLen) +
                              (b < params.shortBlocks ? 0u : 1u);
        std::vector<uint8_t> block(data.begin() + static_cast<ptrdiff_t>(offset),
                                   data.begin() + static_cast<ptrdiff_t>(offset + length));
        offset += length;
        ecBlocks.push_back(ReedSolomonRemainder(block, divisor));
        dataBlocks.push_back(std::move(block));
    }

    // SERPİŞTİRME: önce tüm blokların 1. veri sözcüğü, sonra 2. ... Kısa
    // blokların olmayan son sözcüğü atlanır. Ardından aynı düzenle hata
    // düzeltme sözcükleri. Bir hasar böylece tek bloğa değil hepsine yayılır.
    result.reserve(static_cast<size_t>(params.totalCodewords));
    const size_t longDataLen = static_cast<size_t>(params.shortDataLen) + 1;
    for (size_t i = 0; i < longDataLen; ++i) {
        for (const auto& block : dataBlocks) {
            if (i < block.size()) {
                result.push_back(block[i]);
            }
        }
    }
    for (size_t i = 0; i < static_cast<size_t>(params.ecPerBlock); ++i) {
        for (const auto& block : ecBlocks) {
            result.push_back(block[i]);
        }
    }
    return result;
}

}  // namespace qr
}  // namespace crisp
