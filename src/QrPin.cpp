// QrPin.cpp — QR kartı: beyaz zemin, kod, altında bağlantı ve ipucu.
#include "QrPin.h"

#include "Capture.h"
#include "Localization.h"
#include "PinWindow.h"
#include "QrCode.h"
#include "UiCommon.h"
#include "UploadInternal.h"
#include "Util.h"
#include "resource.h"

#include <algorithm>
#include <cstdint>

namespace crisp {
namespace {

// 96 DPI'da tasarlanmış ölçüler.
constexpr int kQrPixels = 220;   // kodun hedef kenarı
constexpr int kPad = 16;         // kartın iç boşluğu
constexpr int kLineHeight = 18;  // metin satırı
constexpr int kMinCardWidth = 260;
constexpr int kMargin = 24;      // çalışma alanının kenarından uzaklık

constexpr uint32_t kWhite = 0xFFFFFFFFu;
constexpr COLORREF kTextColor = RGB(32, 32, 32);
constexpr COLORREF kHintColor = RGB(110, 110, 110);

// GDI metni DIB'e yazarken alfa baytını sıfırlıyor. İğne penceresi alfayı
// okumuyor ama iğneden "farklı kaydet" PNG'ye gidiyor ve şeffaf harfler o
// dosyada görünmez olurdu. Her pikseli opak yap.
void ForceOpaque(Image& image) noexcept {
    auto* pixels = static_cast<uint32_t*>(image.Bits());
    const size_t count = static_cast<size_t>(image.Width()) *
                         static_cast<size_t>(image.Height());
    for (size_t i = 0; i < count; ++i) {
        pixels[i] |= 0xFF000000u;
    }
}

[[nodiscard]] RECT WorkAreaFor(HWND reference) noexcept {
    POINT anchor{};
    if (reference != nullptr && ::IsWindowVisible(reference) != FALSE) {
        RECT window{};
        ::GetWindowRect(reference, &window);
        anchor.x = (window.left + window.right) / 2;
        anchor.y = (window.top + window.bottom) / 2;
    } else {
        ::GetCursorPos(&anchor);
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = ::MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && ::GetMonitorInfoW(monitor, &info) != FALSE) {
        return info.rcWork;
    }
    return MonitorRectAtPoint(anchor);
}

}  // namespace

bool PinQrForLink(HINSTANCE instance, HWND reference, const std::wstring& link) {
    QrCode code;
    if (link.empty() || !EncodeQr(WideToUtf8(link), code)) {
        LogV(L"QR: bağlantı kodlanamadı (%zu karakter)", link.size());
        return false;
    }

    const unsigned dpi = reference != nullptr ? ::GetDpiForWindow(reference) : 96u;
    Image qr;
    if (!RenderQrToSize(code, Scale(kQrPixels, dpi), 2, qr)) {
        return false;
    }

    const int pad = Scale(kPad, dpi);
    const int line = Scale(kLineHeight, dpi);
    const int width = std::max(qr.Width() + 2 * pad, Scale(kMinCardWidth, dpi));
    const int height = pad + qr.Height() + Scale(8, dpi) + 2 * line + pad;

    Image card;
    if (!card.Create(width, height)) {
        return false;
    }
    card.Fill(kWhite);

    {
        const HDC screen = ::GetDC(nullptr);
        const unique_hdc dc{::CreateCompatibleDC(screen)};
        const unique_hdc source{::CreateCompatibleDC(screen)};
        ::ReleaseDC(nullptr, screen);
        if (!dc || !source) {
            return false;
        }
        const HGDIOBJ oldCard = ::SelectObject(dc.get(), card.Handle());
        const HGDIOBJ oldQr = ::SelectObject(source.get(), qr.Handle());

        const int qrLeft = (width - qr.Width()) / 2;
        ::BitBlt(dc.get(), qrLeft, pad, qr.Width(), qr.Height(), source.get(), 0, 0,
                 SRCCOPY);

        // İKİ SATIR METİN: bağlantı ve ipucu. Bağlantı karttan uzunsa
        // ortadan kısaltılır — sonu (dosya adı) çoğu zaman başından ayırt
        // edicidir ve kod zaten tamamını taşıyor.
        const HFONT fontLink = CreateUiFont(dpi, 9, FW_SEMIBOLD);
        const HFONT fontHint = CreateUiFont(dpi, 9, FW_NORMAL);
        ::SetBkMode(dc.get(), TRANSPARENT);

        RECT linkArea{pad, pad + qr.Height() + Scale(8, dpi), width - pad, 0};
        linkArea.bottom = linkArea.top + line;
        const HGDIOBJ oldFont = ::SelectObject(dc.get(), fontLink);
        ::SetTextColor(dc.get(), kTextColor);
        ::DrawTextW(dc.get(), link.c_str(), -1, &linkArea,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS |
                        DT_NOPREFIX);

        RECT hintArea{pad, linkArea.bottom, width - pad, linkArea.bottom + line};
        ::SelectObject(dc.get(), fontHint);
        ::SetTextColor(dc.get(), kHintColor);
        const std::wstring hint = Loc::Str(IDS_QR_SCAN_HINT);
        ::DrawTextW(dc.get(), hint.c_str(), -1, &hintArea,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                        DT_NOPREFIX);

        // KAPATMA DÜĞMESİ: sağ üst köşede koyu bir daire ve beyaz bir çarpı.
        // Kartın her yeri tıkla-kapanır ama bunu söyleyen bir şey olmalı;
        // ipucu satırı okunmadan da anlaşılan evrensel işaret bu.
        {
            const int diameter = Scale(22, dpi);
            const RECT button{width - pad / 2 - diameter, pad / 2,
                              width - pad / 2, pad / 2 + diameter};
            const HBRUSH fill = ::CreateSolidBrush(RGB(60, 60, 60));
            const HPEN edge = ::CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            const HPEN cross =
                ::CreatePen(PS_SOLID, (std::max)(2, Scale(2, dpi)), RGB(255, 255, 255));
            if (fill != nullptr && edge != nullptr && cross != nullptr) {
                const HGDIOBJ oldBrush = ::SelectObject(dc.get(), fill);
                const HGDIOBJ oldPen = ::SelectObject(dc.get(), edge);
                ::Ellipse(dc.get(), button.left, button.top, button.right, button.bottom);
                ::SelectObject(dc.get(), cross);
                const int inset = diameter * 3 / 10;
                ::MoveToEx(dc.get(), button.left + inset, button.top + inset, nullptr);
                ::LineTo(dc.get(), button.right - inset, button.bottom - inset);
                ::MoveToEx(dc.get(), button.right - inset, button.top + inset, nullptr);
                ::LineTo(dc.get(), button.left + inset, button.bottom - inset);
                ::SelectObject(dc.get(), oldPen);
                ::SelectObject(dc.get(), oldBrush);
            }
            if (fill != nullptr) {
                ::DeleteObject(fill);
            }
            if (edge != nullptr) {
                ::DeleteObject(edge);
            }
            if (cross != nullptr) {
                ::DeleteObject(cross);
            }
        }

        ::SelectObject(dc.get(), oldFont);
        ::SelectObject(dc.get(), oldCard);
        ::SelectObject(source.get(), oldQr);
        ::DeleteObject(fontLink);
        ::DeleteObject(fontHint);
    }
    ForceOpaque(card);

    const RECT work = WorkAreaFor(reference);
    const int margin = Scale(kMargin, dpi);
    POINT topLeft{work.right - width - margin, work.bottom - height - margin};
    if (topLeft.x < work.left) {
        topLeft.x = work.left;
    }
    if (topLeft.y < work.top) {
        topLeft.y = work.top;
    }
    // KART BİR BİLDİRİMDİR, KALICI BİR İĞNE DEĞİL: tıklayınca kapanır, bir
    // dakika sonra kendi kapanır, çıkışta kaydedilmez. Sağ tık menüsü yine
    // çalışır (kopyala, farklı kaydet).
    PinView view;
    view.transient = true;
    view.autoCloseMs = 60000;
    return PinImageWithView(instance, card, topLeft, view);
}

}  // namespace crisp
