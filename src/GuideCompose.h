// GuideCompose.h — Adım görüntülerini numaralı tek bir görüntüde birleştirir.
//
// NEDEN AYRI BİR BİRLEŞTİRİCİ: Stitch.h örtüşen kareleri KAYDIRMA miktarını
// bularak yapıştırır — aynı pencerenin devamı olduklarını varsayar. Kılavuz
// adımları ise birbirinden bağımsız yakalamalardır: farklı pencereler, farklı
// boyutlar. Burada örtüşme aranmaz; her adım kendi kutusuna konur, numaralanır
// ve alt alta dizilir.
//
// PENCERE YOK: çıktı düz bir Image'dır ve yalnızca bellek DC'si kullanılır,
// böylece yerleşim aritmetiği konsol koşucusundan sınanabilir.
#pragma once

#include "Capture.h"

#include <cstdint>
#include <vector>

namespace crisp {

// Renkler 0xAARGGBB düzeninde, Image::Pixel ile aynı.
struct GuideOptions {
    // Sonucun en fazla genişliği. Daha geniş adımlar bu genişliğe SIĞACAK
    // şekilde küçültülür; dar adımlar asla büyütülmez — ekran görüntüsünü
    // büyütmek metni bulanıklaştırır ve hiçbir şey kazandırmaz.
    int maxWidth = 1600;
    // Kenar boşluğu: rozetin görüntü dışına taşan yarısı buraya sığar.
    int padding = 24;
    // Ardışık iki adım arasındaki boşluk.
    int gap = 32;
    // Numara rozetinin çapı; 0 rozeti kapatır.
    int badgeDiameter = 44;
    uint32_t background = 0xFFFFFFFF;
    uint32_t badgeColor = 0xFFE5322D;
    uint32_t badgeText = 0xFFFFFFFF;
    // Her adımın çevresine 1 piksellik çerçeve: beyaz zeminli bir pencere,
    // beyaz arka planda kenarını kaybeder.
    bool drawFrame = true;
};

// Adımları dikey sütunda birleştirir. Sütun genişliği = en geniş (küçültülmüş)
// adım + 2·padding; yükseklik = padding + Σ(adım + gap) − gap + padding.
//
// Boş liste ya da geçersiz bir görüntü false döndürür: yarım bir kılavuz
// üretip "bir adım eksik" dememek, kullanıcının eksik adımı hiç fark
// etmemesine yol açardı.
//
// SINIRA SIĞMAYAN KUYRUK ATILIR: toplam yükseklik kMaxImageSide'ı aşarsa
// yalnızca sığan adımlar yerleştirilir ve en az bir adım yerleştiyse true
// döner. Stitch ile aynı seçim — yakalanan her şeyi çöpe atmaktansa eldeki
// kadarını teslim etmek. Kaç adımın yerleştiği `placed` ile öğrenilir.
[[nodiscard]] bool ComposeGuide(const std::vector<Image>& steps,
                                const GuideOptions& options, Image& out,
                                size_t* placed = nullptr);

[[nodiscard]] bool ComposeGuide(const std::vector<Image>& steps, Image& out);

}  // namespace crisp
