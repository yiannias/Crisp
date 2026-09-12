// GuideCompose.cpp — bkz. GuideCompose.h. Yerleşim ve birleştirme; piksel
// işi GuideDraw.cpp'de.
#include "GuideCompose.h"

#include "GuideInternal.h"
#include "ImageTransform.h"

#include <algorithm>
#include <cstring>

namespace crisp {
namespace {

// Çerçeve rengi seçeneklere konmadı: her çağıranın "çerçeve olsun mu" diye
// sorması anlamlı, "hangi gri" diye sorması değil. Açık bir gri, beyaz ve
// koyu zeminin ikisinde de görünür ama görüntüyle yarışmaz.
constexpr uint32_t kFrameColor = 0xFFC8C8C8u;

[[nodiscard]] bool OptionsSane(const GuideOptions& options) noexcept {
    if (options.padding < 0 || options.gap < 0 || options.badgeDiameter < 0) {
        return false;
    }
    // Rozet ve çerçeve kenar boşluğuna çizilir; içerik alanı en az bir
    // piksel olmalı, yoksa her adım sıfıra ölçeklenirdi.
    return options.maxWidth - 2 * options.padding >= 1;
}

// Adımın küçültülmüş boyutu. YALNIZCA KÜÇÜLTÜR: dar bir görüntüyü sütun
// genişliğine büyütmek, resampler'ın en iyi hâlinde bile metni yumuşatır.
[[nodiscard]] SIZE FittedSize(const Image& step, int innerWidth) noexcept {
    const int width = step.Width();
    const int height = step.Height();
    if (width <= innerWidth) {
        return SIZE{width, height};
    }
    // Oran korunur; yuvarlama "en az 1" ile sınırlanır — 3000×1'lik bir şerit
    // küçültüldüğünde sıfır satırlık görüntü yaratmaya çalışmamalı.
    const long long scaled = (static_cast<long long>(height) * innerWidth +
                              width / 2) / width;
    return SIZE{innerWidth, static_cast<int>((std::max)(1LL, scaled))};
}

// Adımı yerine kopyalar; gerekiyorsa önce küçültür. Satır satır memcpy: iki
// DIB arasında BitBlt de olurdu ama ikinci bir DC ve alfa düzeltmesi
// gerektirir, ve burada her piksel zaten tam olarak bilinir.
[[nodiscard]] bool PlaceStep(const Image& step, const guide::Placement& at,
                             Image& out) {
    Image scaled;
    const Image* source = &step;
    if (at.width != step.Width() || at.height != step.Height()) {
        if (!ScaleImage(step, at.width, at.height, scaled)) {
            return false;
        }
        source = &scaled;
    }

    const auto* from = static_cast<const uint8_t*>(source->Bits());
    auto* to = static_cast<uint8_t*>(out.Bits());
    if (from == nullptr || to == nullptr) {
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(at.width) * 4u;
    for (int y = 0; y < at.height; ++y) {
        std::memcpy(to + static_cast<size_t>(at.y + y) * out.Stride() +
                        static_cast<size_t>(at.x) * 4u,
                    from + static_cast<size_t>(y) * source->Stride(), rowBytes);
    }
    return true;
}

}  // namespace

bool ComposeGuide(const std::vector<Image>& steps, const GuideOptions& options,
                  Image& out, size_t* placed) {
    if (placed != nullptr) {
        *placed = 0;
    }
    if (steps.empty() || !OptionsSane(options)) {
        return false;
    }
    for (const Image& step : steps) {
        if (!step.Valid() || &step == &out) {
            return false;
        }
    }

    // ÖNCE BOYUTLAR, SONRA TUVAL: sütun genişliği en geniş adıma bağlı ve o,
    // hepsine bakmadan bilinemez. Stitch.cpp ile aynı iki geçişli düzen.
    const int innerWidth = options.maxWidth - 2 * options.padding;
    std::vector<guide::Placement> layout;
    layout.reserve(steps.size());

    int columnInner = 1;
    int totalHeight = 2 * options.padding;
    for (const Image& step : steps) {
        const SIZE size = FittedSize(step, innerWidth);
        const int extra = size.cy + (layout.empty() ? 0 : options.gap);
        // Image::Create tek kenarı kMaxImageSide ile sınırlar; sığmayan adımda
        // durulur ve eldeki kadarı teslim edilir (bkz. başlık).
        if (totalHeight + extra > kMaxImageSide) {
            break;
        }
        guide::Placement at;
        at.x = options.padding;
        at.y = totalHeight - options.padding + (layout.empty() ? 0 : options.gap);
        at.width = size.cx;
        at.height = size.cy;
        layout.push_back(at);
        totalHeight += extra;
        columnInner = (std::max)(columnInner, static_cast<int>(size.cx));
    }
    if (layout.empty()) {
        return false;
    }

    const int totalWidth = columnInner + 2 * options.padding;
    if (!out.Create(totalWidth, totalHeight)) {
        return false;
    }
    out.Fill(options.background);

    std::vector<POINT> badges;
    badges.reserve(layout.size());
    for (size_t i = 0; i < layout.size(); ++i) {
        if (!PlaceStep(steps[i], layout[i], out)) {
            out.Reset();
            return false;
        }
        if (options.drawFrame) {
            guide::DrawFrame(out, layout[i], kFrameColor);
        }
        if (options.badgeDiameter > 0) {
            const POINT centre = guide::BadgeCentre(layout[i], options.badgeDiameter);
            guide::DrawBadgeDisk(out, centre, options.badgeDiameter,
                                 options.badgeColor);
            badges.push_back(centre);
        }
    }

    // Numaralar EN SONDA ve tek seferde: her rozet için ayrı DC açmak
    // yavaş değil ama gereksiz, ve alfa düzeltmesi tek yerde toplanıyor.
    if (!badges.empty() && !guide::DrawBadgeNumbers(out, badges, options)) {
        out.Reset();
        return false;
    }

    if (placed != nullptr) {
        *placed = layout.size();
    }
    return true;
}

bool ComposeGuide(const std::vector<Image>& steps, Image& out) {
    return ComposeGuide(steps, GuideOptions{}, out, nullptr);
}

}  // namespace crisp
