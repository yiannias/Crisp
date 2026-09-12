// AppUpdate.cpp — GitHub Releases üzerinden sürüm denetimi.
//
// AppUpload.cpp ile aynı düzen: ağ arka planda, arayüze dokunan her şey
// pencere iş parçacığında, aradaki tek köprü bir pencere mesajı. Bu dosya
// yalnızca "ne zaman sor" ve "sonucu nasıl söyle" sorularına cevap veriyor;
// yanıtı okuma ve sürüm karşılaştırma çekirdekte (UpdateCheck.cpp), istek
// HttpGet.cpp'de.
//
// KAPALI GELMİYOR AMA SESSİZ: açılıştaki denetim yalnızca yeni sürüm varsa
// konuşur. Ağ yoksa, GitHub yoksa ya da yanıt bozuksa hiçbir kutu açılmaz —
// kullanıcı bir ekran görüntüsü aracı açtı, bir ağ tanılama aracı değil.
// Menüden elle başlatılan denetim ise her durumda cevap verir; sessizlik
// orada "çalışmadı" anlamına gelirdi.
#include "App.h"

#include "HttpGet.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Messages.h"
#include "UpdateCheck.h"
#include "Util.h"
#include "Version.h"
#include "resource.h"

#include <memory>
#include <string>
#include <thread>

#include <shellapi.h>

namespace crisp {
namespace {

// Arka plandaki denetimden pencereye dönen sonuç. `WM_CRISP_UPDATE_RESULT`ın
// `lParam`ı bunun adresidir ve alan taraf sahipliği devralır.
struct UpdateResult {
    bool ok = false;      // yanıt alındı ve okundu
    bool newer = false;   // yayındaki sürüm bizimkinden büyük
    std::wstring version;
    std::wstring url;
    bool manual = false;  // kullanıcı menüden istedi: her sonuç söylenir
};

constexpr wchar_t kApiHost[] = L"api.github.com";
constexpr wchar_t kApiPath[] = L"/repos/shadesofdeath/Crisp/releases/latest";

// Açılmasına izin verilen adreslerin öneki.
//
// ADRES SUNUCUDAN GELİYOR VE SUNUCUYA GÜVENİLMİYOR. `html_url` bir gün GitHub
// dışını gösterirse — hesap ele geçirilir, API değişir, yanıt bir ara
// sunucudan gelir — ShellExecute onu olduğu gibi tarayıcıya verirdi. Yalnızca
// deponun kendi alan adı geçer; gerisi hiç açılmaz.
constexpr wchar_t kAllowedPrefix[] = L"https://github.com/";

[[nodiscard]] bool IsAllowedUrl(const std::wstring& url) noexcept {
    return url.compare(0, wcslen(kAllowedPrefix), kAllowedPrefix) == 0;
}

// Kaynak cümlesindeki `%s` yerlerini sırayla verilen metinlerle değiştirir.
//
// swprintf DEĞİL: cümleye giren sürüm metni sunucudan geliyor ve bir biçim
// dizesine kullanıcı denetimindeki metni yaklaştırmak, bir gün `%n` içeren bir
// etiketle bitiyor. Düz arama-değiştirme bu kapıyı hiç açmıyor.
[[nodiscard]] std::wstring FillTwo(std::wstring text, const std::wstring& first,
                                   const std::wstring& second) {
    size_t at = text.find(L"%s");
    if (at != std::wstring::npos) {
        text.replace(at, 2, first);
        at = text.find(L"%s", at + first.size());
        if (at != std::wstring::npos) {
            text.replace(at, 2, second);
        }
    }
    return text;
}

void OpenUrl(const std::wstring& url) {
    if (!IsAllowedUrl(url)) {
        LogV(L"Güncelleme sayfası açılmadı: adres GitHub dışında");
        return;
    }
    ::ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace

void App::CheckForUpdates(bool manual) {
    // İkinci bir denetim başlatmak, iki kutu açmakla biterdi.
    if (m_updateCheckRunning) {
        return;
    }
    m_updateCheckRunning = true;

    const HWND window = m_window;
    std::thread([window, manual]() {
        // Kullanıcı aracısı zorunlu: GitHub API'si onsuz isteği 403 ile
        // çeviriyor. Sürüm numarası da içinde; sunucu tarafında hangi sürümün
        // sorduğu görülebilsin.
        const std::wstring headers = L"User-Agent: Crisp/" CRISP_VERSION_TEXT
                                     L"\r\nAccept: application/vnd.github+json\r\n";

        auto payload = std::make_unique<UpdateResult>();
        payload->manual = manual;

        std::string body;
        unsigned status = 0;
        if (HttpGetText(kApiHost, kApiPath, headers, body, status) && status == 200) {
            UpdateInfo info;
            if (ParseLatestRelease(body, info)) {
                payload->ok = true;
                payload->newer = IsNewer(info, CRISP_VERSION_TEXT);
                payload->version = std::move(info.version);
                payload->url = std::move(info.url);
            } else {
                LogV(L"Güncelleme denetimi: yanıt ayrıştırılamadı");
            }
        } else {
            LogV(L"Güncelleme denetimi: yanıt alınamadı, durum %u", status);
        }

        if (::PostMessageW(window, WM_CRISP_UPDATE_RESULT, 0,
                           reinterpret_cast<LPARAM>(payload.get())) != FALSE) {
            (void)payload.release();   // sahiplik pencereye geçti
        }
    }).detach();
}

void App::FinishUpdateCheck(LPARAM lParam) {
    const std::unique_ptr<UpdateResult> payload(
        reinterpret_cast<UpdateResult*>(lParam));
    m_updateCheckRunning = false;
    if (!payload) {
        return;
    }

    if (!payload->ok) {
        if (payload->manual) {
            ShowMessage(m_instance, m_window, Loc::Str(IDS_UPDATE_FAILED),
                        MessageIcon::Error);
        }
        return;
    }

    if (!payload->newer) {
        // Tepsi menüsündeki "güncelleme var" satırı da düşer: kullanıcı bu
        // arada yeni sürümü kurduysa eski hatırlatma yanlış olurdu.
        m_updateVersion.clear();
        m_updateUrl.clear();
        if (payload->manual) {
            ShowMessage(m_instance, m_window,
                        FillTwo(Loc::Str(IDS_UPDATE_NONE), CRISP_VERSION_TEXT, L""),
                        MessageIcon::Information);
        }
        return;
    }

    // Menü, bu ikisi doluyken "Güncelleme var: x" satırını ekliyor (App.cpp,
    // SetExtraState). Kutu kapatılsa da satır kalır; kullanıcı sonra da
    // ulaşabilsin.
    m_updateVersion = payload->version;
    m_updateUrl = payload->url;

    // Sessiz denetimde bildirimler kapalıysa yalnızca menü satırı kalır:
    // kullanıcı "beni rahatsız etme" demişti ve bir ileti kutusu tam olarak
    // odur.
    if (!payload->manual && !m_settings.showNotification) {
        return;
    }

    const std::wstring text =
        FillTwo(Loc::Str(IDS_UPDATE_AVAILABLE), payload->version, CRISP_VERSION_TEXT) +
        L"\n\n" + Loc::Str(IDS_UPDATE_OPEN);
    if (ShowMessage(m_instance, m_window, text, MessageIcon::Question,
                    MessageButtons::YesNo) == MessageResult::Yes) {
        OpenUrl(m_updateUrl);
    }
}

void App::OpenUpdatePage() {
    if (m_updateUrl.empty()) {
        return;
    }
    OpenUrl(m_updateUrl);
}

}  // namespace crisp
