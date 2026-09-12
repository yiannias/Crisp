// TestStitch.cpp — Kaydırmalı yakalamanın birleştirme yarısı.
//
// EKRANA HİÇ BAKMIYOR. Bütün girdi burada üretiliyor: bilinen bir desen,
// bilinen bir miktar kaydırılıyor, ve aynı sayı geri isteniyor. Bir hizalama
// hatasını çıplak gözle "biraz kaymış" diye fark etmek saatler alır.
#include "TestFramework.h"

#include "Stitch.h"

#include <vector>

using namespace crisp;

namespace {

// Her satırı, satır numarasından türeyen ayırt edici bir desenle dolduran
// görüntü. `offset` sayfanın ne kadar kaydırıldığı.
//
// DESEN SATIRA GÖRE DEĞİŞMELİ: her satırı aynı olan bir görüntüde "kaç piksel
// kaymış" sorusunun tek bir doğru cevabı yoktur ve sınama, gerçekte olmayan
// bir kesinliği ölçmüş olurdu.
[[nodiscard]] bool MakeFrame(int width, int height, int offset, Image& out) {
    if (!out.Create(width, height)) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        const int line = y + offset;
        for (int x = 0; x < width; ++x) {
            const uint32_t r = static_cast<uint32_t>((line * 7 + x * 3) & 0xFF);
            const uint32_t g = static_cast<uint32_t>((line * 13) & 0xFF);
            const uint32_t b = static_cast<uint32_t>((x * 5 + line * 2) & 0xFF);
            out.SetPixel(x, y, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
    }
    return true;
}

// Tek renk: hiçbir satır diğerinden ayırt edilemez.
[[nodiscard]] bool MakeFlat(int width, int height, uint32_t colour, Image& out) {
    if (!out.Create(width, height)) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            out.SetPixel(x, y, colour);
        }
    }
    return true;
}

}  // namespace

CRISP_TEST(Stitch, Bilinen_kaydirmayi_geri_bulur) {
    // Şerit karenin üçte birinden başlıyor ve `minOverlap` satır sürüyor, yani
    // 200 satırlık bir karede en büyük bulunabilir kaydırma 200 - 66 - 40 = 94.
    // Bunun üstü "bulunamadı"dır ve öyle olmalı: o kadar hızlı kaydırılmış iki
    // karenin paylaştığı bir şerit yok.
    for (const int shift : {1, 5, 40, 94}) {
        Image a;
        Image b;
        CHECK(MakeFrame(64, 200, 0, a));
        CHECK(MakeFrame(64, 200, shift, b));
        CHECK_EQ(FindVerticalShift(a, b, 40), shift);
    }
    for (const int shift : {95, 150, 199}) {
        Image a;
        Image b;
        CHECK(MakeFrame(64, 200, 0, a));
        CHECK(MakeFrame(64, 200, shift, b));
        CHECK_EQ(FindVerticalShift(a, b, 40), 0);
    }
}

CRISP_TEST(Stitch, Ustteki_sabit_baslik_eslesmeyi_bozmaz) {
    // GERÇEK BİR HATANIN SINAMASI. Kullanıcının seçtiği alan pencere başlığını
    // ya da sayfaya yapışık bir başlığı içerdiğinde, karenin ÜST kısmı
    // kaydırıldıkça değişmiyor. Şerit en üstten alındığı sürümde bu, hiçbir
    // adayın eşleşmemesine ve "hiçbir şey yakalanamadı" iletisine yol
    // açıyordu — pencere kayıyor olmasına rağmen.
    const int width = 64;
    const int height = 300;
    const int header = 90;
    const int shift = 45;

    Image a;
    Image b;
    CHECK(MakeFrame(width, height, 0, a));
    CHECK(MakeFrame(width, height, shift, b));

    // İki karenin de üst `header` satırını AYNI sabit içerikle ez.
    for (int y = 0; y < header; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t colour =
                0xFF000000u | static_cast<uint32_t>((y * 3 + x) & 0xFF);
            a.SetPixel(x, y, colour);
            b.SetPixel(x, y, colour);
        }
    }

    CHECK_EQ(FindVerticalShift(a, b, 40), shift);
}

CRISP_TEST(Stitch, Ayni_kare_kaydirma_bulmaz) {
    // Hiç kaydırılmamış pencere yeni bir şey göstermiyor; 0 dönmeli ki
    // birleştirme dursun. Aksi hâlde aynı şerit tekrar tekrar eklenirdi.
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFrame(64, 200, 0, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);
}

CRISP_TEST(Stitch, Alakasiz_kareler_uydurmaz) {
    // İKİ İLGİSİZ KARE İÇİN DE BİR "EN İYİ" ADAY VARDIR. Eşik olmasaydı o
    // aday kaydırma miktarı sanılır ve birbirine yapıştırılmış iki alakasız
    // şerit üretilirdi — sessizce, hata vermeden.
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFlat(64, 200, 0xFF102030u, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);
}

CRISP_TEST(Stitch, Farkli_olculer_kaydirma_bulmaz) {
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFrame(80, 200, 20, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);

    Image c;
    CHECK(MakeFrame(64, 150, 20, c));
    CHECK_EQ(FindVerticalShift(a, c, 40), 0);
}

CRISP_TEST(Stitch, Bes_kare_tek_uzun_goruntu_olur) {
    // Kare boyu gerçekçi tutuluyor: şerit karenin üçte birinden başladığı için
    // yüz satırlık bir karede bulunabilir en büyük kaydırma yirmi yediye
    // düşüyor ve sınama, ölçtüğü şeyi değil kendi seçtiği sayıları sınamış
    // olurdu.
    const int width = 48;
    const int height = 200;
    const int shift = 40;

    std::vector<Image> frames;
    for (int i = 0; i < 5; ++i) {
        Image frame;
        CHECK(MakeFrame(width, height, i * shift, frame));
        frames.push_back(std::move(frame));
    }

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, out, &used));
    CHECK_EQ(used, static_cast<size_t>(5));
    CHECK_EQ(out.Width(), width);
    CHECK_EQ(out.Height(), height + shift * 4);

    // BİRLEŞTİRİLEN GÖRÜNTÜ, KESİNTİSİZ SAYFANIN KENDİSİ OLMALI. Her satır,
    // hiç kaydırılmamış tek bir uzun kareden alınmış gibi olmalı; bir piksel
    // kayma bile burada yakalanır.
    Image whole;
    CHECK(MakeFrame(width, out.Height(), 0, whole));
    for (int y = 0; y < out.Height(); ++y) {
        CHECK_EQ(RowDifference(out, y, whole, y), static_cast<uint64_t>(0));
    }
}

CRISP_TEST(Stitch, Eslesmeyen_karede_durur_ve_soyler) {
    const int width = 48;
    const int height = 200;

    std::vector<Image> frames;
    Image first;
    Image second;
    Image stranger;
    CHECK(MakeFrame(width, height, 0, first));
    CHECK(MakeFrame(width, height, 40, second));
    CHECK(MakeFlat(width, height, 0xFF884422u, stranger));
    frames.push_back(std::move(first));
    frames.push_back(std::move(second));
    frames.push_back(std::move(stranger));

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, out, &used));

    // Üçüncü kare eklenmedi ve bu SÖYLENDİ.
    CHECK_EQ(used, static_cast<size_t>(2));
    CHECK_EQ(out.Height(), height + 40);
}

CRISP_TEST(Stitch, Tek_kare_kendisidir) {
    std::vector<Image> frames;
    Image only;
    CHECK(MakeFrame(32, 60, 0, only));
    frames.push_back(std::move(only));

    Image out;
    CHECK(StitchVertical(frames, 20, out, nullptr));
    CHECK_EQ(out.Height(), 60);
    CHECK_EQ(out.Width(), 32);
}

CRISP_TEST(Stitch, Bos_liste_basarisiz) {
    const std::vector<Image> frames;
    Image out;
    CHECK(!StitchVertical(frames, 20, out, nullptr));
}

CRISP_TEST(Stitch, Satir_farki_ayni_satirda_sifir) {
    Image a;
    CHECK(MakeFrame(32, 40, 0, a));
    CHECK_EQ(RowDifference(a, 5, a, 5), static_cast<uint64_t>(0));
    CHECK(RowDifference(a, 5, a, 6) > 0);
    // Sınır dışı okuma sessizce 0 dönmemeli: 0 "birebir aynı" demek.
    CHECK_EQ(RowDifference(a, -1, a, 0), UINT64_MAX);
    CHECK_EQ(RowDifference(a, 0, a, 40), UINT64_MAX);
}

// ---------------------------------------------------------------------------
// Yatay
// ---------------------------------------------------------------------------

namespace {

// MakeFrame'in aynadaki görüntüsü: desen SÜTUNA göre değişir ve `offset`
// sayfanın ne kadar sağa kaydırıldığıdır.
[[nodiscard]] bool MakeWideFrame(int width, int height, int offset, Image& out) {
    if (!out.Create(width, height)) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int column = x + offset;
            const uint32_t r = static_cast<uint32_t>((column * 7 + y * 3) & 0xFF);
            const uint32_t g = static_cast<uint32_t>((column * 13) & 0xFF);
            const uint32_t b = static_cast<uint32_t>((y * 5 + column * 2) & 0xFF);
            out.SetPixel(x, y, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
    }
    return true;
}

// Karelerin [top, top + rows) satırlarını hepsinde AYNI sabit desenle ezer:
// yapışık başlık ya da altlık taklidi.
void PaintStickyRows(std::vector<Image>& frames, int top, int rows) {
    for (Image& frame : frames) {
        for (int y = top; y < top + rows; ++y) {
            for (int x = 0; x < frame.Width(); ++x) {
                frame.SetPixel(x, y, 0xFF000000u |
                                         static_cast<uint32_t>((y * 3 + x) & 0xFF));
            }
        }
    }
}

// `count` kare, her biri `shift` satır aşağı kaymış.
[[nodiscard]] bool MakeScrolledFrames(int width, int height, int shift,
                                      int count, std::vector<Image>& frames) {
    for (int i = 0; i < count; ++i) {
        Image frame;
        if (!MakeFrame(width, height, i * shift, frame)) {
            return false;
        }
        frames.push_back(std::move(frame));
    }
    return true;
}

}  // namespace

CRISP_TEST(Stitch, Yatay_bilinen_kaydirmayi_geri_bulur) {
    // Dikeyin aynası: 200 sütunluk karede şerit 66'dan başlar, 40 sütun sürer,
    // bulunabilir en büyük kaydırma 94.
    for (const int shift : {1, 5, 40, 94}) {
        Image a;
        Image b;
        CHECK(MakeWideFrame(200, 64, 0, a));
        CHECK(MakeWideFrame(200, 64, shift, b));
        CHECK_EQ(FindHorizontalShift(a, b, 40), shift);
    }
    for (const int shift : {95, 150, 199}) {
        Image a;
        Image b;
        CHECK(MakeWideFrame(200, 64, 0, a));
        CHECK(MakeWideFrame(200, 64, shift, b));
        CHECK_EQ(FindHorizontalShift(a, b, 40), 0);
    }
}

CRISP_TEST(Stitch, Yatay_alakasiz_kareler_uydurmaz) {
    Image a;
    Image b;
    Image same;
    CHECK(MakeWideFrame(200, 64, 0, a));
    CHECK(MakeFlat(200, 64, 0xFF102030u, b));
    CHECK(MakeWideFrame(200, 64, 0, same));
    CHECK_EQ(FindHorizontalShift(a, b, 40), 0);
    CHECK_EQ(FindHorizontalShift(a, same, 40), 0);
    // Dikey desenli bir kare yatayda kaymış sayılmaz: eksenler karışmıyor.
    Image tall;
    Image tallShifted;
    CHECK(MakeFrame(200, 64, 0, tall));
    CHECK(MakeFrame(200, 64, 20, tallShifted));
    CHECK_EQ(FindHorizontalShift(tall, tallShifted, 40), 0);
}

CRISP_TEST(Stitch, Dort_kare_tek_genis_goruntu_olur) {
    const int width = 200;
    const int height = 48;
    const int shift = 40;

    std::vector<Image> frames;
    for (int i = 0; i < 4; ++i) {
        Image frame;
        CHECK(MakeWideFrame(width, height, i * shift, frame));
        frames.push_back(std::move(frame));
    }

    Image out;
    size_t used = 0;
    CHECK(StitchHorizontal(frames, 40, out, &used));
    CHECK_EQ(used, static_cast<size_t>(4));
    CHECK_EQ(out.Height(), height);
    CHECK_EQ(out.Width(), width + shift * 3);

    // Kesintisiz geniş sayfanın kendisi olmalı; sütun sütun.
    Image whole;
    CHECK(MakeWideFrame(out.Width(), height, 0, whole));
    for (int x = 0; x < out.Width(); ++x) {
        CHECK_EQ(ColumnDifference(out, x, whole, x), static_cast<uint64_t>(0));
    }

    // Tek kare ve boş liste, dikeyle aynı sözleşme.
    std::vector<Image> one;
    Image only;
    CHECK(MakeWideFrame(60, 30, 0, only));
    one.push_back(std::move(only));
    CHECK(StitchHorizontal(one, 20, out, nullptr));
    CHECK_EQ(out.Width(), 60);
    CHECK(!StitchHorizontal(std::vector<Image>{}, 20, out, nullptr));
}

// ---------------------------------------------------------------------------
// Yapışık başlık ve altlık
// ---------------------------------------------------------------------------

CRISP_TEST(Stitch, Yapisik_altlik_ve_baslik_tespit_edilir) {
    std::vector<Image> frames;
    CHECK(MakeScrolledFrames(48, 300, 40, 4, frames));
    CHECK_EQ(DetectStickyFooter(frames), 0);
    CHECK_EQ(DetectStickyHeader(frames), 0);

    PaintStickyRows(frames, 300 - 30, 30);
    PaintStickyRows(frames, 0, 50);
    CHECK_EQ(DetectStickyFooter(frames), 30);
    CHECK_EQ(DetectStickyHeader(frames), 50);

    // Altlığı BİR karede bir piksel farklı yap: o satır ve üstü artık altlık
    // değil, yalnızca altındaki satırlar sayılır.
    frames[2].SetPixel(3, 300 - 10, 0xFF00FF00u);
    CHECK_EQ(DetectStickyFooter(frames), 9);

    // Tek kare: karşılaştırılacak bir şey yok.
    std::vector<Image> one;
    CHECK(MakeScrolledFrames(48, 300, 40, 1, one));
    PaintStickyRows(one, 200, 100);
    CHECK_EQ(DetectStickyFooter(one), 0);

    // Üçte birden fazlası aynıysa sınırda kesilir: 150 boyanır, 100 döner.
    std::vector<Image> tall;
    CHECK(MakeScrolledFrames(48, 300, 40, 3, tall));
    PaintStickyRows(tall, 150, 150);
    CHECK_EQ(DetectStickyFooter(tall), 100);
}

CRISP_TEST(Stitch, Yuksek_altlik_aramayi_daraltmazsa_kaydirma_kacar) {
    // 300 satırlık karede 100 satırlık altlık: üç parametreli arama şeridi
    // 100'den başlatır ve 80'lik bir kaydırmada `previous`taki karşılık
    // (180..220) altlığa taşar — aday elenir. Altlığı bilen arama 0..200
    // aralığında çalışır ve 80'i bulur.
    std::vector<Image> frames;
    CHECK(MakeScrolledFrames(48, 300, 80, 2, frames));
    PaintStickyRows(frames, 200, 100);
    CHECK_EQ(FindVerticalShift(frames[0], frames[1], 40), 0);
    CHECK_EQ(FindVerticalShift(frames[0], frames[1], 40, 0, 100), 80);
    // Sıfır/sıfır ile üç parametreli sürümün aynısı.
    std::vector<Image> plain;
    CHECK(MakeScrolledFrames(48, 300, 80, 2, plain));
    CHECK_EQ(FindVerticalShift(plain[0], plain[1], 40, 0, 0), 80);
}

CRISP_TEST(Stitch, Yapisik_altlik_bir_kez_ve_en_alta_gelir) {
    const int width = 48;
    const int height = 300;
    const int footer = 30;
    const int shift = 40;
    const int count = 5;

    std::vector<Image> frames;
    CHECK(MakeScrolledFrames(width, height, shift, count, frames));
    PaintStickyRows(frames, height - footer, footer);

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, StitchOptions{}, out, &used));
    CHECK_EQ(used, static_cast<size_t>(count));

    // İçerik: (height - footer) + 4 * shift = 430; artı altlık bir kez = 460.
    const int content = (height - footer) + shift * (count - 1);
    CHECK_EQ(out.Height(), content + footer);

    // [0, content) kesintisiz sayfa — altlık hiçbir yerde araya girmemiş.
    Image whole;
    CHECK(MakeFrame(width, content, 0, whole));
    for (int y = 0; y < content; ++y) {
        CHECK_EQ(RowDifference(out, y, whole, y), static_cast<uint64_t>(0));
    }
    // [content, content + footer) son karenin altlığı.
    for (int row = 0; row < footer; ++row) {
        CHECK_EQ(RowDifference(out, content + row, frames.back(),
                               height - footer + row),
                 static_cast<uint64_t>(0));
    }

    // AYIKLAMA KAPALIYKEN eski davranış: aynı toplam boy, ama altlık ikinci
    // karenin şeridiyle birlikte ortaya kopyalanmış.
    StitchOptions keep;
    keep.trimStickyFooter = false;
    Image old;
    CHECK(StitchVertical(frames, 40, keep, old, nullptr));
    CHECK_EQ(old.Height(), content + footer);
    CHECK_EQ(RowDifference(old, height + shift - 1, frames[1], height - 1),
             static_cast<uint64_t>(0));
    CHECK(RowDifference(out, height + shift - 1, frames[1], height - 1) > 0);
}

CRISP_TEST(Stitch, Yapisik_baslik_bir_kez_kopyalanir) {
    const int width = 48;
    const int height = 300;
    const int header = 90;
    const int shift = 40;
    const int count = 5;

    std::vector<Image> frames;
    CHECK(MakeScrolledFrames(width, height, shift, count, frames));
    PaintStickyRows(frames, 0, header);
    CHECK_EQ(DetectStickyHeader(frames), header);

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, out, &used));
    CHECK_EQ(used, static_cast<size_t>(count));
    CHECK_EQ(out.Height(), height + shift * (count - 1));

    // Başlık yalnızca en üstte; altındaki her satır kesintisiz sayfa.
    Image whole;
    CHECK(MakeFrame(width, out.Height(), 0, whole));
    for (int y = 0; y < header; ++y) {
        CHECK_EQ(RowDifference(out, y, frames[0], y), static_cast<uint64_t>(0));
    }
    for (int y = header; y < out.Height(); ++y) {
        CHECK_EQ(RowDifference(out, y, whole, y), static_cast<uint64_t>(0));
    }
}
