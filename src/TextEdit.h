// TextEdit.h — Düzenleyicinin metin aracı için imleçli, seçimli metin tamponu.
// TASLAK.
#pragma once

#include <string>

namespace crisp {

struct TextEdit {
    std::wstring text;
    size_t caret = 0;
    size_t anchor = 0;
};

}  // namespace crisp
