// ColorFormat.cpp — Rengin panoya gidecek metin biçimleri; bkz. ColorSpace.h.
//
// AYRI DOSYA: ColorSpace.cpp dönüşüm matematiğini taşır ve biçimlendirmeyle
// birlikte 400 satır sınırını aşıyordu. Buradaki hiçbir fonksiyon yeni bir
// dönüşüm yapmaz; yalnızca var olanları metne döker.
#include "ColorSpace.h"

#include <cstdio>
#include <cwchar>

namespace crisp {
namespace {

// Ayar dosyasında geçen kimlikler; sıra ColorFormat ile birebir.
constexpr const wchar_t* kFormatIds[] = {L"hex", L"rgb", L"hsl", L"cssvar",
                                         L"tailwind"};

// Yarıya kadar yukarı yuvarlar: 55.5 → 56. static_cast tek başına aşağı
// keserdi ve "%56" beklenen yerde "%55" çıkardı.
[[nodiscard]] int RoundHalfUp(double value) noexcept {
    return static_cast<int>(value + 0.5);
}

[[nodiscard]] std::wstring FormatRgbText(COLORREF color) {
    wchar_t text[32];
    ::swprintf_s(text, L"rgb(%u, %u, %u)",
                 static_cast<unsigned>(GetRValue(color)),
                 static_cast<unsigned>(GetGValue(color)),
                 static_cast<unsigned>(GetBValue(color)));
    return std::wstring(text);
}

[[nodiscard]] std::wstring FormatHslText(COLORREF color) {
    const Hsl hsl = RgbToHsl(color);
    // 359.6° yuvarlanınca 360 olur; CSS'te 360 geçerli olsa da çıktı 0..359
    // vaat ediyor ve iki farklı yazımı aynı renk için üretmek kafa karıştırır.
    const int hue = RoundHalfUp(hsl.hue) % 360;
    wchar_t text[32];
    ::swprintf_s(text, L"hsl(%d, %d%%, %d%%)", hue,
                 RoundHalfUp(hsl.saturation * 100.0),
                 RoundHalfUp(hsl.lightness * 100.0));
    return std::wstring(text);
}

[[nodiscard]] std::wstring FormatCssVarText(COLORREF color) {
    // Değişken adı küçük harf (CSS geleneği), değer büyük harf (hex biçimiyle
    // aynı): "--color-1e90ff: #1E90FF;".
    wchar_t text[48];
    ::swprintf_s(text, L"--color-%02x%02x%02x: %s;", GetRValue(color),
                 GetGValue(color), GetBValue(color),
                 FormatHexColor(color).c_str());
    return std::wstring(text);
}

}  // namespace

ColorFormat ColorFormatFromId(const wchar_t* id) noexcept {
    if (id != nullptr) {
        for (size_t i = 0; i < sizeof(kFormatIds) / sizeof(kFormatIds[0]); ++i) {
            if (::wcscmp(id, kFormatIds[i]) == 0) {
                return static_cast<ColorFormat>(i);
            }
        }
    }
    return ColorFormat::Hex;
}

const wchar_t* ColorFormatId(ColorFormat format) noexcept {
    const size_t index = static_cast<size_t>(format);
    if (index >= sizeof(kFormatIds) / sizeof(kFormatIds[0])) {
        return kFormatIds[0];
    }
    return kFormatIds[index];
}

std::wstring FormatColor(COLORREF color, ColorFormat format) {
    switch (format) {
        case ColorFormat::Rgb:
            return FormatRgbText(color);
        case ColorFormat::Hsl:
            return FormatHslText(color);
        case ColorFormat::CssVar:
            return FormatCssVarText(color);
        case ColorFormat::Tailwind:
            return NearestTailwindName(color);
        case ColorFormat::Hex:
        default:
            return FormatHexColor(color);
    }
}

std::wstring FormatColorAll(COLORREF color) {
    std::wstring text = FormatHexColor(color);
    text += L"\r\n";
    text += FormatRgbText(color);
    text += L"\r\n";
    text += FormatHslText(color);
    text += L"\r\n";
    text += NearestTailwindName(color);
    return text;
}

}  // namespace crisp
