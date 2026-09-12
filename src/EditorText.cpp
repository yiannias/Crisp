// EditorText.cpp — Metin aracının yazma kipi: kutu, imleç, seçim ve tuşlar.
//
// NEDEN AYRI BİR DOSYA VE NEDEN BU KADAR İŞ: metin aracına basıp tuvale
// tıklandığında EKRANDA HİÇBİR ŞEY OLMUYORDU. Ne bir imleç, ne bir çerçeve,
// ne bir ipucu — kip açılıyordu ama görünmüyordu. Kullanıcının makul çıkarımı
// "bu düğme bozuk" oluyordu; nitekim tam olarak bu bildirildi.
//
// İMLEÇ ARTIK GEZİYOR: önceki sürüm yalnızca sona ekliyordu. Bir harfi
// yanlış yazan kullanıcı ondan sonraki her şeyi silmek zorundaydı; ok tuşu
// basınca hiçbir şey olmuyordu. Tampon mantığı çekirdekteki TextEdit'te ve
// testlidir; burası yalnızca tuşları ona, tamponu da ekrana çevirir.
#include "EditorInternal.h"

#include "ClipboardImage.h"
#include "EditorRender.h"
#include "EditorTextLayout.h"
#include "Geometry.h"
#include "Theme.h"
#include "Util.h"

#include <imm.h>

#include <algorithm>
#include <string>

namespace crisp {
namespace editor {
namespace {

// İmleç yanıp sönme aralığı. Windows'un kendi değeri GetCaretBlinkTime ile
// okunur; kullanıcı onu sistem ayarlarından değiştirdiyse buna da uymalı.
[[nodiscard]] UINT CaretPeriod() noexcept {
    const UINT blink = ::GetCaretBlinkTime();
    // 0 ve INFINITE "yanıp sönme" demek değildir; ikisi de sabit imleç ister.
    return (blink == 0 || blink == INFINITE) ? 0 : blink;
}

// IME aday penceresini İMLECİN yanına taşır ve yazı tipini taslağınkiyle
// eşler. Yapılmazsa Japonca/Korece/Çince yazan kullanıcı aday listesini
// pencerenin sol üst köşesinde, bileşim metnini de başka bir boyda bulur.
void SyncIme(HWND window, const State& state) {
    const HIMC context = ::ImmGetContext(window);
    if (context == nullptr) {
        return;
    }
    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = TextCaretClient(window, state);
    (void)::ImmSetCompositionWindow(context, &form);

    const HFONT font = CreateTextFont(state.textDraft.thickness, state.dpi,
                                      state.scale, false);
    if (font != nullptr) {
        LOGFONTW logFont{};
        if (::GetObjectW(font, sizeof(logFont), &logFont) != 0) {
            (void)::ImmSetCompositionFontW(context, &logFont);
        }
        ::DeleteObject(font);
    }
    (void)::ImmReleaseContext(window, context);
}

// Yarım kalmış IME bileşimini iptal eder. Yazma bittikten sonra bileşim
// kapanırken ürettiği WM_CHAR'lar başka bir yere düşerdi.
void CancelImeComposition(HWND window) {
    const HIMC context = ::ImmGetContext(window);
    if (context == nullptr) {
        return;
    }
    (void)::ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    (void)::ImmReleaseContext(window, context);
}

// Her tampon değişikliğinden sonra: metni taslağa yansıt, imleci göster,
// IME'yi imlece taşı, yeniden çiz.
//
// İMLEÇ HER TUŞTA YENİDEN GÖRÜNÜR OLMALI: yanıp sönme evresi kapalıyken
// yazmak ya da ok tuşuna basmak, harfin nereye gittiğini göstermezdi.
void AfterEdit(HWND window, State& state) {
    state.textDraft.text = state.textEdit.text;
    state.caretOn = true;
    const UINT period = CaretPeriod();
    if (period != 0) {
        ::SetTimer(window, kCaretTimer, period, nullptr);
    }
    SyncIme(window, state);
    ::InvalidateRect(window, nullptr, FALSE);
}

void CopySelection(HWND window, const State& state) {
    if (!state.textEdit.HasSelection()) {
        return;
    }
    const std::wstring selected = state.textEdit.SelectedText();
    if (!CopyTextToClipboard(selected.c_str(), window)) {
        LogV(L"Metin seçimi panoya kopyalanamadı");
    }
}

void PasteFromClipboard(HWND window, State& state) {
    std::wstring raw;
    if (!ReadTextFromClipboard(raw, window)) {
        return;
    }
    const std::wstring clean = TextEdit::Sanitize(raw);
    if (!clean.empty()) {
        state.textEdit.Insert(clean);
    }
}

}  // namespace

void DrawTextDraft(HDC dc, const State& state) {
    const Shape& draft = state.textDraft;
    const Palette& colors = theme::Colors();

    const HFONT font =
        CreateTextFont(draft.thickness, state.dpi, state.scale, false);
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);

    const TextDraftLayout layout = MeasureTextDraft(dc, state);

    // Kutu KESİKLİ ve vurgu renginde: düz bir çerçeve, kullanıcının çizdiği
    // dikdörtgen aracının sonucuyla karışırdı.
    const HPEN pen = ::CreatePen(PS_DOT, 1, colors.accent);
    if (pen != nullptr) {
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, layout.box.left, layout.box.top, layout.box.right,
                    layout.box.bottom);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
    }

    // Seçim şeridi METNİN ALTINA çizilir; üstüne çizilseydi harfleri örterdi.
    if (!layout.empty) {
        DrawTextSelection(dc, state.textEdit, layout, TextSelectionColor());
    }

    ::SetTextColor(dc, layout.empty ? colors.textDim : draft.color);
    RECT area{layout.origin.x, layout.origin.y,
              layout.box.right + Scale(400, state.dpi),
              layout.box.bottom + Scale(400, state.dpi)};
    ::DrawTextW(dc, layout.shown.c_str(), -1, &area, DT_NOPREFIX | DT_NOCLIP);

    // İMLEÇ: tampondaki yerinde, yanıp sönerek. Sabit bir çizgi metnin
    // parçası sanılırdı; yanıp sönen çizgi "buraya yazılıyor" demenin
    // evrensel yolu.
    const POINT caret =
        layout.empty ? layout.origin
                     : TextCaretPixel(dc, state.textEdit, layout,
                                      state.textEdit.caret);
    if (state.caretOn) {
        FillRectColor(dc,
                      RECT{caret.x, caret.y,
                           caret.x + (std::max)(1, Scale(2, state.dpi)),
                           caret.y + layout.lineHeight},
                      layout.empty ? colors.accent : draft.color);
    }

    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

void BeginTextDraft(HWND window, State& state, POINT image) {
    CommitTextDraft(state);
    state.typing = true;
    state.caretOn = true;
    state.textDraft = Shape{};
    state.textDraft.kind = ToolKind::Text;
    state.textDraft.start = image;
    state.textDraft.end = image;
    state.textDraft.color = state.color;
    state.textDraft.thickness = state.thickness;
    state.textEdit.Clear();
    const UINT period = CaretPeriod();
    if (period != 0) {
        ::SetTimer(window, kCaretTimer, period, nullptr);
    }
    // IME DAHA İLK HARFTEN ÖNCE: aday penceresi ilk tuşta açılır ve o ana
    // kadar konum verilmemişse pencerenin köşesinde belirir.
    SyncIme(window, state);
    ::InvalidateRect(window, nullptr, FALSE);
}

void EndTextDraft(HWND window, State& state, bool commit) {
    if (!state.typing) {
        return;
    }
    CancelImeComposition(window);
    ::KillTimer(window, kCaretTimer);
    if (commit) {
        CommitTextDraft(state);
    } else {
        // İlk Esc yazmayı iptal eder, pencereyi kapatmaz: kullanıcı bir harfi
        // yanlış yazdı diye tüm düzenlemeyi kaybetmemeli.
        state.typing = false;
        state.textDraft = Shape{};
        state.textEdit.Clear();
    }
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

bool TextTypingChar(HWND window, State& state, wchar_t ch) {
    if (!state.typing) {
        return false;
    }
    TextEdit& edit = state.textEdit;

    if (ch == L'\b') {
        // Vekil çift ve seçim TextEdit'in işi: burada yalnızca tuş çevrilir.
        edit.Backspace();
    } else if (ch == 0x7F) {
        // Ctrl+Backspace WM_CHAR'a 0x7F (DEL) olarak düşer; ' ' üstünde olduğu
        // için eskiden metne GÖRÜNMEZ bir karakter olarak giriyordu. Her metin
        // kutusundaki anlamı: önceki kelimeyi sil.
        if (!edit.HasSelection()) {
            edit.MoveWordLeft(true);
        }
        edit.EraseSelection();
    } else if (ch == L'\r') {
        edit.InsertChar(L'\n');   // Enter satır atlar
    } else if (ch == L'\n') {
        // Ctrl+Enter bitirir. Enter'ın kendisi satır atladığı için yazmayı
        // sonlandıracak bir tuş gerekiyordu.
        EndTextDraft(window, state, true);
        return true;
    } else if (ch == L'\t') {
        edit.Insert(L"    ");
    } else if (ch >= L' ') {
        edit.InsertChar(ch);
    } else {
        return true;   // diğer denetim karakterleri (Ctrl+A/C/V/X...) yutulur
    }

    AfterEdit(window, state);
    return true;
}

bool TextKeyDown(HWND window, State& state, WPARAM key, bool control,
                 bool shift) {
    if (!state.typing) {
        return false;
    }
    TextEdit& edit = state.textEdit;

    switch (key) {
        case VK_LEFT:
            if (control) {
                edit.MoveWordLeft(shift);
            } else {
                edit.MoveLeft(shift);
            }
            break;
        case VK_RIGHT:
            if (control) {
                edit.MoveWordRight(shift);
            } else {
                edit.MoveRight(shift);
            }
            break;
        case VK_UP:
            edit.MoveUp(shift);
            break;
        case VK_DOWN:
            edit.MoveDown(shift);
            break;
        case VK_HOME:
            // Ctrl+Home metnin başı, yalnız Home satır başı: Windows'un kendi
            // metin kutularıyla aynı.
            if (control) {
                edit.MoveTo(0, shift);
            } else {
                edit.MoveHome(shift);
            }
            break;
        case VK_END:
            if (control) {
                edit.MoveTo(edit.text.size(), shift);
            } else {
                edit.MoveEnd(shift);
            }
            break;
        case VK_DELETE:
            edit.Delete();
            break;
        case 'A':
            if (!control) {
                return false;
            }
            edit.SelectAll();
            break;
        case 'C':
            if (!control) {
                return false;
            }
            // Seçim yokken de yutulur: düşseydi görüntüyü kopyalayan eylem
            // yazılan metni kesinleştirirdi ve kullanıcı bunu istememişti.
            CopySelection(window, state);
            break;
        case 'X':
            if (!control) {
                return false;
            }
            CopySelection(window, state);
            edit.EraseSelection();
            break;
        case 'V':
            if (!control) {
                return false;
            }
            PasteFromClipboard(window, state);
            break;
        default:
            return false;
    }

    AfterEdit(window, state);
    return true;
}

bool TextMouseDown(HWND window, State& state, POINT client) {
    if (!state.typing) {
        return false;
    }
    size_t pos = 0;
    if (!TextHitTest(window, state, client, pos)) {
        return false;   // kutunun dışı: çağıran metni kesinleştirir
    }
    // Shift+tık seçimi uzatır, düz tık imleci taşır: metin kutularının
    // evrensel davranışı.
    const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    state.textEdit.MoveTo(pos, shift);
    AfterEdit(window, state);
    return true;
}

}  // namespace editor
}  // namespace crisp
