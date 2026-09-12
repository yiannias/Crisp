// StitchInternal.h — Stitch*.cpp dosyalarının paylaştığı yardımcılar.
//
// YALNIZCA BİRLEŞTİRME DOSYALARI İÇİN. Buradaki hiçbir şey sınır denetimi
// yapmaz; çağıran, satır ve sütunun görüntünün içinde olduğunu bilir. Genel
// başlığa koymak, bu varsayımı bilmeyen bir çağıranı davet ederdi.
#pragma once

#include "Capture.h"

#include <vector>

namespace crisp::stitch {

// SATIR 0 EN ÜST SATIRDIR, çevirme yok.
//
// DIB'lerin çoğu alttan üste saklanır ve bu dosya bir süre öyle varsaydı;
// sınamalar bilinen kaydırmayı bulamayınca ortaya çıktı. `Image::Create`
// biHeight'ı NEGATİF veriyor — yani top-down bir DIB — ve `Image::Pixel` de
// tamponu doğrudan `y` ile indeksliyor. Buradaki çevirme, zaten doğru duran
// bir tamponu ters çeviriyordu.
[[nodiscard]] inline const uint32_t* RowFromTop(const Image& image,
                                                int row) noexcept {
    const auto* base = static_cast<const uint32_t*>(image.Bits());
    return base + static_cast<size_t>(row) * static_cast<size_t>(image.Width());
}

[[nodiscard]] inline uint32_t* MutableRowFromTop(const Image& image,
                                                 int row) noexcept {
    auto* base = static_cast<uint32_t*>(image.Bits());
    return base + static_cast<size_t>(row) * static_cast<size_t>(image.Width());
}

// Ardışık kareler arasındaki kaydırma miktarlarını toplar; iki eksenin ortak
// ön geçişi.
//
// KAYDIRMA MİKTARLARI ÖNCE HESAPLANIR: toplam boyu bilmeden hedef görüntü
// ayrılamaz, ve iki kez geçmek tek geçişte büyüyen bir tampon yönetmekten
// basit. `total` çağıranın verdiği başlangıç boyundan (ilk karenin eksen
// boyu) başlar ve her kabul edilen kaydırma kadar büyür.
//
// Durma kuralları eksenden bağımsız: ölçüsü tutmayan ya da geçersiz kare,
// eşleşme bulunamayan kare (`find` 0 döner) ve kMaxImageSide'ı aşacak kare —
// hepsi birleştirmeyi ORADA bitirir. Dönen değer kullanılan kare sayısı.
template <typename FindShift>
[[nodiscard]] size_t PlanShifts(const std::vector<Image>& frames, int width,
                                int height, FindShift find,
                                std::vector<int>& shifts, int& total) {
    shifts.clear();
    shifts.reserve(frames.size());
    size_t used = 1;
    for (size_t i = 1; i < frames.size(); ++i) {
        if (!frames[i].Valid() || frames[i].Width() != width ||
            frames[i].Height() != height) {
            break;
        }
        const int shift = find(frames[i - 1], frames[i]);
        if (shift <= 0) {
            break;   // eşleşme yok: burada bitiyoruz
        }
        // Image::Create tek kenarı kMaxImageSide ile sınırlar; sınırı aşan bir
        // kare eklemek bütün birleştirmeyi düşürürdü. O kareden önce durulur
        // ve elde olan kadarı teslim edilir.
        if (total + shift > kMaxImageSide) {
            break;
        }
        shifts.push_back(shift);
        total += shift;
        ++used;
    }
    return used;
}

}  // namespace crisp::stitch
