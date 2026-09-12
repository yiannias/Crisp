// UiCommon.h — Özel çizilen pencerelerin ortak GDI yardımcıları.
//
// NEDEN VAR: Scale, CreateUiFont ve FillRectColor sekiz ayrı dosyada birebir
// aynı gövdeyle tekrarlanıyordu (Hakkında, ileti kutusu, geçmiş, ayarlar,
// düzenleyici, renk ve kalınlık seçiciler, bildirim). Yazı tipi kalitesini
// ya da yüzü değiştirmek sekiz yerde değişiklik demekti ve biri hep unutulurdu.
//
// HEPSİ `inline`: gövdeler birkaç satır ve her boyamada onlarca kez çağrılıyor;
// ayrı bir çeviri birimi yalnızca bağlantı sırası eklerdi.
//
// Kaplamanın kendi kopyaları (OverlayDraw.h) ayrı kalır: oradaki Scale LONG
// alır ve `draw` ad alanı bu başlıkla birlikte kullanıldığında aşırı yükleme
// karışırdı.
#pragma once

#include <windows.h>

namespace crisp {

// 96 DPI'da tasarlanmış bir ölçüyü verilen DPI'ya çevirir.
[[nodiscard]] inline int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

// DPI'ya göre ölçeklenmiş arayüz yazı tipi. Segoe UI Windows 10/11'de daima
// vardır; bulunamazsa GDI en yakınını seçer.
[[nodiscard]] inline HFONT CreateUiFont(unsigned dpi, int pointSize, int weight,
                                        const wchar_t* face = L"Segoe UI") {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, face);
    return ::CreateFontIndirectW(&font);
}

inline void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

// WM_MOUSELEAVE İSTENMEDEN GELMEZ. Fare pencereden çıktığında vurguyu
// söndürmek isteyen her pencere WM_MOUSEMOVE'da bunu çağırır; tekrarlanan
// çağrı zararsızdır, Windows aynı isteği ikinci kez kaydetmez.
inline void TrackMouseLeave(HWND window) noexcept {
    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = window;
    ::TrackMouseEvent(&track);
}

// Kendi mesaj döngüsünü işleten bir pencere WM_CREATE'te -1 dönerse Windows
// onu hemen yok eder ve WM_DESTROY'daki PostQuitMessage kuyruğa bir WM_QUIT
// bırakır. Pencereyi açan fonksiyon döngüye hiç girmediği için o WM_QUIT
// uygulamanın ana döngüsüne düşer ve tepsi uygulaması sessizce kapanırdı.
// Oluşturma başarısız olduğunda çağrılır: sahipsiz WM_QUIT atılır.
inline void DiscardPendingQuit() noexcept {
    MSG message{};
    while (::PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
    }
}

}  // namespace crisp
