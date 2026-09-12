// StitchVertical.cpp — bkz. Stitch.h. Kareleri alt alta ekler; yapışık
// başlık ve altlığı tanır.
#include "Stitch.h"

#include "StitchInternal.h"

#include <algorithm>
#include <cstring>

namespace crisp {
namespace {

using stitch::MutableRowFromTop;
using stitch::RowFromTop;

void CopyRow(Image& out, int outRow, const Image& from, int fromRow) noexcept {
    std::memcpy(MutableRowFromTop(out, outRow), RowFromTop(from, fromRow),
                static_cast<size_t>(from.Width()) * sizeof(uint32_t));
}

// Bütün karelerde aynı olan kenar satırlarını sayar. `fromBottom` alt
// kenardan, değilse üst kenardan başlar.
//
// HER KARE İLK KAREYLE KARŞILAŞTIRILIR: "hepsi birbirine eşit" demek "hepsi
// ilkine eşit" demek. Bir satırın BİR karede bile farklı çıkması sayımı
// bitirir; ötesindeki satırlar ne kadar aynı olursa olsun kenara yapışık
// değildir, çünkü aralarında hareket eden bir satır var.
[[nodiscard]] int CountStickyRows(const std::vector<Image>& frames,
                                  bool fromBottom) noexcept {
    if (frames.size() < 2 || !frames.front().Valid()) {
        return 0;
    }
    const Image& first = frames.front();
    const int height = first.Height();
    for (const Image& frame : frames) {
        if (!frame.Valid() || frame.Width() != first.Width() ||
            frame.Height() != height) {
            return 0;
        }
    }

    // ÜÇTE BİRDEN FAZLASI "KENAR" DEĞİLDİR. Karenin üçte birinden çoğu bütün
    // karelerde aynıysa sayfa ya çok az kaymış ya da hiç kaymamıştır; bunu
    // yapışık altlık diye kesip atmak içeriği yerdi. Sınır, arama şeridinin
    // başladığı üçte birle aynı: ikisi birlikte, şeridin bilinen bir sabit
    // bölgeye hiç düşmemesini sağlıyor.
    const int limit = height / 3;
    int sticky = 0;
    while (sticky < limit) {
        const int row = fromBottom ? height - 1 - sticky : sticky;
        bool same = true;
        for (size_t i = 1; i < frames.size() && same; ++i) {
            same = RowDifference(first, row, frames[i], row) == 0;
        }
        if (!same) {
            break;
        }
        ++sticky;
    }
    return sticky;
}

}  // namespace

int DetectStickyFooter(const std::vector<Image>& frames) noexcept {
    return CountStickyRows(frames, true);
}

int DetectStickyHeader(const std::vector<Image>& frames) noexcept {
    return CountStickyRows(frames, false);
}

bool StitchVertical(const std::vector<Image>& frames, int minOverlap, Image& out,
                    size_t* stopped) {
    return StitchVertical(frames, minOverlap, StitchOptions{}, out, stopped);
}

bool StitchVertical(const std::vector<Image>& frames, int minOverlap,
                    const StitchOptions& options, Image& out, size_t* stopped) {
    if (stopped != nullptr) {
        *stopped = frames.size();
    }
    if (frames.empty() || !frames.front().Valid()) {
        return false;
    }

    const int width = frames.front().Width();
    const int height = frames.front().Height();

    // YAPIŞIK KENARLAR ÖNCE TESPİT EDİLİR, arama onlara göre daraltılır.
    //
    // Altlık aramaya katılsaydı, şeridin `previous`taki karşılığı büyük
    // kaydırmalarda altlığa taşar ve o adaylar boşa elenirdi. Başlık için
    // şerit zaten üçte birden başlıyor; bilinen başlığı da vermek, kalan
    // aralığın üçte birinden başlamayı sağlıyor ve tespit edilen başlığın
    // sınırından bağımsız olarak şeridi onun dışında tutuyor.
    const int header = options.trimStickyHeader ? DetectStickyHeader(frames) : 0;
    const int footer = options.trimStickyFooter ? DetectStickyFooter(frames) : 0;

    // Kayan içeriğin alt sınırı: altlığın üstü. Ara karelerin "yeni" şeridi
    // buradan geriye sayılır, altlık hiç dahil edilmez.
    const int contentEnd = height - footer;

    std::vector<int> shifts;
    int total = height;
    const size_t used = stitch::PlanShifts(
        frames, width, height,
        [&](const Image& previous, const Image& next) {
            return FindVerticalShift(previous, next, minOverlap, header, footer);
        },
        shifts, total);

    if (stopped != nullptr) {
        *stopped = used;
    }
    if (!out.Create(width, total)) {
        return false;
    }

    // İlk karenin İÇERİĞİ bütünüyle: başlık dahil, altlık hariç. Altlık en
    // sona, son kareden gelecek. Tek kare varsa contentEnd == height, çünkü
    // tespit ikiden az karede sıfır döner.
    for (int y = 0; y < contentEnd; ++y) {
        CopyRow(out, y, frames[0], y);
    }

    // Sonrakilerin YALNIZCA yeni kısmı: içeriğin son `shift` satırı.
    //
    // BAŞLIK BURADAN HİÇ KOPYALANMAZ, yapı gereği: kaydırma en çok
    // (contentEnd - header) * 2/3 kadar olabilir (arama şeridi aralığın üçte
    // birinden başlıyor), yani contentEnd - shift her zaman başlığın
    // altındadır. Altlık da kopyalanmaz, çünkü şerit contentEnd'de bitiyor.
    int writtenTo = contentEnd;
    for (size_t i = 0; i < shifts.size(); ++i) {
        const Image& frame = frames[i + 1];
        const int shift = shifts[i];
        for (int row = 0; row < shift; ++row) {
            CopyRow(out, writtenTo + row, frame, contentEnd - shift + row);
        }
        writtenTo += shift;
    }

    // ALTLIK BİR KEZ, SON KULLANILAN KAREDEN. Kullanıcının gördüğü son hâl
    // budur (sohbet kutusunda yazılmış metin, kaydırma çubuğunun son konumu)
    // ve birleşik görüntü tam da bir sayfanın alta kadar kaydırılmış hâli
    // gibi biter. Toplam boy değişmedi: height + Σshift; altlık ara karelerden
    // eksildiği kadar sona eklendi.
    const Image& last = frames[used - 1];
    for (int row = contentEnd; row < height; ++row) {
        CopyRow(out, writtenTo + row - contentEnd, last, row);
    }
    return true;
}

}  // namespace crisp
