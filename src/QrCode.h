// QrCode.h — QR kodu üretimi (bayt kipi), bağımlılıksız.
//
// NEDEN VAR: yüklenen görüntünün bağlantısını telefona geçirmenin en kısa
// yolu, bağlantıyı ekrana bir QR kodu olarak iğnelemek. Kütüphane getirmek
// projenin "üçüncü taraf yok" kuralına takılıyor; kodlayıcı ISO/IEC 18004'e
// göre buradan yazıldı.
//
// YALNIZCA BAYT KİPİ. Girdi daima bir URL ve URL'ler sayısal ya da alfasayısal
// kiplerin kısıtlı alfabesine sığmıyor (küçük harf yok); bayt kipi her girdiyi
// olduğu gibi taşır ve tarayıcılar UTF-8 varsayar. Diğer kipler yalnızca
// birkaç modül tasarruf ederdi.
//
// Dosya bölünmüş durumda: tablolar QrTables.cpp'de, Reed-Solomon ile veri
// bit akışı QrEncodeData.cpp'de, maske ve ceza QrMask.cpp'de, matris
// yerleşimi ile bu başlıktaki API QrCode.cpp'de. Ortak dil QrInternal.h.
#pragma once

#include "Capture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace crisp {

// Hata düzeltme düzeyi: sırasıyla ~%7, %15, %25, %30 hasarı tolere eder.
// Varsayılan M: ekrandan telefonla okunan bir kod için yeterli ve L'ye göre
// yalnızca bir iki sürüm daha büyük.
enum class QrEcLevel { L, M, Q, H };

struct QrCode {
    int version = 0;                 // 1..40
    int size = 0;                    // modül sayısı (kenar) = 17 + 4*version
    std::vector<uint8_t> modules;    // size*size, satır satır; 1 = koyu

    // x sütun, y satır. Sınır dışı → açık (false), tarayıcının sessiz bölgesi
    // gibi; çağıranların kenar kontrolü yazması gerekmesin.
    [[nodiscard]] bool Module(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= size || y >= size) {
            return false;
        }
        return modules[static_cast<size_t>(y) * static_cast<size_t>(size) +
                       static_cast<size_t>(x)] != 0;
    }
};

// Metni (UTF-8 baytları olduğu gibi) en küçük sığan sürümde kodlar. Hiçbir
// sürüme sığmıyorsa false.
[[nodiscard]] bool EncodeQr(const std::string& text, QrCode& out,
                            QrEcLevel ec = QrEcLevel::M);

// Kodu siyah-beyaz bir görüntüye çizer: her modül `moduleSize` piksel, kenarda
// `quietZone` modül genişliğinde beyaz boşluk. Spec'in sessiz bölgesi 4 modül;
// beyaz bir kartın üstüne konacak bir kod için daha azı da okunur.
[[nodiscard]] bool RenderQr(const QrCode& qr, int moduleSize, int quietZone,
                            Image& out);

// Aynısı, ama hedef piksel boyutu verilir; modül boyutu sığan en büyük tam
// sayı seçilir (görüntü hedeften biraz küçük çıkabilir, asla büyük değil).
[[nodiscard]] bool RenderQrToSize(const QrCode& qr, int targetPixels,
                                  int quietZone, Image& out);

}  // namespace crisp
