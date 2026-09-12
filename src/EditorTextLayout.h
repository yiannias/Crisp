// EditorTextLayout.h — Yazılmakta olan metnin PİKSEL geometrisi: tampon
// konumu ↔ ekran noktası çevirisi, seçim şeritleri, kutu ölçüsü.
//
// AYRI DOSYA: EditorText.cpp tuşları ve çizimi taşıyor; ölçüm kodu da oraya
// girince 400 satır aşılıyordu (ev kuralı §9). Ayrım işlevsel de — burası
// "hangi piksel", komşusu "hangi tuş ne yapar" sorusuna bakar.
//
// Yalnızca düzenleyicinin metin dosyaları içerir; EditorInternal.h'ye
// eklenmedi, çünkü o başlık zaten dolu ve bu işlevleri başka kimse çağırmaz.
#pragma once

#include "EditorInternal.h"

#include <string>

namespace crisp {
namespace editor {

// Taslak kutusunun ölçüleri. Yazı tipi DC'de SEÇİLİ olmalı; ölçüm ona göre.
struct TextDraftLayout {
    POINT origin{};        // ilk satırın sol üst köşesi (istemci pikseli)
    int lineHeight = 0;    // DrawText'in satır aralığı (tmHeight)
    RECT box{};            // kesikli çerçeve, dolgu payı dahil
    bool empty = true;     // tampon boş: kutuda ipucu görünüyor
    std::wstring shown;    // çizilen metin (boşken ipucu)
};

[[nodiscard]] TextDraftLayout MeasureTextDraft(HDC dc, const State& state);

// Tampon konumunun piksel yeri: o satırın imleçten önceki kısmının genişliği.
[[nodiscard]] POINT TextCaretPixel(HDC dc, const TextEdit& edit,
                                   const TextDraftLayout& layout, size_t pos);

// İstemci noktasına en yakın tampon konumu; vekil çiftin ortasına düşmez.
[[nodiscard]] size_t TextPositionAt(HDC dc, const TextEdit& edit,
                                    const TextDraftLayout& layout, POINT client);

// Seçili aralığın arka planını satır satır boyar.
void DrawTextSelection(HDC dc, const TextEdit& edit,
                       const TextDraftLayout& layout, COLORREF color);
[[nodiscard]] COLORREF TextSelectionColor() noexcept;

// Boyama dışından (klavye, fare, IME) ölçüm: pencerenin DC'sini ödünç alıp
// taslağın yazı tipiyle ölçer. Boşken kutunun başlangıcını verir.
[[nodiscard]] POINT TextCaretClient(HWND window, const State& state);
// Nokta kutunun içindeyse en yakın tampon konumunu `pos`a yazıp true döner.
[[nodiscard]] bool TextHitTest(HWND window, const State& state, POINT client,
                               size_t& pos);

}  // namespace editor
}  // namespace crisp
