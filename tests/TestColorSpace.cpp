// TestColorSpace.cpp — Renk dönüşümleri ve hex ayrıştırma.
#include "TestFramework.h"

#include "ColorSpace.h"

#include <cwchar>

using namespace crisp;

namespace {

// Yuvarlama yüzünden gidiş-dönüş tam eşit çıkmayabilir; bir birimlik sapma
// gözle görülmez ve testin bunu hata sayması, gerçek bir hatayı gölgelerdi.
[[nodiscard]] bool NearColor(COLORREF a, COLORREF b) noexcept {
    auto close = [](int x, int y) { return x - y <= 1 && y - x <= 1; };
    return close(GetRValue(a), GetRValue(b)) && close(GetGValue(a), GetGValue(b)) &&
           close(GetBValue(a), GetBValue(b));
}

}  // namespace

CRISP_TEST(ColorSpace, Hsv_gidis_donusu_rengi_korur) {
    const COLORREF samples[] = {
        RGB(255, 0, 0),    RGB(0, 255, 0),   RGB(0, 0, 255),
        RGB(255, 255, 0),  RGB(0, 255, 255), RGB(255, 0, 255),
        RGB(30, 144, 255), RGB(18, 52, 86),  RGB(200, 120, 40)};
    for (const COLORREF sample : samples) {
        CHECK(NearColor(HsvToRgb(RgbToHsv(sample)), sample));
    }
}

CRISP_TEST(ColorSpace, Gri_tonlarin_doygunlugu_sifir) {
    for (const COLORREF grey : {RGB(0, 0, 0), RGB(128, 128, 128),
                                RGB(255, 255, 255)}) {
        const Hsv hsv = RgbToHsv(grey);
        CHECK(hsv.saturation == 0.0);
    }
    // Siyahın parlaklığı 0, beyazınki 1.
    CHECK(RgbToHsv(RGB(0, 0, 0)).value == 0.0);
    CHECK(RgbToHsv(RGB(255, 255, 255)).value == 1.0);
}

CRISP_TEST(ColorSpace, Ton_dereceleri_beklenen_yerde) {
    auto hue = [](COLORREF c) { return static_cast<int>(RgbToHsv(c).hue + 0.5); };
    CHECK_EQ(hue(RGB(255, 0, 0)), 0);
    CHECK_EQ(hue(RGB(255, 255, 0)), 60);
    CHECK_EQ(hue(RGB(0, 255, 0)), 120);
    CHECK_EQ(hue(RGB(0, 255, 255)), 180);
    CHECK_EQ(hue(RGB(0, 0, 255)), 240);
    CHECK_EQ(hue(RGB(255, 0, 255)), 300);
}

CRISP_TEST(ColorSpace, HsvToRgb_araligin_disini_kirpar) {
    // Kaydırıcı uçlara dayandığında ton 360'ı, doygunluk 1'i geçebilir;
    // kırpılmasaydı renk karta dışına taşıp siyaha dönerdi.
    CHECK(NearColor(HsvToRgb(Hsv{360.0, 1.0, 1.0}), RGB(255, 0, 0)));
    CHECK(NearColor(HsvToRgb(Hsv{-30.0, 1.0, 1.0}), RGB(255, 0, 128)));
    CHECK(NearColor(HsvToRgb(Hsv{200.0, 5.0, 5.0}), HsvToRgb(Hsv{200.0, 1.0, 1.0})));
    CHECK(NearColor(HsvToRgb(Hsv{200.0, -1.0, 0.5}), RGB(128, 128, 128)));
}

CRISP_TEST(ColorSpace, Hex_alti_ve_uc_hane) {
    COLORREF color = 0;
    CHECK(ParseHexColor(L"#1E90FF", color));
    CHECK_EQ(color, RGB(0x1E, 0x90, 0xFF));

    CHECK(ParseHexColor(L"1e90ff", color));
    CHECK_EQ(color, RGB(0x1E, 0x90, 0xFF));

    // Üç hane CSS kuralıyla ikilenir: #0f0 → #00ff00.
    CHECK(ParseHexColor(L"#0f0", color));
    CHECK_EQ(color, RGB(0, 255, 0));

    CHECK(ParseHexColor(L"  #abc  ", color));
    CHECK_EQ(color, RGB(0xAA, 0xBB, 0xCC));
}

CRISP_TEST(ColorSpace, Hex_gecersiz_girdi_ciktiya_dokunmaz) {
    COLORREF color = RGB(1, 2, 3);
    CHECK(!ParseHexColor(nullptr, color));
    CHECK(!ParseHexColor(L"", color));
    CHECK(!ParseHexColor(L"#12", color));
    CHECK(!ParseHexColor(L"#12345", color));
    CHECK(!ParseHexColor(L"#12345g", color));
    CHECK(!ParseHexColor(L"#1234567", color));
    // Kısmi girdi rengi sıfırlamamalı: kullanıcı her tuşta bir kez geçersiz
    // bir dize üretir ve alan kullanılamaz hâle gelirdi.
    CHECK_EQ(color, RGB(1, 2, 3));
}

CRISP_TEST(ColorSpace, Hex_bicimi_gidis_donusu) {
    const COLORREF color = RGB(0x0A, 0xB1, 0xC2);
    CHECK(FormatHexColor(color) == L"#0AB1C2");
    COLORREF parsed = 0;
    CHECK(ParseHexColor(FormatHexColor(color).c_str(), parsed));
    CHECK_EQ(parsed, color);
}

// ---------------------------------------------------------------------------
// HSL
// ---------------------------------------------------------------------------

CRISP_TEST(ColorSpace, Hsl_bilinen_degerler) {
    // Yüzde olarak yuvarlanmış doygunluk/açıklık; CSS'in gösterdiği sayılar.
    auto percent = [](double v) { return static_cast<int>(v * 100.0 + 0.5); };
    auto degrees = [](double v) { return static_cast<int>(v + 0.5); };

    const Hsl red = RgbToHsl(RGB(255, 0, 0));
    CHECK_EQ(degrees(red.hue), 0);
    CHECK_EQ(percent(red.saturation), 100);
    CHECK_EQ(percent(red.lightness), 50);

    const Hsl white = RgbToHsl(RGB(255, 255, 255));
    CHECK_EQ(degrees(white.hue), 0);
    CHECK_EQ(percent(white.saturation), 0);
    CHECK_EQ(percent(white.lightness), 100);

    const Hsl dodger = RgbToHsl(RGB(0x1E, 0x90, 0xFF));
    CHECK_EQ(degrees(dodger.hue), 210);
    CHECK_EQ(percent(dodger.saturation), 100);
    CHECK_EQ(percent(dodger.lightness), 56);

    // Siyah: açıklık 0, doygunluk 0 — sıfıra bölme yok.
    const Hsl black = RgbToHsl(RGB(0, 0, 0));
    CHECK(black.saturation == 0.0);
    CHECK(black.lightness == 0.0);
}

CRISP_TEST(ColorSpace, Hsl_gidis_donusu_kaba_izgarada) {
    // Her kanalda 15'er adım: 18³ = 5832 renk. Kayan nokta payı bir birimi
    // aşmamalı; aşarsa dönüşümün kendisi yanlıştır, yuvarlama değil.
    for (int r = 0; r < 256; r += 15) {
        for (int g = 0; g < 256; g += 15) {
            for (int b = 0; b < 256; b += 15) {
                const COLORREF sample = RGB(r, g, b);
                CHECK(NearColor(HslToRgb(RgbToHsl(sample)), sample));
            }
        }
    }
    // Uçlar da tam olarak korunmalı.
    CHECK_EQ(HslToRgb(RgbToHsl(RGB(255, 255, 255))), RGB(255, 255, 255));
    CHECK_EQ(HslToRgb(RgbToHsl(RGB(255, 0, 255))), RGB(255, 0, 255));
}

CRISP_TEST(ColorSpace, HslToRgb_araligin_disini_kirpar) {
    CHECK(NearColor(HslToRgb(Hsl{360.0, 1.0, 0.5}), RGB(255, 0, 0)));
    CHECK(NearColor(HslToRgb(Hsl{-120.0, 1.0, 0.5}), RGB(0, 0, 255)));
    CHECK(NearColor(HslToRgb(Hsl{200.0, 3.0, 0.5}), HslToRgb(Hsl{200.0, 1.0, 0.5})));
    CHECK(NearColor(HslToRgb(Hsl{200.0, 1.0, 7.0}), RGB(255, 255, 255)));
}

// ---------------------------------------------------------------------------
// Metin biçimleri
// ---------------------------------------------------------------------------

CRISP_TEST(ColorSpace, FormatColor_her_bicimde_tam_metin) {
    const COLORREF dodger = RGB(0x1E, 0x90, 0xFF);
    CHECK_STR(FormatColor(dodger, ColorFormat::Hex), L"#1E90FF");
    CHECK_STR(FormatColor(dodger, ColorFormat::Rgb), L"rgb(30, 144, 255)");
    CHECK_STR(FormatColor(dodger, ColorFormat::Hsl), L"hsl(210, 100%, 56%)");
    CHECK_STR(FormatColor(dodger, ColorFormat::CssVar),
              L"--color-1e90ff: #1E90FF;");
    CHECK_STR(FormatColor(RGB(0x0E, 0xA5, 0xE9), ColorFormat::Tailwind),
              L"sky-500");

    // Sıfır dolgusu ve gri: hsl'de ton ve doygunluk 0.
    CHECK_STR(FormatColor(RGB(0, 0, 0), ColorFormat::Hex), L"#000000");
    CHECK_STR(FormatColor(RGB(128, 128, 128), ColorFormat::Hsl),
              L"hsl(0, 0%, 50%)");
    CHECK_STR(FormatColor(RGB(0, 0, 0), ColorFormat::CssVar),
              L"--color-000000: #000000;");
    // 359.6° yuvarlanınca 360 olmamalı; çıktı 0..359.
    CHECK_STR(FormatColor(RGB(255, 0, 1), ColorFormat::Hsl),
              L"hsl(0, 100%, 50%)");
}

CRISP_TEST(ColorSpace, FormatColorAll_dort_satir) {
    const std::wstring all = FormatColorAll(RGB(0x0E, 0xA5, 0xE9));
    CHECK_STR(all, L"#0EA5E9\r\nrgb(14, 165, 233)\r\nhsl(199, 89%, 48%)\r\nsky-500");
}

CRISP_TEST(ColorSpace, ColorFormat_kimlik_gidis_donusu) {
    const ColorFormat all[] = {ColorFormat::Hex, ColorFormat::Rgb,
                               ColorFormat::Hsl, ColorFormat::CssVar,
                               ColorFormat::Tailwind};
    for (const ColorFormat format : all) {
        CHECK(ColorFormatFromId(ColorFormatId(format)) == format);
    }
    CHECK_STR(ColorFormatId(ColorFormat::Tailwind), L"tailwind");
    CHECK_STR(ColorFormatId(ColorFormat::CssVar), L"cssvar");

    // Bilinmeyen, boş ve nullptr → Hex.
    CHECK(ColorFormatFromId(L"cmyk") == ColorFormat::Hex);
    CHECK(ColorFormatFromId(L"") == ColorFormat::Hex);
    CHECK(ColorFormatFromId(nullptr) == ColorFormat::Hex);
    // Büyük/küçük harf ayrımı var: ayar zaten Clamp'te küçültülür.
    CHECK(ColorFormatFromId(L"RGB") == ColorFormat::Hex);
}

// ---------------------------------------------------------------------------
// Lab ve Tailwind
// ---------------------------------------------------------------------------

CRISP_TEST(ColorSpace, Lab_beyaz_ve_siyah) {
    auto within = [](double x, double y) { return x - y < 0.1 && y - x < 0.1; };
    const Lab white = RgbToLab(RGB(255, 255, 255));
    CHECK(within(white.l, 100.0));
    CHECK(within(white.a, 0.0));
    CHECK(within(white.b, 0.0));

    const Lab black = RgbToLab(RGB(0, 0, 0));
    CHECK(within(black.l, 0.0));
    CHECK(within(black.a, 0.0));
    CHECK(within(black.b, 0.0));

    // Kırmızı a ekseninde artı, mavi b ekseninde eksi tarafta — işaretler
    // ters olsaydı "en yakın" araması kırmızıyı yeşile eşlerdi.
    CHECK(RgbToLab(RGB(255, 0, 0)).a > 50.0);
    CHECK(RgbToLab(RGB(0, 0, 255)).b < -50.0);
    // Orta gri: a ≈ b ≈ 0, L yaklaşık 53.
    const Lab grey = RgbToLab(RGB(128, 128, 128));
    CHECK(within(grey.a, 0.0));
    CHECK(within(grey.b, 0.0));
    CHECK(grey.l > 52.0 && grey.l < 55.0);
}

CRISP_TEST(ColorSpace, Tailwind_tam_isabetler) {
    CHECK_STR(NearestTailwindName(RGB(0x0E, 0xA5, 0xE9)), L"sky-500");
    CHECK_STR(NearestTailwindName(RGB(0xEF, 0x44, 0x44)), L"red-500");
    CHECK_STR(NearestTailwindName(RGB(0xFF, 0xFF, 0xFF)), L"white");
    CHECK_STR(NearestTailwindName(RGB(0x00, 0x00, 0x00)), L"black");
    CHECK_STR(NearestTailwindName(RGB(0x02, 0x06, 0x17)), L"slate-950");
    CHECK_STR(NearestTailwindName(RGB(0x4C, 0x05, 0x19)), L"rose-950");

    // Eşleşen palet rengi de geri verilir.
    COLORREF matched = 0;
    CHECK_STR(NearestTailwindName(RGB(0x3B, 0x82, 0xF6), &matched), L"blue-500");
    CHECK_EQ(matched, RGB(0x3B, 0x82, 0xF6));

    // zinc-50 ile neutral-50 aynı hex: eşitlikte tablodaki ilk kazanır.
    CHECK_STR(NearestTailwindName(RGB(0xFA, 0xFA, 0xFA)), L"zinc-50");
}

CRISP_TEST(ColorSpace, Tailwind_yakin_kacirma) {
    COLORREF matched = 0;
    CHECK_STR(NearestTailwindName(RGB(0x0E, 0xA5, 0xEA), &matched), L"sky-500");
    CHECK_EQ(matched, RGB(0x0E, 0xA5, 0xE9));
    CHECK_STR(NearestTailwindName(RGB(0xF0, 0x45, 0x43)), L"red-500");
    CHECK_STR(NearestTailwindName(RGB(0xFE, 0xFE, 0xFE)), L"white");
    CHECK_STR(NearestTailwindName(RGB(0x01, 0x01, 0x01)), L"black");
}

CRISP_TEST(ColorSpace, Tailwind_paleti_tam_ve_adlar_benzersiz) {
    size_t count = 0;
    const TailwindColor* palette = TailwindPalette(count);
    CHECK(palette != nullptr);
    CHECK_EQ(count, 244);

    bool unique = true;
    for (size_t i = 0; i < count && unique; ++i) {
        CHECK(palette[i].name != nullptr && palette[i].name[0] != L'\0');
        for (size_t j = i + 1; j < count; ++j) {
            if (::wcscmp(palette[i].name, palette[j].name) == 0) {
                unique = false;
                break;
            }
        }
    }
    CHECK(unique);

    // Her palet rengi kendine en yakın olmalı (aynı hex'i paylaşanlar
    // dışında): tablo girdisi Lab'a çevrilip geri aranınca kendini bulur.
    for (size_t i = 0; i < count; ++i) {
        COLORREF matched = 0;
        (void)NearestTailwindName(palette[i].rgb, &matched);
        CHECK_EQ(matched, palette[i].rgb);
    }
}

CRISP_TEST(ColorSpace, Okunakli_murekkep_secimi) {
    CHECK(PrefersDarkInk(RGB(255, 255, 255)));
    CHECK(PrefersDarkInk(RGB(255, 214, 10)));    // sarı: üstüne siyah yazılır
    CHECK(!PrefersDarkInk(RGB(0, 0, 0)));
    CHECK(!PrefersDarkInk(RGB(10, 132, 255)));   // mavi: üstüne beyaz yazılır
}

CRISP_TEST(ColorSpace, Kontrast_ayirt_edilebilirligi_olcer) {
    const COLORREF darkSurface = RGB(32, 32, 35);
    const COLORREF accent = RGB(10, 132, 255);
    const COLORREF red = RGB(255, 59, 48);

    // Kırmızı koyu zeminde okunur — "ikisi de koyu" diyen eski kural bunu
    // reddediyordu ve kalınlık listesi kullanıcının rengini göstermiyordu.
    CHECK(HasContrast(red, darkSurface));
    // Vurgu mavisiyle aynı kırmızı ayırt edilemez.
    CHECK(!HasContrast(red, accent));
    // Uç durumlar.
    CHECK(HasContrast(RGB(255, 255, 255), RGB(0, 0, 0)));
    CHECK(!HasContrast(RGB(70, 70, 70), RGB(75, 75, 75)));
    // Simetrik olmalı: sıranın önemi yok.
    CHECK(HasContrast(darkSurface, red) == HasContrast(red, darkSurface));
}
