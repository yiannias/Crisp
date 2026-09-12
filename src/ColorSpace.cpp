// ColorSpace.cpp — bkz. ColorSpace.h.
#include "ColorSpace.h"

#include <cmath>
#include <cwctype>

namespace crisp {
namespace {

[[nodiscard]] int HexDigit(wchar_t ch) noexcept {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return ch - L'a' + 10;
    }
    if (ch >= L'A' && ch <= L'F') {
        return ch - L'A' + 10;
    }
    return -1;
}

[[nodiscard]] BYTE ToByte(double value) noexcept {
    const double scaled = value * 255.0 + 0.5;
    if (scaled <= 0.0) {
        return 0;
    }
    if (scaled >= 255.0) {
        return 255;
    }
    return static_cast<BYTE>(scaled);
}

}  // namespace

Hsv RgbToHsv(COLORREF color) noexcept {
    const double r = static_cast<double>(GetRValue(color)) / 255.0;
    const double g = static_cast<double>(GetGValue(color)) / 255.0;
    const double b = static_cast<double>(GetBValue(color)) / 255.0;

    const double high = (r > g ? (r > b ? r : b) : (g > b ? g : b));
    const double low = (r < g ? (r < b ? r : b) : (g < b ? g : b));
    const double span = high - low;

    Hsv hsv;
    hsv.value = high;
    hsv.saturation = high <= 0.0 ? 0.0 : span / high;

    // GRİ TONLARIN TONU YOKTUR ve 0 verilir. Hesaplamaya zorlamak sıfıra
    // bölme demek; kullanıcı açısından da siyahın "kırmızımsı" olması anlamsız.
    if (span <= 0.0) {
        hsv.hue = 0.0;
        return hsv;
    }

    double hue = 0.0;
    if (high == r) {
        hue = (g - b) / span;
    } else if (high == g) {
        hue = 2.0 + (b - r) / span;
    } else {
        hue = 4.0 + (r - g) / span;
    }
    hue *= 60.0;
    if (hue < 0.0) {
        hue += 360.0;
    }
    hsv.hue = hue;
    return hsv;
}

COLORREF HsvToRgb(const Hsv& hsv) noexcept {
    double hue = std::fmod(hsv.hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double saturation = hsv.saturation < 0.0
                                  ? 0.0
                                  : (hsv.saturation > 1.0 ? 1.0 : hsv.saturation);
    const double value =
        hsv.value < 0.0 ? 0.0 : (hsv.value > 1.0 ? 1.0 : hsv.value);

    const double sector = hue / 60.0;
    const int index = static_cast<int>(sector) % 6;
    const double fraction = sector - std::floor(sector);

    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * fraction);
    const double t = value * (1.0 - saturation * (1.0 - fraction));

    double r = value;
    double g = t;
    double b = p;
    switch (index) {
        case 0: r = value; g = t;     b = p;     break;
        case 1: r = q;     g = value; b = p;     break;
        case 2: r = p;     g = value; b = t;     break;
        case 3: r = p;     g = q;     b = value; break;
        case 4: r = t;     g = p;     b = value; break;
        default: r = value; g = p;    b = q;     break;
    }
    return RGB(ToByte(r), ToByte(g), ToByte(b));
}

Hsl RgbToHsl(COLORREF color) noexcept {
    const double r = static_cast<double>(GetRValue(color)) / 255.0;
    const double g = static_cast<double>(GetGValue(color)) / 255.0;
    const double b = static_cast<double>(GetBValue(color)) / 255.0;

    const double high = (r > g ? (r > b ? r : b) : (g > b ? g : b));
    const double low = (r < g ? (r < b ? r : b) : (g < b ? g : b));
    const double span = high - low;

    Hsl hsl;
    hsl.lightness = (high + low) / 2.0;
    if (span <= 0.0) {
        // Gri: ton yok, doygunluk yok (bkz. RgbToHsv).
        return hsl;
    }

    // Doygunluk paydası açıklığa göre değişir: orta gri civarında 1, uçlara
    // doğru sıfıra iner. Payda tam sıfır olamaz çünkü span > 0 iken açıklık
    // ne 0 ne 1'dir.
    const double denominator = 1.0 - std::fabs(2.0 * hsl.lightness - 1.0);
    hsl.saturation = span / denominator;
    if (hsl.saturation > 1.0) {
        hsl.saturation = 1.0;   // kayan nokta payı
    }

    // Ton hesabı HSV ile birebir aynı; iki model aynı çarkı paylaşır.
    hsl.hue = RgbToHsv(color).hue;
    return hsl;
}

COLORREF HslToRgb(const Hsl& hsl) noexcept {
    double hue = std::fmod(hsl.hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double saturation = hsl.saturation < 0.0
                                  ? 0.0
                                  : (hsl.saturation > 1.0 ? 1.0 : hsl.saturation);
    const double lightness =
        hsl.lightness < 0.0 ? 0.0 : (hsl.lightness > 1.0 ? 1.0 : hsl.lightness);

    // Kroma (en yüksek ile en düşük kanal farkı), ara kanal ve taban.
    const double chroma = (1.0 - std::fabs(2.0 * lightness - 1.0)) * saturation;
    const double sector = hue / 60.0;
    const double middle =
        chroma * (1.0 - std::fabs(std::fmod(sector, 2.0) - 1.0));
    const double base = lightness - chroma / 2.0;

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    switch (static_cast<int>(sector) % 6) {
        case 0: r = chroma; g = middle; break;
        case 1: r = middle; g = chroma; break;
        case 2: g = chroma; b = middle; break;
        case 3: g = middle; b = chroma; break;
        case 4: r = middle; b = chroma; break;
        default: r = chroma; b = middle; break;
    }
    return RGB(ToByte(r + base), ToByte(g + base), ToByte(b + base));
}

Lab RgbToLab(COLORREF color) noexcept {
    // sRGB aktarım eğrisi: 8 bitlik değer doğrusal ışığa çevrilir. Gamma'yı
    // atlamak koyu tonları olduğundan çok daha ayrık, açıkları çok daha yakın
    // gösterir ve "en yakın renk" hep açık tonlara kayar.
    auto linear = [](BYTE channel) noexcept {
        const double c = static_cast<double>(channel) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    const double r = linear(GetRValue(color));
    const double g = linear(GetGValue(color));
    const double b = linear(GetBValue(color));

    // sRGB → XYZ (D65). Satır toplamları tam olarak D65 beyazını verir, bu
    // yüzden beyaz için a = b = 0 çıkar; test bunu doğrular.
    const double x = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
    const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
    const double z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;

    constexpr double kWhiteX = 0.95047;
    constexpr double kWhiteY = 1.0;
    constexpr double kWhiteZ = 1.08883;

    // Küp kök, sıfıra yakın bölgede doğrusal parçayla yumuşatılır (CIE
    // tanımı); aksi hâlde siyah civarında türev sonsuza giderdi.
    auto f = [](double t) noexcept {
        constexpr double kEpsilon = 216.0 / 24389.0;   // (6/29)^3
        constexpr double kKappa = 24389.0 / 27.0;      // (29/3)^3
        return t > kEpsilon ? std::cbrt(t) : (kKappa * t + 16.0) / 116.0;
    };
    const double fx = f(x / kWhiteX);
    const double fy = f(y / kWhiteY);
    const double fz = f(z / kWhiteZ);

    Lab lab;
    lab.l = 116.0 * fy - 16.0;
    lab.a = 500.0 * (fx - fy);
    lab.b = 200.0 * (fy - fz);
    return lab;
}

bool ParseHexColor(const wchar_t* text, COLORREF& out) noexcept {
    if (text == nullptr) {
        return false;
    }
    while (*text == L' ' || *text == L'\t' || *text == L'#') {
        ++text;
    }

    int digits[6] = {0, 0, 0, 0, 0, 0};
    int count = 0;
    while (count < 6 && text[count] != L'\0') {
        const int digit = HexDigit(text[count]);
        if (digit < 0) {
            break;   // hane bitti; kalanı aşağıdaki denetim inceler
        }
        digits[count] = digit;
        ++count;
    }
    // Kalanı boşluk olabilir ama başka bir şey OLAMAZ: "#1e90ffzz" kabul
    // edilseydi kullanıcı yazım hatasını hiç fark etmezdi.
    for (const wchar_t* rest = text + count; *rest != L'\0'; ++rest) {
        if (*rest != L' ' && *rest != L'\t') {
            return false;
        }
    }

    if (count == 3) {
        out = RGB(digits[0] * 17, digits[1] * 17, digits[2] * 17);
        return true;
    }
    if (count == 6) {
        out = RGB(digits[0] * 16 + digits[1], digits[2] * 16 + digits[3],
                  digits[4] * 16 + digits[5]);
        return true;
    }
    return false;
}

std::wstring FormatHexColor(COLORREF color) {
    wchar_t text[8];
    ::swprintf_s(text, L"#%02X%02X%02X", GetRValue(color), GetGValue(color),
                 GetBValue(color));
    return std::wstring(text);
}

int RelativeLuma(COLORREF color) noexcept {
    return (299 * GetRValue(color) + 587 * GetGValue(color) +
            114 * GetBValue(color)) /
           1000;
}

bool PrefersDarkInk(COLORREF background) noexcept {
    return RelativeLuma(background) > 140;
}

bool HasContrast(COLORREF a, COLORREF b) noexcept {
    // 55 EŞİĞİ DENEYEREK BULUNDU: koyu arayüz zemini (~32) ile kırmızı (116)
    // arasındaki fark 84 ve gözle rahat ayrılıyor; vurgu mavisi (109) ile aynı
    // kırmızı arasındaki 7 ise ayrılmıyor.
    const int difference = RelativeLuma(a) - RelativeLuma(b);
    return (difference < 0 ? -difference : difference) >= 55;
}

}  // namespace crisp
