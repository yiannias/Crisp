// PinMenu.cpp — İğnenin bağlam menüsü ve menünün uyguladığı komutlar.
//
// AYRI DOSYA: PinWindow.cpp pencerenin ömrünü ve mesajlarını anlatıyor ve
// 450 satıra dayanmıştı; üç yeni açma-kapama menüyle birlikte oraya
// sığmazdı (docs §9). Burada pencere OLUŞTURULMAZ, yalnızca var olanın durumu
// değiştirilir.
#include "PinInternal.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "resource.h"

#include <commdlg.h>

#include <string>

namespace crisp {
namespace pin {
namespace {

// Menü komutları yalnızca bu dosyada anlamlı; resource.h'yi kirletmezler.
enum PinCommand {
    kPinCopy = 1,
    kPinSaveAs,
    kPinActualSize,
    kPinOpacityFull,
    kPinOpacityHalf,
    kPinTopMost,
    kPinFrame,
    kPinClickThrough,
    kPinClose,
};

// Yarı saydam iğnenin alfa değeri: tamamen görünmezle "arkası okunur" arası.
constexpr BYTE kTranslucentAlpha = 140;

void SetOpacity(PinState& state, BYTE opacity) {
    state.opacity = opacity;
    ::SetLayeredWindowAttributes(state.window, 0, state.opacity, LWA_ALPHA);
}

}  // namespace

void ApplyZoom(PinState& state, int newZoom, POINT anchorScreen) {
    if (newZoom == state.zoom) {
        return;
    }

    RECT bounds{};
    if (!::GetWindowRect(state.window, &bounds)) {
        return;
    }

    const SIZE oldSize{geom::Width(bounds), geom::Height(bounds)};
    const SIZE newSize = geom::ScaledSize(
        SIZE{state.image.Width(), state.image.Height()}, newZoom);
    const POINT origin = geom::ZoomAnchoredOrigin(
        POINT{bounds.left, bounds.top}, anchorScreen, oldSize, newSize);

    state.zoom = newZoom;
    // Z-SIRASI DOKUNULMAZ (SWP_NOZORDER): burada HWND_TOPMOST vermek, "her
    // zaman üstte"yi kapatmış bir iğneyi her tekerlek adımında yeniden üste
    // çıkarırdı.
    ::SetWindowPos(state.window, nullptr, origin.x, origin.y, newSize.cx,
                   newSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);
    ::InvalidateRect(state.window, nullptr, FALSE);
}

void SaveAs(const PinState& state) {
    // UZUN YOL TAMPONU: MAX_PATH'ten uzun bir hedef seçildiğinde iletişim
    // kutusu FNERR_BUFFERTOOSMALL ile döner ve bu, iptal sanılırdı.
    std::wstring path(32768, L'\0');
    const std::wstring suggestion = L"Crisp " + TimestampForFileName() + L".png";
    ::wcscpy_s(path.data(), path.size(), suggestion.c_str());

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = state.window;
    // Dilden bağımsız süzgeç metni: düzenleyicinin "Farklı kaydet"iyle aynı.
    dialog.lpstrFilter = L"PNG (*.png)\0*.png\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!::GetSaveFileNameW(&dialog)) {
        return;   // kullanıcı iptal etti; hata değil
    }
    if (!SavePng(state.image, path)) {
        ShowMessage(::GetModuleHandleW(nullptr), state.window,
                    Loc::Str(IDS_SAVE_FAILED), MessageIcon::Error);
    }
}

void SetTopMost(PinState& state, bool topMost) {
    state.topMost = topMost;
    // Yalnızca z-sırası değişir; konum ve ölçü olduğu gibi kalır.
    ::SetWindowPos(state.window, topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0,
                   0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void SetFrame(PinState& state, bool frame) {
    state.frame = frame;
    // Çerçeve pencere ölçüsünü DEĞİŞTİRMEZ, kenarın içine çizilir (bkz.
    // PinWindow.cpp WM_PAINT); yeniden çizmek yeter.
    ::InvalidateRect(state.window, nullptr, FALSE);
}

void SetClickThrough(PinState& state, bool clickThrough) {
    state.clickThrough = clickThrough;

    // WS_EX_TRANSPARENT: fare olayları pencereye hiç uğramaz, alttakine gider.
    // Katmanlı (WS_EX_LAYERED) bir pencerede çalışması için ikisi birlikte
    // olmalı; iğne zaten katmanlı ama burada yine de istenir ki bu işlev
    // oluşturma kodundaki bayrağa bağımlı kalmasın.
    LONG_PTR exStyle = ::GetWindowLongPtrW(state.window, GWL_EXSTYLE);
    if (clickThrough) {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }
    ::SetWindowLongPtrW(state.window, GWL_EXSTYLE, exStyle);
    // Stil değişikliğinin çerçeveye işlemesi için: SetWindowLongPtr tek başına
    // pencere yöneticisine haber vermez.
    ::SetWindowPos(state.window, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);

    // İLK AÇILIŞTA BİR KEZ UYARI. Geçirgen bir iğne artık sağ tıklanamaz ve
    // menüden geri alınamaz; çıkış yolunu (tepsi menüsü / kısayol ile gizle,
    // ya da odaktayken Esc) bilmeden kullanıcı iğneyi ekrana "yapışmış"
    // sanırdı. Her seferinde göstermek ise özelliği kullanan birine zulüm
    // olurdu; oturumda bir kez yeter.
    static bool hintShown = false;
    if (clickThrough && !hintShown) {
        hintShown = true;
        ShowMessage(::GetModuleHandleW(nullptr), state.window,
                    Loc::Str(IDS_PIN_CLICKTHROUGH_HINT), MessageIcon::Information);
    }
}

void ShowContextMenu(PinState& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const std::wstring copyText = Loc::MenuText(IDS_PIN_COPY, IDS_ACCEL_COPY);
    const std::wstring saveText = Loc::MenuText(IDS_PIN_SAVE_AS, IDS_ACCEL_SAVE);
    const std::wstring sizeText = Loc::Str(IDS_PIN_ACTUAL_SIZE);
    const std::wstring opaqueText = Loc::Str(IDS_PIN_OPAQUE);
    const std::wstring translucentText = Loc::Str(IDS_PIN_TRANSLUCENT);
    const std::wstring topMostText = Loc::Str(IDS_PIN_TOPMOST);
    const std::wstring frameText = Loc::Str(IDS_PIN_FRAME);
    const std::wstring clickThroughText = Loc::Str(IDS_PIN_CLICKTHROUGH);
    const std::wstring closeText = Loc::MenuText(IDS_PIN_CLOSE, IDS_ACCEL_ESC);

    const auto checked = [](bool on) -> UINT {
        return on ? static_cast<UINT>(MF_CHECKED) : 0u;
    };

    ::AppendMenuW(menu, MF_STRING, kPinCopy, copyText.c_str());
    ::AppendMenuW(menu, MF_STRING, kPinSaveAs, saveText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kPinActualSize, sizeText.c_str());
    ::AppendMenuW(menu, MF_STRING | checked(state.opacity == 255),
                  kPinOpacityFull, opaqueText.c_str());
    ::AppendMenuW(menu, MF_STRING | checked(state.opacity != 255),
                  kPinOpacityHalf, translucentText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING | checked(state.topMost), kPinTopMost,
                  topMostText.c_str());
    ::AppendMenuW(menu, MF_STRING | checked(state.frame), kPinFrame,
                  frameText.c_str());
    ::AppendMenuW(menu, MF_STRING | checked(state.clickThrough),
                  kPinClickThrough, clickThroughText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kPinClose, closeText.c_str());

    POINT cursor{};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(state.window);

    const int command = ::TrackPopupMenuEx(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y,
        state.window, nullptr);
    ::DestroyMenu(menu);

    switch (command) {
        case kPinCopy:
            if (!CopyImageToClipboard(state.image, state.window)) {
                LogV(L"İğneden panoya kopyalama başarısız");
            }
            break;
        case kPinSaveAs:
            SaveAs(state);
            break;
        case kPinActualSize:
            ApplyZoom(state, 100, cursor);
            break;
        case kPinOpacityFull:
            SetOpacity(state, 255);
            break;
        case kPinOpacityHalf:
            SetOpacity(state, kTranslucentAlpha);
            break;
        case kPinTopMost:
            SetTopMost(state, !state.topMost);
            break;
        case kPinFrame:
            SetFrame(state, !state.frame);
            break;
        case kPinClickThrough:
            SetClickThrough(state, !state.clickThrough);
            break;
        case kPinClose:
            ::DestroyWindow(state.window);
            break;
        default:
            break;
    }
}

}  // namespace pin
}  // namespace crisp
