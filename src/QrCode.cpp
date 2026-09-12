// QrCode.cpp — Matris: işlev desenleri, biçim/sürüm bilgisi, zikzak
// yerleşim, maske seçimi ve görüntüye çizim.
//
// KOORDİNAT SÖZLEŞMESİ: her yerde (x, y) = (sütun, satır), sol üst (0, 0).
// Standart bazı şekillerde satır-sütun yazıyor; burada tek sözleşme var ve
// bit yerleştirme ile biçim bilgisi kopyaları da ona göre yazıldı. Bir yerde
// eksenleri karıştırmak, transpoze — dolayısıyla okunmayan — bir kod verir.
#include "QrInternal.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace crisp {
namespace qr {
namespace {

// Bir modülü yazar ve işlev modülü olarak işaretler.
class Matrix {
public:
    Matrix(QrCode& code, std::vector<uint8_t>& function) noexcept
        : m_code(code), m_function(function) {}

    void SetFunction(int x, int y, bool dark) noexcept {
        if (x < 0 || y < 0 || x >= m_code.size || y >= m_code.size) {
            return;
        }
        const size_t at = static_cast<size_t>(y) * static_cast<size_t>(m_code.size) +
                          static_cast<size_t>(x);
        m_code.modules[at] = dark ? 1 : 0;
        m_function[at] = 1;
    }

private:
    QrCode& m_code;
    std::vector<uint8_t>& m_function;
};

// Bulucu desen: 7x7 (3x3 koyu çekirdek, açık halka, koyu çerçeve) ve
// çevresinde 1 modül açık ayırıcı. Merkez (cx, cy); kod dışına taşan
// ayırıcı parçaları SetFunction'ın sınır kontrolüyle düşer.
void DrawFinder(Matrix& matrix, int cx, int cy) {
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            matrix.SetFunction(cx + dx, cy + dy, distance != 2 && distance != 4);
        }
    }
}

// Hizalama deseni: 5x5, koyu çerçeve, açık halka, koyu merkez.
void DrawAlignment(Matrix& matrix, int cx, int cy) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            matrix.SetFunction(cx + dx, cy + dy, distance != 1);
        }
    }
}

// Sürüm bilgisini iki kopyasına yazar: sağ üst bulucunun solundaki 6x3 blok
// ve sol alt bulucunun üstündeki 3x6 blok (birbirinin transpozu).
void DrawVersionBits(Matrix& matrix, int version, int size) {
    if (version < 7) {
        return;
    }
    const uint32_t bits = VersionBits(version);
    for (int i = 0; i < 18; ++i) {
        const bool bit = ((bits >> i) & 1u) != 0;
        const int a = size - 11 + i % 3;
        const int b = i / 3;
        matrix.SetFunction(a, b, bit);
        matrix.SetFunction(b, a, bit);
    }
}

// Veri dışındaki her şeyi çizer ve işlev haritasını doldurur. Biçim bilgisi
// alanları da işaretlenir ki zikzak onların üstünden atlasın; asıl bitler
// maske seçildikten sonra DrawFormatBits ile yazılır.
void DrawFunctionPatterns(QrCode& code, std::vector<uint8_t>& function) {
    Matrix matrix(code, function);
    const int size = code.size;

    // Zamanlama desenleri: 6. satır ve 6. sütun, koyu ile başlayıp dönüşümlü.
    for (int i = 0; i < size; ++i) {
        matrix.SetFunction(6, i, i % 2 == 0);
        matrix.SetFunction(i, 6, i % 2 == 0);
    }

    DrawFinder(matrix, 3, 3);
    DrawFinder(matrix, size - 4, 3);
    DrawFinder(matrix, 3, size - 4);

    // Hizalama: konumların kartezyen çarpımı, bulucularla çakışan üç köşe hariç.
    const std::vector<int> positions = AlignmentPositions(code.version);
    const size_t count = positions.size();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            const bool topLeft = i == 0 && j == 0;
            const bool topRight = i == 0 && j == count - 1;
            const bool bottomLeft = i == count - 1 && j == 0;
            if (!topLeft && !topRight && !bottomLeft) {
                DrawAlignment(matrix, positions[j], positions[i]);
            }
        }
    }

    // Biçim alanlarını ayır (geçici olarak açık); karanlık modül dahil.
    DrawFormatBits(code, QrEcLevel::L, 0);
    for (int i = 0; i < 9; ++i) {
        matrix.SetFunction(8, i, code.Module(8, i));
        matrix.SetFunction(i, 8, code.Module(i, 8));
    }
    for (int i = 0; i < 8; ++i) {
        matrix.SetFunction(size - 1 - i, 8, code.Module(size - 1 - i, 8));
        matrix.SetFunction(8, size - 1 - i, code.Module(8, size - 1 - i));
    }

    DrawVersionBits(matrix, code.version, size);
}

// Kod sözcüklerini zikzakla yerleştirir: sağdan sola ikişer sütunluk
// şeritler, şeritler dönüşümlü olarak yukarı ve aşağı; her adımda sağdaki
// sütun önce. 6. sütun (dikey zamanlama) tümüyle atlanır. Kalan bitler
// (sürüme göre 0-7) açık bırakılır.
void DrawCodewords(QrCode& code, const std::vector<uint8_t>& function,
                   const std::vector<uint8_t>& data) {
    const int size = code.size;
    const size_t totalBits = data.size() * 8;
    size_t i = 0;
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == 6) {
            right = 5;
        }
        for (int vert = 0; vert < size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? size - 1 - vert : vert;
                const size_t at = static_cast<size_t>(y) * static_cast<size_t>(size) +
                                  static_cast<size_t>(x);
                if (function[at] == 0 && i < totalBits) {
                    code.modules[at] =
                        static_cast<uint8_t>((data[i >> 3] >> (7 - (i & 7))) & 1u);
                    ++i;
                }
            }
        }
    }
}

}  // namespace

uint32_t FormatBits(QrEcLevel ec, int mask) noexcept {
    // Düzey bitleri standartta sezgisel sırada değil: L=01, M=00, Q=11, H=10.
    static constexpr uint32_t kLevelBits[4] = {1, 0, 3, 2};
    const uint32_t data = (kLevelBits[static_cast<int>(ec)] << 3) | static_cast<uint32_t>(mask & 7);
    uint32_t rem = data;
    for (int i = 0; i < 10; ++i) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537u);
    }
    return ((data << 10) | rem) ^ 0x5412u;
}

uint32_t VersionBits(int version) noexcept {
    uint32_t rem = static_cast<uint32_t>(version);
    for (int i = 0; i < 12; ++i) {
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25u);
    }
    return (static_cast<uint32_t>(version) << 12) | rem;
}

void DrawFormatBits(QrCode& code, QrEcLevel ec, int mask) {
    const uint32_t bits = FormatBits(ec, mask);
    const int size = code.size;
    auto set = [&code, size](int x, int y, bool dark) {
        code.modules[static_cast<size_t>(y) * static_cast<size_t>(size) +
                     static_cast<size_t>(x)] = dark ? 1 : 0;
    };
    auto bit = [bits](int i) { return ((bits >> i) & 1u) != 0; };

    // İlk kopya, sol üst bulucunun çevresinde: 0-5 sütun 8'de aşağı, 6-7
    // zamanlama satırının altında, 8 satır 8'de, 9-14 satır 8'de sola doğru.
    for (int i = 0; i <= 5; ++i) {
        set(8, i, bit(i));
    }
    set(8, 7, bit(6));
    set(8, 8, bit(7));
    set(7, 8, bit(8));
    for (int i = 9; i < 15; ++i) {
        set(14 - i, 8, bit(i));
    }

    // İkinci kopya: 0-7 satır 8'de sağdan sola, 8-14 sütun 8'de alttan
    // yukarı; ve hiçbir maskeye bağlı olmayan karanlık modül.
    for (int i = 0; i < 8; ++i) {
        set(size - 1 - i, 8, bit(i));
    }
    for (int i = 8; i < 15; ++i) {
        set(8, size - 15 + i, bit(i));
    }
    set(8, size - 8, true);
}

bool EncodeDetailed(const std::string& text, QrCode& out, QrEcLevel ec,
                    EncodeInfo& info) {
    int version = 0;
    for (int candidate = kMinVersion; candidate <= kMaxVersion; ++candidate) {
        if (text.size() <= static_cast<size_t>(ByteCapacity(candidate, ec))) {
            version = candidate;
            break;
        }
    }
    if (version == 0) {
        return false;
    }

    const EcParams params = EcParamsFor(version, ec);
    const std::vector<uint8_t> data = BuildDataCodewords(text, version, ec);
    if (data.empty()) {
        return false;
    }
    info.codewords = InterleaveBlocks(data, params);
    if (info.codewords.size() != static_cast<size_t>(params.totalCodewords)) {
        return false;
    }

    QrCode code;
    code.version = version;
    code.size = 17 + 4 * version;
    const size_t area = static_cast<size_t>(code.size) * static_cast<size_t>(code.size);
    code.modules.assign(area, 0);
    info.function.assign(area, 0);

    DrawFunctionPatterns(code, info.function);
    DrawCodewords(code, info.function, info.codewords);

    // Sekiz maskeyi dene; XOR olduğu için aynı çağrı geri alır.
    int bestMask = 0;
    int bestPenalty = -1;
    for (int mask = 0; mask < 8; ++mask) {
        ApplyMask(code, info.function, mask);
        DrawFormatBits(code, ec, mask);
        const int penalty = PenaltyScore(code);
        if (bestPenalty < 0 || penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMask = mask;
        }
        ApplyMask(code, info.function, mask);
    }
    ApplyMask(code, info.function, bestMask);
    DrawFormatBits(code, ec, bestMask);

    info.mask = bestMask;
    out = std::move(code);
    return true;
}

}  // namespace qr

bool EncodeQr(const std::string& text, QrCode& out, QrEcLevel ec) {
    qr::EncodeInfo info;
    return qr::EncodeDetailed(text, out, ec, info);
}

bool RenderQr(const QrCode& qr, int moduleSize, int quietZone, Image& out) {
    if (qr.size <= 0 || moduleSize <= 0 || quietZone < 0 ||
        qr.modules.size() != static_cast<size_t>(qr.size) * static_cast<size_t>(qr.size)) {
        return false;
    }
    const int side = (qr.size + 2 * quietZone) * moduleSize;
    if (side > kMaxImageSide || !out.Create(side, side)) {
        return false;
    }
    out.Fill(0xFFFFFFFFu);
    for (int y = 0; y < qr.size; ++y) {
        for (int x = 0; x < qr.size; ++x) {
            if (!qr.Module(x, y)) {
                continue;
            }
            const int left = (x + quietZone) * moduleSize;
            const int top = (y + quietZone) * moduleSize;
            for (int py = 0; py < moduleSize; ++py) {
                for (int px = 0; px < moduleSize; ++px) {
                    out.SetPixel(left + px, top + py, 0xFF000000u);
                }
            }
        }
    }
    return true;
}

bool RenderQrToSize(const QrCode& qr, int targetPixels, int quietZone, Image& out) {
    if (qr.size <= 0 || quietZone < 0) {
        return false;
    }
    const int moduleSize = targetPixels / (qr.size + 2 * quietZone);
    return RenderQr(qr, moduleSize, quietZone, out);
}

}  // namespace crisp
