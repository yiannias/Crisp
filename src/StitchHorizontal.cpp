// StitchHorizontal.cpp — bkz. Stitch.h. Kareleri yan yana ekler.
#include "Stitch.h"

#include "StitchInternal.h"

#include <cstring>

namespace crisp {
namespace {

using stitch::MutableRowFromTop;
using stitch::RowFromTop;

// Bir karenin [fromColumn, fromColumn + columns) sütunlarını, hedefin
// `toColumn`undan başlayarak kopyalar; satır satır, çünkü sütunlar bellekte
// ardışık değil.
void CopyColumns(Image& out, int toColumn, const Image& from, int fromColumn,
                 int columns) noexcept {
    const int height = from.Height();
    for (int y = 0; y < height; ++y) {
        std::memcpy(MutableRowFromTop(out, y) + toColumn,
                    RowFromTop(from, y) + fromColumn,
                    static_cast<size_t>(columns) * sizeof(uint32_t));
    }
}

}  // namespace

bool StitchHorizontal(const std::vector<Image>& frames, int minOverlap,
                      Image& out, size_t* stopped) {
    if (stopped != nullptr) {
        *stopped = frames.size();
    }
    if (frames.empty() || !frames.front().Valid()) {
        return false;
    }

    const int width = frames.front().Width();
    const int height = frames.front().Height();

    // Dikeyle aynı ön geçiş; yalnızca eksen ve arama değişiyor.
    std::vector<int> shifts;
    int total = width;
    const size_t used = stitch::PlanShifts(
        frames, width, height,
        [minOverlap](const Image& previous, const Image& next) {
            return FindHorizontalShift(previous, next, minOverlap);
        },
        shifts, total);

    if (stopped != nullptr) {
        *stopped = used;
    }
    if (!out.Create(total, height)) {
        return false;
    }

    // İlk kare bütünüyle, sonrakilerin YALNIZCA yeni kısmı: her karenin son
    // `shift` sütunu.
    CopyColumns(out, 0, frames[0], 0, width);
    int writtenTo = width;
    for (size_t i = 0; i < shifts.size(); ++i) {
        const int shift = shifts[i];
        CopyColumns(out, writtenTo, frames[i + 1], width - shift, shift);
        writtenTo += shift;
    }
    return true;
}

}  // namespace crisp
