// PinWindow.cpp — bkz. PinWindow.h.
//
// Bağlam menüsü ve menünün komutları PinMenu.cpp'de; bu dosya pencerenin
// ömrünü, mesajlarını ve çizimini anlatır.
#include "PinWindow.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "PinInternal.h"
#include "Theme.h"
#include "resource.h"
#include "Util.h"

// windowsx.h: GET_X_LPARAM / GET_Y_LPARAM. Elle kaydırmak yerine makro
// kullanılır çünkü koordinatlar İŞARETLİDİR ve çok monitörlü kurulumda
// negatif olabilir; LOWORD ile çıkarmak onları 65000 gibi değerlere çevirir.
#include <windowsx.h>

#include <memory>
#include <vector>

namespace crisp {
namespace {

using pin::PinState;

constexpr const wchar_t* kWindowClass = L"CrispPinWindow";

// Çerçevenin kalınlığı. KENARIN İÇİNE ÇİZİLİR, pencere büyütülmez: pencere
// ölçüsü görüntü ölçüsüne eşit kalınca yakınlaştırma, konum kaydetme ve
// çift tıkla gerçek boyut hesaplarının hiçbiri çerçeveden haberdar olmak
// zorunda kalmaz. Bedeli görüntünün en dıştaki iki pikselinin örtülmesi ve
// bu, çerçeveyi açan kullanıcının zaten kabul ettiği bir şey.
constexpr int kFrameThickness = 2;

void ForgetPin(PinState* state) noexcept {
    auto& pins = pin::Pins();
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        if (it->get() == state) {
            pins.erase(it);
            return;
        }
    }
}

void PaintPin(HWND window, const PinState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);

    // COLORONCOLOR yerine HALFTONE: iğne küçültüldüğünde metin okunabilir
    // kalsın. Büyütmede piksel bloklarını korumak için 100%'ün üstünde ham
    // kopyaya düşülür.
    if (state.zoom > 100) {
        ::SetStretchBltMode(dc, COLORONCOLOR);
    } else {
        ::SetStretchBltMode(dc, HALFTONE);
        ::SetBrushOrgEx(dc, 0, 0, nullptr);
    }

    ::StretchBlt(dc, 0, 0, geom::Width(client), geom::Height(client),
                 state.imageDc.get(), 0, 0, state.image.Width(),
                 state.image.Height(), SRCCOPY);

    // Çerçeve İSTEĞE BAĞLI: iğnenin nerede bitip masaüstünün nerede başladığı
    // benzer renkli bir arka planda belirsiz kalabiliyor, ama her iğnenin
    // etrafında bir çizgi de görüntüyü karşılaştırırken dikkat dağıtıyordu.
    // Kullanıcı isterse açar. Vurgu rengi, kalınlık kadar iç içe FrameRect.
    if (state.frame) {
        const HBRUSH brush = ::CreateSolidBrush(theme::Colors().accent);
        if (brush != nullptr) {
            RECT edge = client;
            for (int i = 0; i < kFrameThickness; ++i) {
                ::FrameRect(dc, &edge, brush);
                ::InflateRect(&edge, -1, -1);
            }
            ::DeleteObject(brush);
        }
    }
    ::EndPaint(window, &paint);
}

// Odaktaki iğnenin klavyesi. Esc kapatır; Ctrl+C / Ctrl+S menüdekiyle aynı.
// F çerçeveyi, T "her zaman üstte"yi açıp kapar — menü metinlerinde
// yazmayan, ama tıklama-geçirgen bir iğnede (sağ tık çalışmazken) hâlâ
// ulaşılabilir olan tek yol bu.
[[nodiscard]] bool HandleKey(PinState& state, WPARAM key) {
    if (key == VK_ESCAPE) {
        ::DestroyWindow(state.window);
        return true;
    }
    const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (control && key == 'C') {
        (void)CopyImageToClipboard(state.image, state.window);
        return true;
    }
    if (control && key == 'S') {
        pin::SaveAs(state);
        return true;
    }
    if (!control && key == 'F') {
        pin::SetFrame(state, !state.frame);
        return true;
    }
    if (!control && key == 'T') {
        pin::SetTopMost(state, !state.topMost);
        return true;
    }
    return false;
}

LRESULT CALLBACK PinProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state =
        reinterpret_cast<PinState*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }

        case WM_PAINT:
            if (state != nullptr) {
                PaintPin(window, *state);
            } else {
                ::ValidateRect(window, nullptr);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        // Pencerenin her yeri başlık çubuğu gibi davranır: kullanıcı görüntünün
        // herhangi bir yerinden tutup sürükleyebilir.
        case WM_NCHITTEST:
            return HTCAPTION;

        case WM_MOUSEWHEEL: {
            if (state == nullptr) {
                break;
            }
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            pin::ApplyZoom(*state, geom::ZoomStep(state->zoom, delta), cursor);
            return 0;
        }

        // WM_NCHITTEST her yeri başlık çubuğu saydığı için çift tık istemci
        // değil, İSTEMCİ DIŞI çift tık olarak gelir; WM_LBUTTONDBLCLK hiç
        // ulaşmıyordu ve "çift tık = gerçek boyut" çalışmıyordu.
        case WM_NCLBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK: {
            if (state != nullptr) {
                POINT cursor{};
                ::GetCursorPos(&cursor);
                pin::ApplyZoom(*state, 100, cursor);
            }
            return 0;
        }

        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            if (state != nullptr) {
                pin::ShowContextMenu(*state);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (state != nullptr && HandleKey(*state, wParam)) {
                return 0;
            }
            break;

        case WM_NCDESTROY: {
            // Durum burada silinir: WM_DESTROY'da silmek, sonrasında gelen
            // mesajların serbest bırakılmış belleğe erişmesine yol açardı.
            if (state != nullptr) {
                ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
                ForgetPin(state);
            }
            return 0;
        }

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    // CS_DBLCLKS olmadan WM_LBUTTONDBLCLK hiç gelmez.
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = PinProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

// Pencereyi ekranın içinde tutar: yakalama sağ kenardan yapıldıysa iğne
// tamamen görünmez bir yere açılabilirdi.
[[nodiscard]] RECT PlaceOnScreen(POINT topLeft, int width, int height) {
    const RECT screen = VirtualScreenRect();
    RECT placement{topLeft.x, topLeft.y, topLeft.x + width, topLeft.y + height};
    if (placement.right > screen.right) {
        placement.left -= placement.right - screen.right;
    }
    if (placement.bottom > screen.bottom) {
        placement.top -= placement.bottom - screen.bottom;
    }
    if (placement.left < screen.left) {
        placement.left = screen.left;
    }
    if (placement.top < screen.top) {
        placement.top = screen.top;
    }
    return placement;
}

}  // namespace

// İşlev içi static: ev kuralı global değişkeni yasaklar ama .cpp içindeki
// static'e izin verir; başlıkta yalnızca erişimcinin bildirimi var.
std::vector<std::unique_ptr<PinState>>& pin::Pins() {
    static std::vector<std::unique_ptr<PinState>> pins;
    return pins;
}

bool PinImageToScreen(HINSTANCE instance, const Image& image, POINT topLeft) {
    return PinImageWithView(instance, image, topLeft, PinView{});
}

bool PinImageWithView(HINSTANCE instance, const Image& image, POINT topLeft,
                      int zoom, unsigned opacity) {
    PinView view;
    view.zoom = zoom;
    view.opacity = opacity;
    return PinImageWithView(instance, image, topLeft, view);
}

bool PinImageWithView(HINSTANCE instance, const Image& image, POINT topLeft,
                      const PinView& view) {
    if (!image.Valid() || !EnsureWindowClass(instance)) {
        return false;
    }

    auto state = std::make_unique<PinState>();
    state->zoom = view.zoom;
    state->opacity = static_cast<BYTE>(view.opacity);
    state->topMost = view.topMost;
    state->frame = view.frame;
    state->clickThrough = view.clickThrough;

    // Görüntü kopyalanır: çağıranın Image'ı bu çağrıdan sonra yok olabilir.
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), state->image)) {
        return false;
    }

    const window_dc screenDc{nullptr};
    if (!screenDc.valid()) {
        return false;
    }
    state->imageDc.reset(::CreateCompatibleDC(screenDc.get()));
    if (!state->imageDc) {
        return false;
    }
    ::SelectObject(state->imageDc.get(), state->image.Handle());

    // PENCERE ÖLÇÜSÜ YAKINLAŞTIRMAYA GÖRE. %150'de bırakılmış bir iğne
    // görüntünün ham ölçüsüyle açılsaydı, geri gelen pencere kullanıcının
    // bıraktığından küçük olurdu.
    const int shownWidth = ::MulDiv(state->image.Width(), state->zoom, 100);
    const int shownHeight = ::MulDiv(state->image.Height(), state->zoom, 100);
    const RECT placement = PlaceOnScreen(topLeft, shownWidth, shownHeight);

    // Stil daha oluşturulurken görünüme göre kurulur: önce üstte açıp sonra
    // indirmek, kapalı bıraktığı "her zaman üstte"yi bir anlığına da olsa
    // kullanıcıya göstermek olurdu.
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (state->topMost) {
        exStyle |= WS_EX_TOPMOST;
    }
    if (state->clickThrough) {
        exStyle |= WS_EX_TRANSPARENT;
    }

    const std::wstring pinTitle = Loc::Str(IDS_PIN_TITLE);
    PinState* raw = state.get();
    const HWND window = ::CreateWindowExW(
        exStyle, kWindowClass, pinTitle.c_str(), WS_POPUP, placement.left,
        placement.top, shownWidth, shownHeight, nullptr, nullptr, instance,
        raw);

    if (window == nullptr) {
        LogV(L"İğne penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        return false;
    }

    raw->window = window;
    theme::ApplyToWindow(window);
    ::SetLayeredWindowAttributes(window, 0, raw->opacity, LWA_ALPHA);
    pin::Pins().push_back(std::move(state));

    // GİZLİ GERİ YÜKLENEN İĞNE GÖSTERİLMEZ. Kullanıcı çıkmadan önce tepsiden
    // "gizle" demişti; açılışta hepsinin geri fırlaması o tercihi çiğnerdi.
    // Pencere WS_VISIBLE olmadan oluşturuldu; ShowWindow'u atlamak yeter.
    if (!view.hidden) {
        ::ShowWindow(window, SW_SHOWNOACTIVATE);
        ::UpdateWindow(window);
    }
    return true;
}

void CloseAllPins() noexcept {
    // Kopya üzerinden gezilir: DestroyWindow, WM_NCDESTROY üzerinden listeyi
    // değiştirir ve canlı listede yineleme geçersiz yineleyiciye düşerdi.
    std::vector<HWND> windows;
    for (const auto& entry : pin::Pins()) {
        windows.push_back(entry->window);
    }
    for (const HWND window : windows) {
        if (window != nullptr) {
            ::DestroyWindow(window);
        }
    }
}

void HideAllPins(bool hide) noexcept {
    for (const auto& entry : pin::Pins()) {
        if (entry != nullptr && entry->window != nullptr) {
            // SW_SHOWNOACTIVATE: kısayoldan gösterilen iğneler odağı
            // kullanıcının o an çalıştığı pencereden çalmamalı.
            ::ShowWindow(entry->window, hide ? SW_HIDE : SW_SHOWNOACTIVATE);
        }
    }
}

bool AnyPinVisible() noexcept {
    for (const auto& entry : pin::Pins()) {
        if (entry != nullptr && entry->window != nullptr &&
            ::IsWindowVisible(entry->window) != FALSE) {
            return true;
        }
    }
    return false;
}

int OpenPinCount() noexcept {
    return static_cast<int>(pin::Pins().size());
}

}  // namespace crisp
