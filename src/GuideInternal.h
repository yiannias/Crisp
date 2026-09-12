// GuideInternal.h — GuideCompose.cpp ile GuideDraw.cpp arasında paylaşılan
// çizim yardımcıları. Dışarıya açık arayüz GuideCompose.h'dedir.
//
// NEDEN AYRI DOSYA: yerleşim aritmetiği (kim nereye, hangi boyutta) ile
// piksel işi (çerçeve, rozet, numara) tek dosyada 400 satır sınırını
// zorluyordu ve ikisi ayrı ayrı okunur — biri sayı, diğeri boya.
#pragma once

#include "Capture.h"
#include "GuideCompose.h"

#include <windows.h>

#include <cstdint>
#include <vector>

namespace crisp::guide {

// Bir adımın sonuç görüntüsündeki yeri (küçültülmüş boyutuyla).
struct Placement {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Görüntünün hemen DIŞINA, kenar boşluğuna bir piksellik çerçeve çizer. İçine
// çizmek görüntünün kendi kenar pikselini yutardı.
void DrawFrame(Image& out, const Placement& at, uint32_t color) noexcept;

// Rozetin merkezi: adımın sol-üst köşesi, ama rozet tuvalden taşmayacak
// şekilde içeri itilmiş. Kenar boşluğu yarıçaptan darsa rozet köşeyi tam
// ortalamaz ama yine de tamamen görünür.
[[nodiscard]] POINT BadgeCentre(const Placement& at, int diameter) noexcept;

// Kenarları yumuşatılmış dolu daire; GDI Ellipse yumuşatma yapmaz ve
// 44 piksellik bir dairede tırtıklı kenar hemen göze çarpar.
void DrawBadgeDisk(Image& out, POINT centre, int diameter, uint32_t color) noexcept;

// Her rozetin numarasını (1'den başlayarak) tek bir bellek DC'si üzerinden
// çizer. Metin GDI ile çizildiği için alfa baytı tanımsız kalır; bu işlev
// çıkmadan önce rozet bölgelerinin alfasını 255'e sabitler.
[[nodiscard]] bool DrawBadgeNumbers(Image& out, const std::vector<POINT>& centres,
                                    const GuideOptions& options);

}  // namespace crisp::guide
