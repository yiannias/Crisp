// EditorTextLayout.cpp — Metin taslağının piksel geometrisi. Gerekçe başlıkta.
//
// ÖLÇÜM YÖNTEMİ: her satır için imleçten önceki parça GetTextExtentPoint32W
// ile ölçülür. DrawText'in kendisi hangi harfin nerede bittiğini söylemez;
// önek ölçmek aynı yazı tipiyle aynı sonucu verir, çünkü DrawText de
// satırları kerning'siz, soldan sağa dizer.
#include "EditorTextLayout.h"

#include "EditorRender.h"
#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "resource.h"

#include <algorithm>

namespace crisp {
namespace editor {
namespace {

// Bir satırın [start, pos) önekinin genişliği.
[[nodiscard]] int PrefixWidth(HDC dc, const std::wstring& text, size_t start,
                              size_t pos) noexcept {
    if (pos <= start) {
        return 0;   // GetTextExtentPoint32W sıfır uzunlukta başarısız olur
    }
    SIZE size{};
    if (!::GetTextExtentPoint32W(dc, text.c_str() + start,
                                 static_cast<int>(pos - start), &size)) {
        return 0;
    }
    return static_cast<int>(size.cx);
}

// İki rengi karıştırır; amount 0..255 ikinci rengin ağırlığı. Alfa yerine
// katı renk: seçim şeridi tek FillRect ile çizilir, AlphaBlend gerekmez.
[[nodiscard]] COLORREF Mix(COLORREF base, COLORREF over, int amount) noexcept {
    const int inverse = 255 - amount;
    const int r = (GetRValue(base) * inverse + GetRValue(over) * amount) / 255;
    const int g = (GetGValue(base) * inverse + GetGValue(over) * amount) / 255;
    const int b = (GetBValue(base) * inverse + GetBValue(over) * amount) / 255;
    return RGB(r, g, b);
}

// Pencerenin DC'sini taslağın yazı tipiyle ödünç alır; yıkıcı geri verir.
// Klavye ve fare yolları boyama dışında ölçmek zorunda, çünkü imlecin
// pikselini IME'ye ve tıklamayı tampon konumuna çevirmek WM_PAINT'i bekleyemez.
class DraftDc {
public:
    DraftDc(HWND window, const State& state) : window_(window) {
        dc_ = ::GetDC(window);
        if (dc_ == nullptr) {
            return;
        }
        font_ = CreateTextFont(state.textDraft.thickness, state.dpi,
                               state.scale, false);
        if (font_ != nullptr) {
            old_ = ::SelectObject(dc_, font_);
        }
    }
    ~DraftDc() {
        if (dc_ != nullptr) {
            if (old_ != nullptr) {
                ::SelectObject(dc_, old_);
            }
            ::ReleaseDC(window_, dc_);
        }
        if (font_ != nullptr) {
            ::DeleteObject(font_);
        }
    }
    DraftDc(const DraftDc&) = delete;
    DraftDc& operator=(const DraftDc&) = delete;

    [[nodiscard]] bool Ok() const noexcept {
        return dc_ != nullptr && font_ != nullptr;
    }
    [[nodiscard]] HDC Get() const noexcept { return dc_; }

private:
    HWND window_ = nullptr;
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ old_ = nullptr;
};

}  // namespace

COLORREF TextSelectionColor() noexcept {
    // Vurgu renginin ~%35'i tema zemini üstünde: tam vurgu, kullanıcının
    // seçtiği metin rengini (çoğunlukla kırmızı) okunmaz hâle getirirdi.
    const Palette& colors = theme::Colors();
    return Mix(colors.surface, colors.accent, 90);
}

TextDraftLayout MeasureTextDraft(HDC dc, const State& state) {
    TextDraftLayout layout;
    const Shape& draft = state.textDraft;
    layout.origin = ToClient(state, draft.start);
    layout.empty = draft.text.empty();
    // BOŞKEN İPUCU: kutunun içinde soluk bir "Yazmaya başlayın" durur. Boş bir
    // çerçeve, kullanıcıya orada ne yapması gerektiğini söylemiyordu.
    layout.shown = layout.empty ? Loc::Str(IDS_TEXT_HINT) : draft.text;

    TEXTMETRICW metrics{};
    ::GetTextMetricsW(dc, &metrics);
    layout.lineHeight = (std::max)(1, static_cast<int>(metrics.tmHeight));

    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, layout.shown.c_str(), -1, &measure,
                DT_CALCRECT | DT_NOPREFIX);
    const int width = (std::max)(static_cast<int>(geom::Width(measure)),
                                 Scale(24, state.dpi));
    // SONDAKİ BOŞ SATIR DA SAYILIR: DT_CALCRECT "abc\n"ı tek satır ölçer ama
    // imleç ikinci satırdadır; kutu onu da kapsamalı.
    const int lines = layout.empty ? 1 : state.textEdit.LineCount();
    const int height = (std::max)(static_cast<int>(geom::Height(measure)),
                                  lines * layout.lineHeight);

    const int pad = Scale(4, state.dpi);
    layout.box = RECT{layout.origin.x - pad, layout.origin.y - pad,
                      layout.origin.x + width + pad,
                      layout.origin.y + height + pad};
    return layout;
}

POINT TextCaretPixel(HDC dc, const TextEdit& edit,
                     const TextDraftLayout& layout, size_t pos) {
    pos = edit.Snap(pos);
    const int line = edit.LineOf(pos);
    const size_t start = edit.LineStart(line);
    return POINT{layout.origin.x + PrefixWidth(dc, edit.text, start, pos),
                 layout.origin.y + line * layout.lineHeight};
}

size_t TextPositionAt(HDC dc, const TextEdit& edit,
                      const TextDraftLayout& layout, POINT client) {
    const int lines = edit.LineCount();
    int line = (client.y - layout.origin.y) / layout.lineHeight;
    if (client.y < layout.origin.y) {
        line = 0;
    }
    line = (std::max)(0, (std::min)(line, lines - 1));

    const size_t start = edit.LineStart(line);
    const size_t end = edit.LineEnd(start);
    const int x = client.x - layout.origin.x;

    // Sınırdan sınıra yürüyüp tıklamanın hangi harfin ORTASINI geçtiğine
    // bakılır: harfin sol yarısına tıklayan kullanıcı imleci önünde bekler.
    size_t previous = start;
    int previousX = 0;
    for (size_t pos = start; pos < end;) {
        const size_t next = edit.NextBoundary(pos);
        const int nextX = PrefixWidth(dc, edit.text, start, next);
        if (x < (previousX + nextX) / 2) {
            return previous;
        }
        previous = next;
        previousX = nextX;
        pos = next;
    }
    return end;
}

void DrawTextSelection(HDC dc, const TextEdit& edit,
                       const TextDraftLayout& layout, COLORREF color) {
    if (!edit.HasSelection()) {
        return;
    }
    const auto [from, to] = edit.SelectionRange();
    const int firstLine = edit.LineOf(from);
    const int lastLine = edit.LineOf(to);
    // Satır sonunun seçili olduğunu göstermek için şerit bir boşluk kadar
    // uzar; yoksa "\n" seçiliyken hiçbir şey görünmez.
    const int breakWidth = PrefixWidth(dc, std::wstring(L" "), 0, 1);

    for (int line = firstLine; line <= lastLine; ++line) {
        const size_t start = edit.LineStart(line);
        const size_t end = edit.LineEnd(start);
        const size_t segmentFrom = (std::max)(from, start);
        const size_t segmentTo = (std::min)(to, end);
        const int left = PrefixWidth(dc, edit.text, start, segmentFrom);
        int right = PrefixWidth(dc, edit.text, start, segmentTo);
        if (to > end) {
            right += breakWidth;
        }
        const int y = layout.origin.y + line * layout.lineHeight;
        FillRectColor(dc,
                      RECT{layout.origin.x + left, y, layout.origin.x + right,
                           y + layout.lineHeight},
                      color);
    }
}

POINT TextCaretClient(HWND window, const State& state) {
    const DraftDc dc(window, state);
    if (!dc.Ok()) {
        return ToClient(state, state.textDraft.start);
    }
    const TextDraftLayout layout = MeasureTextDraft(dc.Get(), state);
    if (layout.empty) {
        return layout.origin;
    }
    return TextCaretPixel(dc.Get(), state.textEdit, layout,
                          state.textEdit.caret);
}

bool TextHitTest(HWND window, const State& state, POINT client, size_t& pos) {
    const DraftDc dc(window, state);
    if (!dc.Ok()) {
        return false;
    }
    const TextDraftLayout layout = MeasureTextDraft(dc.Get(), state);
    if (!::PtInRect(&layout.box, client)) {
        return false;
    }
    pos = layout.empty ? 0
                       : TextPositionAt(dc.Get(), state.textEdit, layout, client);
    return true;
}

}  // namespace editor
}  // namespace crisp
