// ColorSpace.h — Renk dönüşümleri ve metin biçimleri.
//
// PENCERE YOK: renk seçicinin matematiği buradadır ve çekirdekte durur, çünkü
// "ton şeridinin ucu tam kırmızıya dönüyor mu" ya da "#0f0 üç haneli hâliyle
// okunuyor mu" gibi sorular pencere açmadan sınanabilmeli. Seçicinin kendisi
// (ColorPicker.cpp) yalnızca bu fonksiyonları çizime bağlar.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Ton 0..360 (derece), doygunluk ve parlaklık 0..1.
//
// TON DERECE OLARAK: renk çarkı bir açıdır ve 0..1'e sıkıştırmak, kullanıcıya
// gösterilecek "210°" gibi bir değeri her seferinde geri çarpmak demekti.
struct Hsv {
    double hue = 0.0;
    double saturation = 0.0;
    double value = 0.0;
};

[[nodiscard]] Hsv RgbToHsv(COLORREF color) noexcept;
[[nodiscard]] COLORREF HsvToRgb(const Hsv& hsv) noexcept;

// CSS'in konuştuğu HSL: ton derece, doygunluk ve açıklık 0..1 — Hsv ile aynı
// birimler, ki iki yapı arasında geçiş yaparken çarpan hatırlamak gerekmesin.
//
// HSV'NİN YANINA NEDEN İKİNCİSİ: seçici çarkı HSV ile çizer ama tasarımcılar
// "hsl(210, 100%, 56%)" yazar; ikisi aynı ton eksenini paylaşsa da doygunluk
// ve açıklık farklı hesaplanır ve birinden diğerine çevirmek yeni bir
// dönüşümden ucuz değildir.
struct Hsl {
    double hue = 0.0;
    double saturation = 0.0;
    double lightness = 0.0;
};

[[nodiscard]] Hsl RgbToHsl(COLORREF color) noexcept;
[[nodiscard]] COLORREF HslToRgb(const Hsl& hsl) noexcept;

// CIE L*a*b* (D65 beyaz noktası, sRGB → doğrusal → XYZ yolu). L 0..100,
// a ve b kabaca -128..127.
//
// NEDEN LAB: "en yakın palet rengi" sorusu RGB uzaklığıyla yanlış cevap verir
// — gözün pek ayırt etmediği iki koyu mavi RGB'de uzak, sarı ile sarımsı iki
// ton ise yakın çıkar. Lab'daki Öklid uzaklığı (ΔE76) algıya yeterince yakın
// ve tek formüldür.
struct Lab {
    double l = 0.0;
    double a = 0.0;
    double b = 0.0;
};

[[nodiscard]] Lab RgbToLab(COLORREF color) noexcept;

// Panoya kopyalanacak metnin biçimi; Settings::colorFormat kimliğiyle eşleşir.
enum class ColorFormat { Hex, Rgb, Hsl, CssVar, Tailwind };

// Bilinmeyen ya da boş kimlik Hex'tir: ayar dosyası elle bozulduğunda
// kullanıcı yine de bir renk kopyalayabilmeli.
[[nodiscard]] ColorFormat ColorFormatFromId(const wchar_t* id) noexcept;
[[nodiscard]] const wchar_t* ColorFormatId(ColorFormat format) noexcept;

// Hex      → "#1E90FF"
// Rgb      → "rgb(30, 144, 255)"
// Hsl      → "hsl(210, 100%, 56%)"   (tam sayılar; ton 0..359)
// CssVar   → "--color-1e90ff: #1E90FF;"
// Tailwind → "sky-500"               (en yakın palet girdisi)
[[nodiscard]] std::wstring FormatColor(COLORREF color, ColorFormat format);

// Hex, rgb, hsl ve Tailwind adı — her biri kendi satırında (CRLF; Windows
// panosuna gidecek metin için doğal satır sonu).
[[nodiscard]] std::wstring FormatColorAll(COLORREF color);

// Tailwind CSS v3.4 varsayılan paleti: 22 renk × 11 ton + black + white.
struct TailwindColor {
    const wchar_t* name;
    COLORREF rgb;
};

[[nodiscard]] const TailwindColor* TailwindPalette(size_t& count) noexcept;

// Lab uzaklığı en küçük olan palet girdisinin adı; eşitlikte tablodaki ilk
// kazanır. `matched` verilirse girdinin kendi rengi de yazılır.
[[nodiscard]] std::wstring NearestTailwindName(COLORREF color,
                                               COLORREF* matched = nullptr);

// "#1E90FF", "1e90ff", "#0f0" ve "0f0" kabul edilir. Üç hane, her hanenin
// ikilenmesidir (CSS kuralı): #0f0 → #00ff00.
//
// BAŞARISIZLIKTA out'a DOKUNULMAZ: kullanıcı hex alanına yazarken her tuş
// sonrası dize geçersiz olur ve her yarım girdi rengi sıfırlasaydı alan
// kullanılamazdı.
[[nodiscard]] bool ParseHexColor(const wchar_t* text, COLORREF& out) noexcept;

// Daima "#RRGGBB", büyük harf.
[[nodiscard]] std::wstring FormatHexColor(COLORREF color);

// Algısal parlaklık 0..255 (ITU-R BT.601 ağırlıkları).
[[nodiscard]] int RelativeLuma(COLORREF color) noexcept;

// Bu rengin ÜSTÜNE yazılacak metin koyu mu olmalı? Sarı bir örneğin üstüne
// beyaz yazmak onu okunmaz yapar; kararı gözle vermek yerine parlaklık
// hesaplanır.
[[nodiscard]] bool PrefersDarkInk(COLORREF background) noexcept;

// İki renk birbirinden AYIRT EDİLEBİLİR mi?
//
// "İkisi de koyu mu" diye sormak yetmez: koyu bir arayüz zemininde kırmızı
// (parlaklık 116) gayet okunur ama ikisi de "koyu" sayıldığı için reddedilirdi
// — kalınlık listesindeki önizleme çizgisi tam olarak bu yüzden kullanıcının
// seçtiği rengi değil beyazı gösteriyordu.
[[nodiscard]] bool HasContrast(COLORREF a, COLORREF b) noexcept;

}  // namespace crisp
