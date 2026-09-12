// QrPin.h — Yükleme bağlantısını QR kodu olarak ekrana iğneler.
//
// NEDEN VAR: bağlantı panoya düşüyor ama telefona geçmesi gerekiyorsa
// yapıştıracak bir yer yok. Kod kartını bir iğne penceresinde göstermek,
// iğnelerin zaten bildiği her şeyi (sürükleme, kapatma, kaydetme) bedavaya
// getiriyor; ayrı bir QR penceresi aynı şeyi ikinci kez yazmak olurdu.
//
// İki çağıranı var — tepsi akışındaki otomatik yükleme ile düzenleyicideki
// yükleme — ve ikisi de aynı kartı istiyor; kart burada bir kez kurulur.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Bağlantıyı kodlar, altına bağlantı metni ile "telefonla tara" ipucunu
// yazar ve `reference` penceresinin (yoksa imlecin) bulunduğu monitörün
// çalışma alanının sağ altına iğneler. Kodlanamayan ya da çizilemeyen
// bağlantıda false; çağıran bunu sessizce geçer, bağlantı zaten panoda.
[[nodiscard]] bool PinQrForLink(HINSTANCE instance, HWND reference,
                                const std::wstring& link);

}  // namespace crisp
