// GuideDraw.cpp — Kılavuz görüntüsünün süsü: çerçeve, rozet, numara.
// Yerleşim GuideCompose.cpp'de; bkz. GuideInternal.h.
#include "GuideInternal.h"

#include "Util.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>

namespace crisp::guide {
namespace {

// Util.h'de yazı tipi için sahiplik tipi yok — GDI yazı tipleri bu depoda
// pencere kodunda yaşıyor ve orada ömürleri pencereyle birlikte yönetiliyor.
// Buradaki tek kullanım için küçük bir sarmalayıcı yeter.
class scoped_font {
public:
    explicit scoped_font(HFONT font) noexcept : m_font(font) {}
    ~scoped_font() {
        if (m_font != nullptr) {
            ::DeleteObject(m_font);
        }
    }
    scoped_font(const scoped_font&) = delete;
    scoped_font& operator=(const scoped_font&) = delete;

    [[nodiscard]] HFONT get() const noexcept { return m_font; }

private:
    HFONT m_font;
};

[[nodiscard]] HFONT CreateBadgeFont(int pixelHeight) noexcept {
    LOGFONTW font{};
    // Negatif yükseklik "karakter yüksekliği" demek — iç boşluk hariç. Böylece
    // 24 istendiğinde rakamlar gerçekten 24 piksele yakın çıkar.
    font.lfHeight = -(std::max)(1, pixelHeight);
    font.lfWeight = FW_BOLD;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfOutPrecision = OUT_TT_PRECIS;
    // GRİ TONLU YUMUŞATMA, ClearType değil: ClearType alt piksel renkleri
    // beyaz zeminde görünmez ama kırmızı rozet üzerinde renkli saçak bırakır
    // ve PNG olarak kaydedilen görüntüde kalıcı olur.
    font.lfQuality = ANTIALIASED_QUALITY;
    font.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

[[nodiscard]] uint32_t Blend(uint32_t under, uint32_t over, int coverage,
                             int total) noexcept {
    uint32_t result = 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8) {
        const int a = static_cast<int>((under >> shift) & 0xFFu);
        const int b = static_cast<int>((over >> shift) & 0xFFu);
        const int mixed = (a * (total - coverage) + b * coverage + total / 2) / total;
        result |= static_cast<uint32_t>(mixed) << shift;
    }
    return result;
}

// Rozet dikdörtgeni: merkez ± yarıçap, tuvale kırpılmış.
[[nodiscard]] RECT BadgeRect(POINT centre, int diameter, const Image& out) noexcept {
    const int radius = diameter / 2;
    RECT rect{};
    rect.left = (std::max)(0L, centre.x - radius);
    rect.top = (std::max)(0L, centre.y - radius);
    rect.right = (std::min)(static_cast<LONG>(out.Width()), centre.x + radius + 1);
    rect.bottom = (std::min)(static_cast<LONG>(out.Height()), centre.y + radius + 1);
    return rect;
}

// GDI metin çıkışı 32 bpp hedefte alfa baytını 0 bırakır (bkz. Capture.cpp'de
// BitBlt için aynı not). PNG'ye giden görüntüde numaralar saydam delik olurdu.
void ForceOpaque(Image& out, const RECT& rect) noexcept {
    auto* bits = static_cast<uint8_t*>(out.Bits());
    for (LONG y = rect.top; y < rect.bottom; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(
            bits + static_cast<size_t>(y) * out.Stride());
        for (LONG x = rect.left; x < rect.right; ++x) {
            row[x] |= 0xFF000000u;
        }
    }
}

}  // namespace

void DrawFrame(Image& out, const Placement& at, uint32_t color) noexcept {
    // SetPixel sınır dışını yok sayar; kenar boşluğu sıfırsa çerçeve sessizce
    // düşer ve bu doğru — görüntünün üstüne çizmek daha kötü.
    for (int x = at.x - 1; x <= at.x + at.width; ++x) {
        out.SetPixel(x, at.y - 1, color);
        out.SetPixel(x, at.y + at.height, color);
    }
    for (int y = at.y; y < at.y + at.height; ++y) {
        out.SetPixel(at.x - 1, y, color);
        out.SetPixel(at.x + at.width, y, color);
    }
}

POINT BadgeCentre(const Placement& at, int diameter) noexcept {
    const int radius = diameter / 2;
    return POINT{(std::max)(at.x, radius), (std::max)(at.y, radius)};
}

void DrawBadgeDisk(Image& out, POINT centre, int diameter, uint32_t color) noexcept {
    if (!out.Valid() || diameter <= 0) {
        return;
    }
    // 4×4 alt örnekleme: kenar pikseli, dairenin kapladığı alt örnek sayısı
    // kadar boyanır. Tam analitik kapsama hesabından basit ve 16 seviye,
    // 44 piksellik bir dairede tırtığı gözden silmeye yetiyor.
    constexpr int kSub = 4;
    constexpr int kTotal = kSub * kSub;
    const double radius = diameter / 2.0;
    const double radiusSq = radius * radius;
    const RECT rect = BadgeRect(centre, diameter, out);

    for (LONG y = rect.top; y < rect.bottom; ++y) {
        for (LONG x = rect.left; x < rect.right; ++x) {
            int inside = 0;
            for (int sy = 0; sy < kSub; ++sy) {
                const double dy = (y + (sy + 0.5) / kSub) - centre.y - 0.5;
                for (int sx = 0; sx < kSub; ++sx) {
                    const double dx = (x + (sx + 0.5) / kSub) - centre.x - 0.5;
                    if (dx * dx + dy * dy <= radiusSq) {
                        ++inside;
                    }
                }
            }
            if (inside == kTotal) {
                out.SetPixel(x, y, color);
            } else if (inside > 0) {
                out.SetPixel(x, y, Blend(out.Pixel(x, y), color, inside, kTotal));
            }
        }
    }
}

bool DrawBadgeNumbers(Image& out, const std::vector<POINT>& centres,
                      const GuideOptions& options) {
    if (!out.Valid() || options.badgeDiameter <= 0) {
        return false;
    }
    const int diameter = options.badgeDiameter;
    // Rakam yüksekliği çapın yarısından biraz fazla: tek haneli sayı rozeti
    // doldurur ama kenara değmez. İki ve üç haneliler aşağıda daraltılır.
    const int baseHeight = (std::max)(1, diameter * 55 / 100);
    // Metin dairenin içinde kalmalı; köşegen değil genişlik ölçüldüğü için
    // çapın %80'i güvenli sınır.
    const int maxTextWidth = (std::max)(1, diameter * 80 / 100);

    {
        const unique_hdc dc{::CreateCompatibleDC(nullptr)};
        if (!dc) {
            return false;
        }
        const dc_selection target{dc.get(), out.Handle()};
        const scoped_font baseFont{CreateBadgeFont(baseHeight)};
        if (baseFont.get() == nullptr) {
            return false;
        }
        ::SetBkMode(dc.get(), TRANSPARENT);
        const uint32_t text = options.badgeText;
        ::SetTextColor(dc.get(), RGB((text >> 16) & 0xFFu, (text >> 8) & 0xFFu,
                                     text & 0xFFu));

        for (size_t i = 0; i < centres.size(); ++i) {
            wchar_t label[16];
            ::swprintf_s(label, L"%u", static_cast<unsigned>(i + 1));
            const int length = static_cast<int>(::wcslen(label));

            const dc_selection useBase{dc.get(), baseFont.get()};
            SIZE extent{};
            if (!::GetTextExtentPoint32W(dc.get(), label, length, &extent)) {
                return false;
            }

            // Sığmayan numara için küçültülmüş yazı tipi; 10 ve üstü 44
            // piksellik rozette buraya düşer. Seçim kapsamı yazı tipinin
            // kapsamından DAR: DC'de seçili duran bir yazı tipi silinemez.
            HFONT shrunkFont = nullptr;
            if (extent.cx > maxTextWidth) {
                const int shrunk = (std::max)(
                    1, static_cast<int>(static_cast<long long>(baseHeight) *
                                        maxTextWidth / extent.cx));
                shrunkFont = CreateBadgeFont(shrunk);
            }
            const scoped_font smaller{shrunkFont};
            const dc_selection useSmaller{
                dc.get(), smaller.get() != nullptr ? smaller.get() : baseFont.get()};

            RECT rect = BadgeRect(centres[i], diameter, out);
            ::DrawTextW(dc.get(), label, length, &rect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        // DC KAPANMADAN ÖNCE: GDI çizimleri kuyrukta bekleyebilir ve alfa
        // düzeltmesi onlardan önce koşarsa numaralar yine delik kalır.
        ::GdiFlush();
    }

    for (const POINT& centre : centres) {
        ForceOpaque(out, BadgeRect(centre, diameter, out));
    }
    return true;
}

}  // namespace crisp::guide
