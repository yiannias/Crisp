// TailwindPalette.cpp — Tailwind CSS v3.4 varsayılan paleti ve en yakın
// girdi araması; bkz. ColorSpace.h.
//
// AYRI DOSYA: tablo tek başına 250 satırdır ve ColorSpace.cpp'ye eklenince
// ev kuralının 400 satır sınırı aşılıyordu. Arama da burada, çünkü tabloyu
// tanıyan tek yer burası: sıralama ("eşitlikte ilk kazanır") bu dosyanın
// vaadidir.
//
// SIRA ÖNEMLİ: Tailwind'in kendi belgeleme sırası korunur (slate'ten rose'a,
// her renkte 50'den 950'ye), ardından black ve white. zinc-50 ile neutral-50
// aynı hex'e sahiptir; eşitlikte tablodaki ilk (zinc) döner ve test bunu
// kararlı davranış olarak bekler.
#include "ColorSpace.h"

namespace crisp {
namespace {

// Hex sabitleri 0xRRGGBB olarak yazılır — belgede görülen hâliyle — ve RGB()
// makrosuyla COLORREF'e çevrilir; elle BGR yazmak yazım hatasına davetiyedir.
#define CRISP_TW(name, hex) \
    {L##name, RGB(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)}

constexpr TailwindColor kPalette[] = {
    CRISP_TW("slate-50", 0xf8fafc),   CRISP_TW("slate-100", 0xf1f5f9),
    CRISP_TW("slate-200", 0xe2e8f0),  CRISP_TW("slate-300", 0xcbd5e1),
    CRISP_TW("slate-400", 0x94a3b8),  CRISP_TW("slate-500", 0x64748b),
    CRISP_TW("slate-600", 0x475569),  CRISP_TW("slate-700", 0x334155),
    CRISP_TW("slate-800", 0x1e293b),  CRISP_TW("slate-900", 0x0f172a),
    CRISP_TW("slate-950", 0x020617),

    CRISP_TW("gray-50", 0xf9fafb),    CRISP_TW("gray-100", 0xf3f4f6),
    CRISP_TW("gray-200", 0xe5e7eb),   CRISP_TW("gray-300", 0xd1d5db),
    CRISP_TW("gray-400", 0x9ca3af),   CRISP_TW("gray-500", 0x6b7280),
    CRISP_TW("gray-600", 0x4b5563),   CRISP_TW("gray-700", 0x374151),
    CRISP_TW("gray-800", 0x1f2937),   CRISP_TW("gray-900", 0x111827),
    CRISP_TW("gray-950", 0x030712),

    CRISP_TW("zinc-50", 0xfafafa),    CRISP_TW("zinc-100", 0xf4f4f5),
    CRISP_TW("zinc-200", 0xe4e4e7),   CRISP_TW("zinc-300", 0xd4d4d8),
    CRISP_TW("zinc-400", 0xa1a1aa),   CRISP_TW("zinc-500", 0x71717a),
    CRISP_TW("zinc-600", 0x52525b),   CRISP_TW("zinc-700", 0x3f3f46),
    CRISP_TW("zinc-800", 0x27272a),   CRISP_TW("zinc-900", 0x18181b),
    CRISP_TW("zinc-950", 0x09090b),

    CRISP_TW("neutral-50", 0xfafafa), CRISP_TW("neutral-100", 0xf5f5f5),
    CRISP_TW("neutral-200", 0xe5e5e5), CRISP_TW("neutral-300", 0xd4d4d4),
    CRISP_TW("neutral-400", 0xa3a3a3), CRISP_TW("neutral-500", 0x737373),
    CRISP_TW("neutral-600", 0x525252), CRISP_TW("neutral-700", 0x404040),
    CRISP_TW("neutral-800", 0x262626), CRISP_TW("neutral-900", 0x171717),
    CRISP_TW("neutral-950", 0x0a0a0a),

    CRISP_TW("stone-50", 0xfafaf9),   CRISP_TW("stone-100", 0xf5f5f4),
    CRISP_TW("stone-200", 0xe7e5e4),  CRISP_TW("stone-300", 0xd6d3d1),
    CRISP_TW("stone-400", 0xa8a29e),  CRISP_TW("stone-500", 0x78716c),
    CRISP_TW("stone-600", 0x57534e),  CRISP_TW("stone-700", 0x44403c),
    CRISP_TW("stone-800", 0x292524),  CRISP_TW("stone-900", 0x1c1917),
    CRISP_TW("stone-950", 0x0c0a09),

    CRISP_TW("red-50", 0xfef2f2),     CRISP_TW("red-100", 0xfee2e2),
    CRISP_TW("red-200", 0xfecaca),    CRISP_TW("red-300", 0xfca5a5),
    CRISP_TW("red-400", 0xf87171),    CRISP_TW("red-500", 0xef4444),
    CRISP_TW("red-600", 0xdc2626),    CRISP_TW("red-700", 0xb91c1c),
    CRISP_TW("red-800", 0x991b1b),    CRISP_TW("red-900", 0x7f1d1d),
    CRISP_TW("red-950", 0x450a0a),

    CRISP_TW("orange-50", 0xfff7ed),  CRISP_TW("orange-100", 0xffedd5),
    CRISP_TW("orange-200", 0xfed7aa), CRISP_TW("orange-300", 0xfdba74),
    CRISP_TW("orange-400", 0xfb923c), CRISP_TW("orange-500", 0xf97316),
    CRISP_TW("orange-600", 0xea580c), CRISP_TW("orange-700", 0xc2410c),
    CRISP_TW("orange-800", 0x9a3412), CRISP_TW("orange-900", 0x7c2d12),
    CRISP_TW("orange-950", 0x431407),

    CRISP_TW("amber-50", 0xfffbeb),   CRISP_TW("amber-100", 0xfef3c7),
    CRISP_TW("amber-200", 0xfde68a),  CRISP_TW("amber-300", 0xfcd34d),
    CRISP_TW("amber-400", 0xfbbf24),  CRISP_TW("amber-500", 0xf59e0b),
    CRISP_TW("amber-600", 0xd97706),  CRISP_TW("amber-700", 0xb45309),
    CRISP_TW("amber-800", 0x92400e),  CRISP_TW("amber-900", 0x78350f),
    CRISP_TW("amber-950", 0x451a03),

    CRISP_TW("yellow-50", 0xfefce8),  CRISP_TW("yellow-100", 0xfef9c3),
    CRISP_TW("yellow-200", 0xfef08a), CRISP_TW("yellow-300", 0xfde047),
    CRISP_TW("yellow-400", 0xfacc15), CRISP_TW("yellow-500", 0xeab308),
    CRISP_TW("yellow-600", 0xca8a04), CRISP_TW("yellow-700", 0xa16207),
    CRISP_TW("yellow-800", 0x854d0e), CRISP_TW("yellow-900", 0x713f12),
    CRISP_TW("yellow-950", 0x422006),

    CRISP_TW("lime-50", 0xf7fee7),    CRISP_TW("lime-100", 0xecfccb),
    CRISP_TW("lime-200", 0xd9f99d),   CRISP_TW("lime-300", 0xbef264),
    CRISP_TW("lime-400", 0xa3e635),   CRISP_TW("lime-500", 0x84cc16),
    CRISP_TW("lime-600", 0x65a30d),   CRISP_TW("lime-700", 0x4d7c0f),
    CRISP_TW("lime-800", 0x3f6212),   CRISP_TW("lime-900", 0x365314),
    CRISP_TW("lime-950", 0x1a2e05),

    CRISP_TW("green-50", 0xf0fdf4),   CRISP_TW("green-100", 0xdcfce7),
    CRISP_TW("green-200", 0xbbf7d0),  CRISP_TW("green-300", 0x86efac),
    CRISP_TW("green-400", 0x4ade80),  CRISP_TW("green-500", 0x22c55e),
    CRISP_TW("green-600", 0x16a34a),  CRISP_TW("green-700", 0x15803d),
    CRISP_TW("green-800", 0x166534),  CRISP_TW("green-900", 0x14532d),
    CRISP_TW("green-950", 0x052e16),

    CRISP_TW("emerald-50", 0xecfdf5), CRISP_TW("emerald-100", 0xd1fae5),
    CRISP_TW("emerald-200", 0xa7f3d0), CRISP_TW("emerald-300", 0x6ee7b7),
    CRISP_TW("emerald-400", 0x34d399), CRISP_TW("emerald-500", 0x10b981),
    CRISP_TW("emerald-600", 0x059669), CRISP_TW("emerald-700", 0x047857),
    CRISP_TW("emerald-800", 0x065f46), CRISP_TW("emerald-900", 0x064e3b),
    CRISP_TW("emerald-950", 0x022c22),

    CRISP_TW("teal-50", 0xf0fdfa),    CRISP_TW("teal-100", 0xccfbf1),
    CRISP_TW("teal-200", 0x99f6e4),   CRISP_TW("teal-300", 0x5eead4),
    CRISP_TW("teal-400", 0x2dd4bf),   CRISP_TW("teal-500", 0x14b8a6),
    CRISP_TW("teal-600", 0x0d9488),   CRISP_TW("teal-700", 0x0f766e),
    CRISP_TW("teal-800", 0x115e59),   CRISP_TW("teal-900", 0x134e4a),
    CRISP_TW("teal-950", 0x042f2e),

    CRISP_TW("cyan-50", 0xecfeff),    CRISP_TW("cyan-100", 0xcffafe),
    CRISP_TW("cyan-200", 0xa5f3fc),   CRISP_TW("cyan-300", 0x67e8f9),
    CRISP_TW("cyan-400", 0x22d3ee),   CRISP_TW("cyan-500", 0x06b6d4),
    CRISP_TW("cyan-600", 0x0891b2),   CRISP_TW("cyan-700", 0x0e7490),
    CRISP_TW("cyan-800", 0x155e75),   CRISP_TW("cyan-900", 0x164e63),
    CRISP_TW("cyan-950", 0x083344),

    CRISP_TW("sky-50", 0xf0f9ff),     CRISP_TW("sky-100", 0xe0f2fe),
    CRISP_TW("sky-200", 0xbae6fd),    CRISP_TW("sky-300", 0x7dd3fc),
    CRISP_TW("sky-400", 0x38bdf8),    CRISP_TW("sky-500", 0x0ea5e9),
    CRISP_TW("sky-600", 0x0284c7),    CRISP_TW("sky-700", 0x0369a1),
    CRISP_TW("sky-800", 0x075985),    CRISP_TW("sky-900", 0x0c4a6e),
    CRISP_TW("sky-950", 0x082f49),

    CRISP_TW("blue-50", 0xeff6ff),    CRISP_TW("blue-100", 0xdbeafe),
    CRISP_TW("blue-200", 0xbfdbfe),   CRISP_TW("blue-300", 0x93c5fd),
    CRISP_TW("blue-400", 0x60a5fa),   CRISP_TW("blue-500", 0x3b82f6),
    CRISP_TW("blue-600", 0x2563eb),   CRISP_TW("blue-700", 0x1d4ed8),
    CRISP_TW("blue-800", 0x1e40af),   CRISP_TW("blue-900", 0x1e3a8a),
    CRISP_TW("blue-950", 0x172554),

    CRISP_TW("indigo-50", 0xeef2ff),  CRISP_TW("indigo-100", 0xe0e7ff),
    CRISP_TW("indigo-200", 0xc7d2fe), CRISP_TW("indigo-300", 0xa5b4fc),
    CRISP_TW("indigo-400", 0x818cf8), CRISP_TW("indigo-500", 0x6366f1),
    CRISP_TW("indigo-600", 0x4f46e5), CRISP_TW("indigo-700", 0x4338ca),
    CRISP_TW("indigo-800", 0x3730a3), CRISP_TW("indigo-900", 0x312e81),
    CRISP_TW("indigo-950", 0x1e1b4b),

    CRISP_TW("violet-50", 0xf5f3ff),  CRISP_TW("violet-100", 0xede9fe),
    CRISP_TW("violet-200", 0xddd6fe), CRISP_TW("violet-300", 0xc4b5fd),
    CRISP_TW("violet-400", 0xa78bfa), CRISP_TW("violet-500", 0x8b5cf6),
    CRISP_TW("violet-600", 0x7c3aed), CRISP_TW("violet-700", 0x6d28d9),
    CRISP_TW("violet-800", 0x5b21b6), CRISP_TW("violet-900", 0x4c1d95),
    CRISP_TW("violet-950", 0x2e1065),

    CRISP_TW("purple-50", 0xfaf5ff),  CRISP_TW("purple-100", 0xf3e8ff),
    CRISP_TW("purple-200", 0xe9d5ff), CRISP_TW("purple-300", 0xd8b4fe),
    CRISP_TW("purple-400", 0xc084fc), CRISP_TW("purple-500", 0xa855f7),
    CRISP_TW("purple-600", 0x9333ea), CRISP_TW("purple-700", 0x7e22ce),
    CRISP_TW("purple-800", 0x6b21a8), CRISP_TW("purple-900", 0x581c87),
    CRISP_TW("purple-950", 0x3b0764),

    CRISP_TW("fuchsia-50", 0xfdf4ff), CRISP_TW("fuchsia-100", 0xfae8ff),
    CRISP_TW("fuchsia-200", 0xf5d0fe), CRISP_TW("fuchsia-300", 0xf0abfc),
    CRISP_TW("fuchsia-400", 0xe879f9), CRISP_TW("fuchsia-500", 0xd946ef),
    CRISP_TW("fuchsia-600", 0xc026d3), CRISP_TW("fuchsia-700", 0xa21caf),
    CRISP_TW("fuchsia-800", 0x86198f), CRISP_TW("fuchsia-900", 0x701a75),
    CRISP_TW("fuchsia-950", 0x4a044e),

    CRISP_TW("pink-50", 0xfdf2f8),    CRISP_TW("pink-100", 0xfce7f3),
    CRISP_TW("pink-200", 0xfbcfe8),   CRISP_TW("pink-300", 0xf9a8d4),
    CRISP_TW("pink-400", 0xf472b6),   CRISP_TW("pink-500", 0xec4899),
    CRISP_TW("pink-600", 0xdb2777),   CRISP_TW("pink-700", 0xbe185d),
    CRISP_TW("pink-800", 0x9d174d),   CRISP_TW("pink-900", 0x831843),
    CRISP_TW("pink-950", 0x500724),

    CRISP_TW("rose-50", 0xfff1f2),    CRISP_TW("rose-100", 0xffe4e6),
    CRISP_TW("rose-200", 0xfecdd3),   CRISP_TW("rose-300", 0xfda4af),
    CRISP_TW("rose-400", 0xfb7185),   CRISP_TW("rose-500", 0xf43f5e),
    CRISP_TW("rose-600", 0xe11d48),   CRISP_TW("rose-700", 0xbe123c),
    CRISP_TW("rose-800", 0x9f1239),   CRISP_TW("rose-900", 0x881337),
    CRISP_TW("rose-950", 0x4c0519),

    CRISP_TW("black", 0x000000),      CRISP_TW("white", 0xffffff),
};

#undef CRISP_TW

constexpr size_t kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);
static_assert(kPaletteCount == 22 * 11 + 2, "Tailwind paleti eksik ya da fazla");

// ΔE76: Lab'da düz Öklid uzaklığının karesi. Karekök alınmaz — yalnızca
// karşılaştırılıyor ve sıralamayı değiştirmez.
[[nodiscard]] double LabDistanceSquared(const Lab& x, const Lab& y) noexcept {
    const double dl = x.l - y.l;
    const double da = x.a - y.a;
    const double db = x.b - y.b;
    return dl * dl + da * da + db * db;
}

}  // namespace

const TailwindColor* TailwindPalette(size_t& count) noexcept {
    count = kPaletteCount;
    return kPalette;
}

std::wstring NearestTailwindName(COLORREF color, COLORREF* matched) {
    // Tablo küçük (244 girdi); her çağrıda Lab'a çevirmek bir renk seçiminde
    // ölçülemeyecek kadar ucuz ve önbellek tutmaktan basit.
    const Lab target = RgbToLab(color);
    size_t best = 0;
    double bestDistance = LabDistanceSquared(target, RgbToLab(kPalette[0].rgb));
    for (size_t i = 1; i < kPaletteCount; ++i) {
        const double distance =
            LabDistanceSquared(target, RgbToLab(kPalette[i].rgb));
        // Kesin "küçüktür": eşitlikte önceki (tablodaki ilk) girdi kalır.
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    if (matched != nullptr) {
        *matched = kPalette[best].rgb;
    }
    return std::wstring(kPalette[best].name);
}

}  // namespace crisp
