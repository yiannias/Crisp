// AppPins.cpp — İğne grubunu tek kısayolla gizleyip gösterme.
//
// TEK BİR EYLEM, İKİ YÖN: tepsi menüsündeki "Hide or show all pins" ve aynı
// adlı kısayol buraya düşer. Yön, ekranda görünen bir iğne olup olmadığına
// göre seçilir; kullanıcının "şu an gizli mi" diye düşünmesine gerek yok —
// bir bası ekranı boşaltır, bir bası daha geri getirir.
//
// TIKLAMA-GEÇİRGEN İĞNELERİN ÇIKIŞ YOLU DA BURASI: sağ tıklanamayan bir iğne
// menüsünü açamaz; onu saklamanın belgelenmiş yolu bu eylemdir ve gizleyip
// göstermek geçirgenliği bozmaz.
#include "App.h"

#include "Localization.h"
#include "PinWindow.h"
#include "Toast.h"
#include "resource.h"

namespace crisp {

void App::TogglePins() {
    // Hiç iğne yokken bildirim göstermek "neyi gizledim?" sorusunu doğururdu.
    if (OpenPinCount() == 0) {
        return;
    }

    const bool hide = AnyPinVisible();
    HideAllPins(hide);

    if (!m_settings.showNotification) {
        return;
    }
    // Diğer bildirimler gibi: küçük resimsiz, başlık yalnız.
    const Image none;
    ShowCaptureToast(m_instance, none,
                     Loc::Str(hide ? IDS_TOAST_PINS_HIDDEN : IDS_TOAST_PINS_SHOWN),
                     std::wstring(), std::wstring());
}

}  // namespace crisp
