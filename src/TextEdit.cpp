// TextEdit.cpp — İmleçli, seçimli metin tamponu. Gerekçe ve değişmezler
// başlıkta; burası yalnızca onları koruyan aritmetik.
#include "TextEdit.h"

#include <algorithm>

namespace crisp {
namespace {

[[nodiscard]] bool IsLead(wchar_t ch) noexcept {
    return ch >= 0xD800 && ch <= 0xDBFF;
}

[[nodiscard]] bool IsTrail(wchar_t ch) noexcept {
    return ch >= 0xDC00 && ch <= 0xDFFF;
}

// Kelime sınırı için "boşluk": satır sonu da ayraç sayılır, yoksa Ctrl+Sağ
// satırın sonundan bir sonraki satırın ilk kelimesinin sonuna atlardı.
[[nodiscard]] bool IsSpace(wchar_t ch) noexcept {
    return ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\r';
}

}  // namespace

size_t TextEdit::Snap(size_t pos) const noexcept {
    pos = (std::min)(pos, text.size());
    if (pos > 0 && pos < text.size() && IsTrail(text[pos]) &&
        IsLead(text[pos - 1])) {
        --pos;
    }
    return pos;
}

void TextEdit::Normalize() noexcept {
    caret = Snap(caret);
    anchor = Snap(anchor);
}

size_t TextEdit::PreviousBoundary(size_t pos) const noexcept {
    if (pos == 0) {
        return 0;
    }
    // Vekil çift tek adımda geçilir: ortasında durmak yasak.
    if (pos >= 2 && IsTrail(text[pos - 1]) && IsLead(text[pos - 2])) {
        return pos - 2;
    }
    return pos - 1;
}

size_t TextEdit::NextBoundary(size_t pos) const noexcept {
    if (pos >= text.size()) {
        return text.size();
    }
    if (pos + 1 < text.size() && IsLead(text[pos]) && IsTrail(text[pos + 1])) {
        return pos + 2;
    }
    return pos + 1;
}

void TextEdit::Place(size_t pos, bool extend) noexcept {
    caret = Snap(pos);
    if (!extend) {
        anchor = caret;
    }
}

// --- Seçim -------------------------------------------------------------------

bool TextEdit::HasSelection() const noexcept {
    return Snap(caret) != Snap(anchor);
}

std::pair<size_t, size_t> TextEdit::SelectionRange() const noexcept {
    const size_t a = Snap(caret);
    const size_t b = Snap(anchor);
    return a < b ? std::pair<size_t, size_t>{a, b}
                 : std::pair<size_t, size_t>{b, a};
}

std::wstring TextEdit::SelectedText() const {
    const auto [from, to] = SelectionRange();
    return text.substr(from, to - from);
}

void TextEdit::EraseSelection() {
    Normalize();
    if (!HasSelection()) {
        return;
    }
    const auto [from, to] = SelectionRange();
    text.erase(from, to - from);
    caret = anchor = from;
}

void TextEdit::SelectAll() noexcept {
    anchor = 0;
    caret = text.size();
}

void TextEdit::Clear() noexcept {
    text.clear();
    caret = anchor = 0;
}

// --- Düzenleme ---------------------------------------------------------------

void TextEdit::Insert(std::wstring_view piece) {
    // Seçili metnin üstüne yazmak onu DEĞİŞTİRİR: her düzenleyicide böyle ve
    // kullanıcı "seç, yaz" ile düzeltme yapabilmeli.
    EraseSelection();
    text.insert(caret, piece.data(), piece.size());
    caret += piece.size();
    anchor = caret;
}

void TextEdit::InsertChar(wchar_t ch) {
    Insert(std::wstring_view(&ch, 1));
}

void TextEdit::Backspace() {
    Normalize();
    if (HasSelection()) {
        EraseSelection();
        return;
    }
    const size_t from = PreviousBoundary(caret);
    text.erase(from, caret - from);
    caret = anchor = from;
}

void TextEdit::Delete() {
    Normalize();
    if (HasSelection()) {
        EraseSelection();
        return;
    }
    const size_t to = NextBoundary(caret);
    text.erase(caret, to - caret);
    anchor = caret;
}

// --- Hareket -----------------------------------------------------------------

void TextEdit::MoveTo(size_t pos, bool extend) noexcept {
    Normalize();
    Place(pos, extend);
}

void TextEdit::MoveLeft(bool extend) noexcept {
    Normalize();
    // Seçim varken Shift'siz sol ok seçimin BAŞINA çöker, bir adım daha
    // gitmez: Windows'un kendi metin kutuları böyle davranır.
    if (!extend && HasSelection()) {
        Place(SelectionRange().first, false);
        return;
    }
    Place(PreviousBoundary(caret), extend);
}

void TextEdit::MoveRight(bool extend) noexcept {
    Normalize();
    if (!extend && HasSelection()) {
        Place(SelectionRange().second, false);
        return;
    }
    Place(NextBoundary(caret), extend);
}

void TextEdit::MoveHome(bool extend) noexcept {
    Normalize();
    Place(LineStart(LineOf(caret)), extend);
}

void TextEdit::MoveEnd(bool extend) noexcept {
    Normalize();
    Place(LineEnd(caret), extend);
}

void TextEdit::MoveUp(bool extend) noexcept {
    Normalize();
    const int line = LineOf(caret);
    if (line == 0) {
        // İlk satırda yukarı gidecek yer yok; metnin başına gider. Hiçbir
        // şey yapmamak, tuşun bozuk olduğu izlenimini verirdi.
        Place(0, extend);
        return;
    }
    const size_t column = ColumnOf(caret);
    const size_t start = LineStart(line - 1);
    const size_t end = LineEnd(start);
    Place((std::min)(start + column, end), extend);
}

void TextEdit::MoveDown(bool extend) noexcept {
    Normalize();
    const int line = LineOf(caret);
    if (line + 1 >= LineCount()) {
        Place(text.size(), extend);
        return;
    }
    const size_t column = ColumnOf(caret);
    const size_t start = LineStart(line + 1);
    const size_t end = LineEnd(start);
    Place((std::min)(start + column, end), extend);
}

void TextEdit::MoveWordLeft(bool extend) noexcept {
    Normalize();
    size_t pos = caret;
    while (pos > 0 && IsSpace(text[pos - 1])) {
        --pos;
    }
    while (pos > 0 && !IsSpace(text[pos - 1])) {
        --pos;
    }
    Place(pos, extend);
}

void TextEdit::MoveWordRight(bool extend) noexcept {
    Normalize();
    size_t pos = caret;
    // Önce kelimenin sonuna, sonra ardındaki boşlukların sonuna: imleç bir
    // sonraki kelimenin BAŞINA iner, Windows'un Ctrl+Sağ'ı gibi.
    while (pos < text.size() && !IsSpace(text[pos])) {
        ++pos;
    }
    while (pos < text.size() && IsSpace(text[pos])) {
        ++pos;
    }
    Place(pos, extend);
}

// --- Satır/sütun -------------------------------------------------------------

int TextEdit::LineOf(size_t pos) const noexcept {
    pos = (std::min)(pos, text.size());
    return static_cast<int>(std::count(text.begin(), text.begin() +
                                       static_cast<std::ptrdiff_t>(pos), L'\n'));
}

int TextEdit::LineCount() const noexcept {
    return LineOf(text.size()) + 1;
}

size_t TextEdit::LineStart(int line) const noexcept {
    size_t pos = 0;
    for (int i = 0; i < line; ++i) {
        const size_t next = text.find(L'\n', pos);
        if (next == std::wstring::npos) {
            break;   // istenen satır yok; son satırın başı döner
        }
        pos = next + 1;
    }
    return pos;
}

size_t TextEdit::LineEnd(size_t pos) const noexcept {
    pos = (std::min)(pos, text.size());
    const size_t next = text.find(L'\n', pos);
    return next == std::wstring::npos ? text.size() : next;
}

size_t TextEdit::ColumnOf(size_t pos) const noexcept {
    pos = (std::min)(pos, text.size());
    return pos - LineStart(LineOf(pos));
}

std::wstring TextEdit::Sanitize(std::wstring_view raw) {
    std::wstring out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const wchar_t ch = raw[i];
        if (ch == L'\r') {
            out.push_back(L'\n');
            if (i + 1 < raw.size() && raw[i + 1] == L'\n') {
                ++i;   // "\r\n" tek satır sonu
            }
        } else if (ch == L'\t') {
            out.append(4, L' ');
        } else if (ch == L'\n' || ch >= L' ') {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace crisp
