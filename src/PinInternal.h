// PinInternal.h — İğne penceresinin İÇ paylaşımı.
//
// PinWindow.cpp (pencere ömrü, mesajlar, çizim) ile PinMenu.cpp (bağlam
// menüsü ve menünün uyguladığı komutlar) arasında paylaşılır; dışarıya açık
// değildir. Ayrım ev kuralının 400 satır sınırından doğdu: üç açma-kapama
// (üstte, çerçeve, tıklama-geçirgen) menüyle birlikte PinWindow.cpp'yi
// sınırın çok üstüne taşıyordu.
#pragma once

#include "Capture.h"
#include "Util.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace crisp {
namespace pin {

struct PinState {
    Image image;
    unique_hdc imageDc;
    int zoom = 100;
    BYTE opacity = 255;
    HWND window = nullptr;

    // Üç açma-kapama. Pencere stilinden (WS_EX_TOPMOST / WS_EX_TRANSPARENT)
    // geri okunabilirlerdi, ama diske yazarken ve menüde işaret koyarken tek
    // bir doğruluk kaynağı olsun diye burada da tutulurlar; stil bunları
    // İZLER, tersi değil.
    bool topMost = true;
    bool frame = false;
    bool clickThrough = false;

    // Bkz. PinView::transient. Kalıcı listeye girmez, tıkla kapanır.
    bool transient = false;
};

// Geçici iğnenin kendi kendine kapanma zamanlayıcısı.
inline constexpr UINT_PTR kAutoCloseTimer = 1;

// --- PinWindow.cpp ----------------------------------------------------------

// Açık iğneler. unique_ptr, PinState'in adresinin liste büyüdükçe
// değişmemesini garanti eder — pencere verisi (GWLP_USERDATA) o adrese işaret
// ediyor. Liste yalnızca PinWindow.cpp'de değişir; PinPersist.cpp okur.
[[nodiscard]] std::vector<std::unique_ptr<PinState>>& Pins();

// --- PinMenu.cpp ------------------------------------------------------------

// Yakınlaştırmayı `anchorScreen` sabit kalacak biçimde uygular.
void ApplyZoom(PinState& state, int newZoom, POINT anchorScreen);

// "Farklı kaydet" iletişim kutusu; iptal hata değildir.
void SaveAs(const PinState& state);

// Üç açma-kapamanın uygulanması. Menü de klavye de bunları çağırır; durumu
// ve pencere stilini birlikte değiştiren TEK yer burasıdır.
void SetTopMost(PinState& state, bool topMost);
void SetFrame(PinState& state, bool frame);
void SetClickThrough(PinState& state, bool clickThrough);

// Sağ tık menüsü; seçilen komutu uygular.
void ShowContextMenu(PinState& state);

}  // namespace pin
}  // namespace crisp
