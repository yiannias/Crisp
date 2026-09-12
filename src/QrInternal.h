// QrInternal.h — QR kodlayıcının parçaları arasındaki ortak dil.
//
// Dışarıya (QrCode.h'a) çıkmıyor: çağıranın bilmesi gereken tek şey "metni
// ver, matrisi al". Ama buradaki her adım — tablo, Reed-Solomon, serpiştirme,
// maske — tek başına sınanabilir ve sınanmalı: yanlış tek bir bit kodu
// okunmaz yapar ve okunmayan bir kod hangi adımın yanlış olduğunu söylemez.
#pragma once

#include "QrCode.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crisp {
namespace qr {

inline constexpr int kMinVersion = 1;
inline constexpr int kMaxVersion = 40;

// Sürümün blok yapısı (ISO/IEC 18004 Tablo 9). Kod sözcükleri önce kısa
// bloklara, sonra bir fazla veri sözcüğü taşıyan uzun bloklara bölünür.
struct EcParams {
    int ecPerBlock = 0;     // her bloktaki hata düzeltme sözcüğü sayısı
    int blocks = 0;         // toplam blok sayısı
    int totalCodewords = 0; // veri + hata düzeltme, tüm bloklar
    int dataCodewords = 0;  // totalCodewords - blocks * ecPerBlock
    int shortBlocks = 0;    // veri sözcüğü sayısı `shortDataLen` olan bloklar
    int shortDataLen = 0;   // kısa bloktaki veri sözcüğü; uzunlar +1
};

// Sürümün taşıdığı toplam kod sözcüğü; düzeyden bağımsız.
[[nodiscard]] int TotalCodewords(int version) noexcept;

// Hata düzeltme tablosundan blok yapısı.
[[nodiscard]] EcParams EcParamsFor(int version, QrEcLevel ec) noexcept;

// Bayt kipinde sığan en fazla bayt (kip + sayaç başlığı düşülmüş).
[[nodiscard]] int ByteCapacity(int version, QrEcLevel ec) noexcept;

// Hizalama deseni merkezlerinin koordinatları (Ek E). Sürüm 1 için boş; her
// eksende aynı liste kullanılır, üç bulucu deseniyle çakışan üç köşe atlanır.
[[nodiscard]] std::vector<int> AlignmentPositions(int version);

// Bayt kipi sayaç alanının bit genişliği: 8 (v1-9) ya da 16 (v10-40).
[[nodiscard]] int ByteCountBits(int version) noexcept;

// --- QrEncodeData.cpp -------------------------------------------------------

// GF(256) çarpımı; indirgeme polinomu x^8+x^4+x^3+x^2+1 (0x11D).
[[nodiscard]] uint8_t GfMultiply(uint8_t a, uint8_t b) noexcept;

// Derecesi `degree` olan üreteç polinomu: (x-α^0)(x-α^1)...(x-α^(degree-1)).
// En yüksek dereceli terim (katsayısı 1) atılır; kalan `degree` katsayı döner.
[[nodiscard]] std::vector<uint8_t> ReedSolomonDivisor(int degree);

// Verinin üreteç polinomuna bölümünden kalan: hata düzeltme sözcükleri.
[[nodiscard]] std::vector<uint8_t> ReedSolomonRemainder(
    const std::vector<uint8_t>& data, const std::vector<uint8_t>& divisor);

// Metni veri kod sözcüklerine çevirir: kip, sayaç, baytlar, sonlandırıcı,
// dolgu (0xEC/0x11). Uzunluk daima `EcParamsFor(...).dataCodewords`.
// Sığmıyorsa boş döner.
[[nodiscard]] std::vector<uint8_t> BuildDataCodewords(const std::string& text,
                                                      int version, QrEcLevel ec);

// Veri sözcüklerini bloklara böler, her bloğa hata düzeltme ekler ve spec'in
// sırasıyla serpiştirir. Sonuç tam `TotalCodewords(version)` bayt.
[[nodiscard]] std::vector<uint8_t> InterleaveBlocks(const std::vector<uint8_t>& data,
                                                    const EcParams& params);

// --- QrMask.cpp -------------------------------------------------------------

// Maske deseninin (x, y) modülünü tersine çevirip çevirmediği.
[[nodiscard]] bool MaskBit(int mask, int x, int y) noexcept;

// Maskeyi uygular (ya da aynı çağrıyla geri alır: XOR). İşlev modülleri
// (`function` 1 olanlar) dokunulmadan kalır.
void ApplyMask(QrCode& code, const std::vector<uint8_t>& function, int mask);

// Dört ceza kuralının toplamı (N1=3, N2=3, N3=40, N4=10).
[[nodiscard]] int PenaltyScore(const QrCode& code);

// --- QrCode.cpp -------------------------------------------------------------

// Biçim bilgisi: 5 veri biti + BCH(15,5), 0x5412 ile maskelenmiş.
[[nodiscard]] uint32_t FormatBits(QrEcLevel ec, int mask) noexcept;

// Sürüm bilgisi (v7+): 6 veri biti + BCH(18,6).
[[nodiscard]] uint32_t VersionBits(int version) noexcept;

// Biçim bilgisini iki kopyasına da yazar. Maske seçimi sırasında her aday
// için yeniden çağrılır, çünkü ceza puanı biçim bitlerini de sayar.
void DrawFormatBits(QrCode& code, QrEcLevel ec, int mask);

// Kodlamanın ara sonuçları; testler matristen geri okuyup karşılaştırır.
struct EncodeInfo {
    int mask = -1;
    std::vector<uint8_t> codewords;   // serpiştirilmiş; matrise yazılan sıra
    std::vector<uint8_t> function;    // size*size; 1 = işlev modülü
};

[[nodiscard]] bool EncodeDetailed(const std::string& text, QrCode& out,
                                  QrEcLevel ec, EncodeInfo& info);

}  // namespace qr
}  // namespace crisp
