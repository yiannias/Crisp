// Stitch.cpp — bkz. Stitch.h. Kaydırma miktarı araması; alt alta ekleme
// StitchVertical.cpp'de, yan yana ekleme StitchHorizontal.cpp'de.
#include "Stitch.h"

#include "StitchInternal.h"

#include <algorithm>

namespace crisp {
namespace {

using stitch::RowFromTop;

// Kanal başına mutlak fark. Alfa yok sayılıyor: yakalanan görüntülerde her
// zaman 0xFF ve karşılaştırmaya bir şey katmıyor.
[[nodiscard]] uint64_t PixelDifference(uint32_t p, uint32_t q) noexcept {
    const int dr = static_cast<int>((p >> 16) & 0xFFu) -
                   static_cast<int>((q >> 16) & 0xFFu);
    const int dg = static_cast<int>((p >> 8) & 0xFFu) -
                   static_cast<int>((q >> 8) & 0xFFu);
    const int db = static_cast<int>(p & 0xFFu) - static_cast<int>(q & 0xFFu);
    return static_cast<uint64_t>(dr < 0 ? -dr : dr) +
           static_cast<uint64_t>(dg < 0 ? -dg : dg) +
           static_cast<uint64_t>(db < 0 ? -db : db);
}

// Bir satır/sütun grubunun ne kadar benzediği, erken çıkışlı.
//
// ERKEN ÇIKIŞ ÖNEMLİ: kaydırma miktarını ararken yüzlerce aday deneniyor ve
// adayların neredeyse tamamı ilk birkaç satırda eleniyor. Sınırı aşan bir
// adayı sonuna kadar hesaplamak, aramayı on kat yavaşlatırdı.
template <typename LineDifference>
[[nodiscard]] uint64_t BandDifference(const Image& a, int startA, const Image& b,
                                      int startB, int lines, uint64_t giveUpAt,
                                      LineDifference difference) noexcept {
    uint64_t total = 0;
    for (int i = 0; i < lines; ++i) {
        total += difference(a, startA + i, b, startB + i);
        if (total >= giveUpAt) {
            return total;
        }
    }
    return total;
}

// İKİ EKSENİN ORTAK ÇEKİRDEĞİ. Eksen boyunca [begin, end) aralığı aranır;
// `breadth` dik eksenin boyu (eşik bütçesi için), `difference` ise iki
// çizginin (satır ya da sütun) farkı. Dikey için begin=0, end=height,
// breadth=width verildiğinde, bu fonksiyon tek eksenli ilk sürümün adım adım
// aynısıdır — o sürümün sınamaları bunu doğruluyor.
template <typename LineDifference>
[[nodiscard]] int FindShiftAlong(const Image& previous, const Image& next,
                                 int minOverlap, int begin, int end,
                                 int breadth,
                                 LineDifference difference) noexcept {
    const int length = end - begin;
    if (length < 1) {
        return 0;
    }

    // ŞERİT BAŞTAN DEĞİL, ÜÇTE BİRDEN ALINIR.
    //
    // Kullanıcının seçtiği alan yalnızca kayan içerik olmayabilir: bir pencere
    // başlığı, bir araç çubuğu ya da sayfaya yapışık bir başlık üstte durur ve
    // kaydırıldıkça KIPIRDAMAZ. Şerit en üstten alındığında karşılaştırılan
    // şey o sabit bölge oluyordu; sabit bölge yalnızca kendisiyle, yani shift=0
    // ile eşleşir — ve shift=0 dışlandığı için hiçbir aday eşiği geçemiyordu.
    // Sonuç, kaydırmalı yakalamanın "hiçbir şey yakalanamadı" demesiydi, oysa
    // pencere gayet güzel kayıyordu.
    //
    // Üçte birden başlayan bir şerit, makul her başlığın altında kalıyor ve
    // yalnızca gerçekten kayan içeriği okuyor. Başlık AYRICA biliniyorsa
    // (`begin` > 0) şerit onun da altından, kalan aralığın üçte birinden
    // başlar: bilinen başlık bilinmeyen bir başlığı dışlamıyor.
    const int bandStart = begin + length / 3;
    const int overlap = (std::max)(1, (std::min)(minOverlap, end - bandStart));

    // shift = 0 anlamsız: hiç kaydırmamış bir pencere yeni bir şey göstermiyor
    // ve birleştirmenin durması gerekiyor. En büyük shift, şeridi aralığın
    // dışına taşırmayan değer.
    const int maxShift = end - bandStart - overlap;
    if (maxShift < 1) {
        return 0;
    }

    // ÖRTÜŞEN BÖLGENİN TAMAMI DEĞİL, BİR ŞERİDİ karşılaştırılıyor: `overlap`
    // çizgi, ayırt etmeye fazlasıyla yetiyor ve arama maliyetini kaydırma
    // miktarından bağımsız kılıyor.
    int best = 0;
    uint64_t bestScore = UINT64_MAX;
    for (int shift = 1; shift <= maxShift; ++shift) {
        // `next`in `shift` kadar kaydığı varsayımı: `next`in şeridi,
        // `previous`ın `shift` çizgi ilerisindeki aynı şeritle eşleşmeli.
        const uint64_t score =
            BandDifference(previous, bandStart + shift, next, bandStart, overlap,
                           bestScore, difference);
        if (score < bestScore) {
            bestScore = score;
            best = shift;
        }
    }

    // EŞİK: en iyi aday yeterince iyi değilse HİÇBİR şey döndürülmez.
    //
    // Her aramanın bir "en iyisi" vardır ve tamamen ilgisiz iki kare için de
    // bir sayı çıkar. O sayıyı kaydırma miktarı sanmak, alakasız iki şeridi
    // birbirine yapıştırılmış bir görüntü üretirdi — ve bu sessizce olurdu.
    // Piksel başına ortalama 12 birimlik (üç kanal toplamı) bir fark, JPEG
    // benzeri gürültüye ve alt piksel yazı yumuşatmasına yer bırakıyor ama
    // gerçekten farklı bir içeriği geçirmiyor.
    const uint64_t budget = static_cast<uint64_t>(breadth) *
                            static_cast<uint64_t>(overlap) * 12u;
    if (bestScore > budget) {
        return 0;
    }
    return best;
}

[[nodiscard]] bool SameSize(const Image& previous, const Image& next) noexcept {
    return previous.Valid() && next.Valid() &&
           previous.Width() == next.Width() &&
           previous.Height() == next.Height();
}

}  // namespace

uint64_t RowDifference(const Image& a, int rowA, const Image& b,
                       int rowB) noexcept {
    if (!a.Valid() || !b.Valid() || a.Width() != b.Width()) {
        return UINT64_MAX;
    }
    if (rowA < 0 || rowB < 0 || rowA >= a.Height() || rowB >= b.Height()) {
        return UINT64_MAX;
    }

    const uint32_t* left = RowFromTop(a, rowA);
    const uint32_t* right = RowFromTop(b, rowB);
    const int width = a.Width();

    uint64_t total = 0;
    for (int x = 0; x < width; ++x) {
        total += PixelDifference(left[x], right[x]);
    }
    return total;
}

uint64_t ColumnDifference(const Image& a, int columnA, const Image& b,
                          int columnB) noexcept {
    if (!a.Valid() || !b.Valid() || a.Height() != b.Height()) {
        return UINT64_MAX;
    }
    if (columnA < 0 || columnB < 0 || columnA >= a.Width() ||
        columnB >= b.Width()) {
        return UINT64_MAX;
    }

    // Sütun okuma satır atlayarak ilerliyor; önbellek için satır kadar dost
    // değil ama şerit yüz sütun ve arama yüzlerce aday — ölçülebilir bir
    // sorun olmadı.
    const int height = a.Height();
    uint64_t total = 0;
    for (int y = 0; y < height; ++y) {
        total += PixelDifference(RowFromTop(a, y)[columnA],
                                 RowFromTop(b, y)[columnB]);
    }
    return total;
}

int FindVerticalShift(const Image& previous, const Image& next,
                      int minOverlap) noexcept {
    return FindVerticalShift(previous, next, minOverlap, 0, 0);
}

int FindVerticalShift(const Image& previous, const Image& next, int minOverlap,
                      int headerRows, int footerRows) noexcept {
    if (!SameSize(previous, next)) {
        return 0;
    }
    // Saçma değerler sessizce sıfırlanır: negatif bir altlık ya da toplamı
    // kareyi aşan bir çift, aralığı boş bırakır ve çekirdek 0 döner.
    const int header = (std::max)(0, headerRows);
    const int footer = (std::max)(0, footerRows);
    return FindShiftAlong(previous, next, minOverlap, header,
                          previous.Height() - footer, previous.Width(),
                          &RowDifference);
}

int FindHorizontalShift(const Image& previous, const Image& next,
                        int minOverlap) noexcept {
    if (!SameSize(previous, next)) {
        return 0;
    }
    return FindShiftAlong(previous, next, minOverlap, 0, previous.Width(),
                          previous.Height(), &ColumnDifference);
}

}  // namespace crisp
