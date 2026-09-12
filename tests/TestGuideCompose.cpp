// TestGuideCompose.cpp — Kılavuz birleştiricinin yerleşim aritmetiği.
//
// EKRANA BAKMAZ: adımlar tek renkli görüntüler olarak burada üretilir ve
// sonuçta "nerede hangi renk olmalı" sorulur. Rozet numarasının biçimi
// (yazı tipi, yumuşatma) sınanmaz — o makineye göre değişir; numaranın
// rozeti gerçekten BOYADIĞI sınanır.
#include "TestFramework.h"

#include "GuideCompose.h"

#include <vector>

using namespace crisp;

namespace {

constexpr uint32_t kRed = 0xFFFF0000u;
constexpr uint32_t kGreen = 0xFF00FF00u;
constexpr uint32_t kBlue = 0xFF0000FFu;

[[nodiscard]] Image MakeFlat(int width, int height, uint32_t colour) {
    Image image;
    if (image.Create(width, height)) {
        image.Fill(colour);
    }
    return image;
}

[[nodiscard]] std::vector<Image> ThreeSteps() {
    std::vector<Image> steps;
    steps.push_back(MakeFlat(300, 200, kRed));
    steps.push_back(MakeFlat(300, 200, kGreen));
    steps.push_back(MakeFlat(300, 200, kBlue));
    return steps;
}

// Rozet merkezinin altında, rakamların bitiminden sonra ama dairenin içinde
// kalan bir nokta: rakam yüksekliği çapın %55'i, yani merkezden en çok
// 0.55·r aşağı iner; 0.75·r hâlâ dairenin içinde.
[[nodiscard]] uint32_t BelowBadgeCentre(const Image& out, const GuideOptions& o,
                                        int stepTop) {
    const int radius = o.badgeDiameter / 2;
    return out.Pixel(o.padding, stepTop + radius * 3 / 4);
}

}  // namespace

CRISP_TEST(GuideCompose, Tek_adim_boyutu) {
    std::vector<Image> steps;
    steps.push_back(MakeFlat(300, 200, kRed));
    const GuideOptions o;
    Image out;
    CHECK(ComposeGuide(steps, o, out));
    CHECK_EQ(out.Width(), 300 + 2 * o.padding);
    CHECK_EQ(out.Height(), 200 + 2 * o.padding);
    // İçerik yerinde ve olduğu gibi.
    CHECK_EQ(out.Pixel(o.padding + 150, o.padding + 100), kRed);
}

CRISP_TEST(GuideCompose, Iki_ve_uc_adim_boyutu) {
    const GuideOptions o;
    {
        std::vector<Image> steps;
        steps.push_back(MakeFlat(300, 200, kRed));
        steps.push_back(MakeFlat(120, 80, kGreen));
        Image out;
        CHECK(ComposeGuide(steps, o, out));
        CHECK_EQ(out.Width(), 300 + 2 * o.padding);
        CHECK_EQ(out.Height(), 2 * o.padding + 200 + o.gap + 80);
        // İkinci adım sol kenara yaslı, altta.
        CHECK_EQ(out.Pixel(o.padding + 60, o.padding + 200 + o.gap + 40), kGreen);
        // Dar adımın sağındaki alan arka plan.
        CHECK_EQ(out.Pixel(o.padding + 200, o.padding + 200 + o.gap + 40),
                 o.background);
    }
    {
        Image out;
        CHECK(ComposeGuide(ThreeSteps(), o, out));
        CHECK_EQ(out.Width(), 300 + 2 * o.padding);
        CHECK_EQ(out.Height(), 2 * o.padding + 3 * 200 + 2 * o.gap);
        const int third = o.padding + 2 * (200 + o.gap);
        CHECK_EQ(out.Pixel(o.padding + 150, third + 100), kBlue);
    }
}

CRISP_TEST(GuideCompose, Genis_adim_kucultulur_dar_adim_buyutulmez) {
    const GuideOptions o;
    {
        std::vector<Image> steps;
        steps.push_back(MakeFlat(3000, 600, kRed));
        Image out;
        CHECK(ComposeGuide(steps, o, out));
        CHECK_EQ(out.Width(), o.maxWidth);
        // 600 · 1552 / 3000 = 310.4 → 310.
        const int inner = o.maxWidth - 2 * o.padding;
        CHECK_EQ(out.Height(), 2 * o.padding + 310);
        CHECK_EQ(out.Pixel(o.padding + inner / 2, o.padding + 155), kRed);
    }
    {
        std::vector<Image> steps;
        steps.push_back(MakeFlat(100, 50, kGreen));
        Image out;
        CHECK(ComposeGuide(steps, o, out));
        CHECK_EQ(out.Width(), 100 + 2 * o.padding);
        CHECK_EQ(out.Height(), 50 + 2 * o.padding);
    }
}

CRISP_TEST(GuideCompose, Kenar_bosluklari_arka_plan) {
    const GuideOptions o;
    Image out;
    CHECK(ComposeGuide(ThreeSteps(), o, out));
    // Köşeler rozetin dışında kalır (rozet merkezden 22 piksel, köşe 34).
    CHECK_EQ(out.Pixel(0, 0), o.background);
    CHECK_EQ(out.Pixel(out.Width() - 1, 0), o.background);
    CHECK_EQ(out.Pixel(0, out.Height() - 1), o.background);
    CHECK_EQ(out.Pixel(out.Width() - 1, out.Height() - 1), o.background);
    // Adımlar arası boşluğun ortası da arka plan; çerçeve boşluğun ilk ve son
    // pikselinde, ortasında değil. SOL KENARDAN UZAK bakılır: bir sonraki
    // adımın rozeti köşesinden yukarı, boşluğun içine taşar.
    const int gapMiddle = o.padding + 200 + o.gap / 2;
    CHECK_EQ(out.Pixel(o.padding + 150, gapMiddle), o.background);
    CHECK_EQ(out.Pixel(o.padding + 290, gapMiddle), o.background);
    CHECK_EQ(out.Pixel(o.padding + 150, gapMiddle + o.gap / 4), o.background);
}

CRISP_TEST(GuideCompose, Cerceve_ve_rozet_cizilir) {
    const GuideOptions o;
    Image out;
    CHECK(ComposeGuide(ThreeSteps(), o, out));
    for (int i = 0; i < 3; ++i) {
        const int top = o.padding + i * (200 + o.gap);
        // Rozet rengi her adımın sol-üst köşesinde.
        CHECK_EQ(BelowBadgeCentre(out, o, top), o.badgeColor);
        // Çerçeve görüntünün hemen dışında: alt kenarın ortası ne görüntü
        // rengi ne arka plan.
        const uint32_t frame = out.Pixel(o.padding + 150, top + 200);
        CHECK(frame != o.background);
        CHECK(frame != kRed && frame != kGreen && frame != kBlue);
        // Görüntünün kendi kenar pikseli dokunulmamış.
        CHECK(out.Pixel(o.padding + 150, top + 199) != frame);
    }

    // Çerçevesiz istek: aynı nokta arka plan.
    GuideOptions plain;
    plain.drawFrame = false;
    Image bare;
    CHECK(ComposeGuide(ThreeSteps(), plain, bare));
    CHECK_EQ(bare.Pixel(plain.padding + 150, plain.padding + 200), plain.background);
}

CRISP_TEST(GuideCompose, Numara_rozeti_boyar_ve_opak_kalir) {
    // 12 adım: iki haneli numara da rozete sığmalı ve görünür olmalı.
    std::vector<Image> steps;
    for (int i = 0; i < 12; ++i) {
        steps.push_back(MakeFlat(60, 20, kRed));
    }
    const GuideOptions o;
    Image out;
    CHECK(ComposeGuide(steps, o, out));

    const int radius = o.badgeDiameter / 2;
    for (int i = 0; i < 12; ++i) {
        const int cy = o.padding + i * (20 + o.gap);
        // Dairenin iç bölgesinde rozet renginden farklı en az bir piksel var:
        // numara çizilmiş. Yalnızca dairenin içi taranıyor, kenar yumuşatması
        // sayılmasın diye yarıçapın yarısı.
        bool painted = false;
        for (int y = cy - radius / 2; y <= cy + radius / 2 && !painted; ++y) {
            for (int x = o.padding - radius / 2; x <= o.padding + radius / 2; ++x) {
                if (out.Pixel(x, y) != o.badgeColor) {
                    painted = true;
                    break;
                }
            }
        }
        CHECK(painted);
    }

    // GDI metni alfayı sıfırlar; düzeltme her pikseli opak bırakmalı.
    bool opaque = true;
    for (int y = 0; y < out.Height() && opaque; ++y) {
        for (int x = 0; x < out.Width(); ++x) {
            if ((out.Pixel(x, y) >> 24) != 0xFFu) {
                opaque = false;
                break;
            }
        }
    }
    CHECK(opaque);
}

CRISP_TEST(GuideCompose, Bos_ve_gecersiz_girdi_reddedilir) {
    Image out;
    const std::vector<Image> none;
    CHECK(!ComposeGuide(none, out));
    CHECK(!out.Valid());

    std::vector<Image> broken;
    broken.push_back(MakeFlat(100, 100, kRed));
    broken.emplace_back();   // geçersiz görüntü
    broken.push_back(MakeFlat(100, 100, kBlue));
    CHECK(!ComposeGuide(broken, out));
    CHECK(!out.Valid());

    // Anlamsız seçenekler de reddedilir: içerik alanı kalmıyor.
    GuideOptions tight;
    tight.maxWidth = 40;
    tight.padding = 20;
    std::vector<Image> one;
    one.push_back(MakeFlat(10, 10, kRed));
    CHECK(!ComposeGuide(one, tight, out));
}

CRISP_TEST(GuideCompose, Sinira_sigmayan_kuyruk_atilir) {
    // Dört adım × 10000 satır kMaxImageSide'ı aşar; üçü sığar.
    std::vector<Image> steps;
    for (int i = 0; i < 4; ++i) {
        steps.push_back(MakeFlat(10, 10000, kRed));
    }
    const GuideOptions o;
    Image out;
    size_t placed = 0;
    CHECK(ComposeGuide(steps, o, out, &placed));
    CHECK_EQ(placed, 3);
    CHECK_EQ(out.Height(), 2 * o.padding + 3 * 10000 + 2 * o.gap);
    CHECK(out.Height() <= kMaxImageSide);

    // Tek adım bile sığmıyorsa hiçbir şey yerleşmez ve false döner.
    std::vector<Image> tall;
    tall.push_back(MakeFlat(10, kMaxImageSide, kRed));
    Image none;
    CHECK(!ComposeGuide(tall, o, none, &placed));
    CHECK_EQ(placed, 0);
}
