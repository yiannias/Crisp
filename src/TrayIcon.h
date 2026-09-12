// TrayIcon.h — Bildirim alanı simgesi ve bağlam menüsü.
#pragma once

#include "Settings.h"

#include <windows.h>

#include <string>

namespace crisp {

class TrayIcon {
public:
    TrayIcon() noexcept = default;

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    ~TrayIcon() { Remove(); }

    [[nodiscard]] bool Add(HWND owner, HINSTANCE instance);
    void Remove() noexcept;

    // Görev çubuğu temasına göre açık/koyu simge çeşidini seçer. Tema
    // değişmediyse hiçbir şey yapmaz, bu yüzden bir zamanlayıcıdan sık sık
    // çağrılabilir.
    void RefreshTheme();

    // Explorer yeniden başladığında simge kaybolur; kabuk bunu
    // "TaskbarCreated" mesajıyla duyurur ve simge yeniden eklenmelidir.
    [[nodiscard]] static UINT TaskbarCreatedMessage() noexcept;
    void Restore();

    // Menü açılmadan ÖNCE bildirilir: hangi komutların etkin olduğu duruma
    // bağlı ve tepsi simgesi uygulamanın durumunu bilmez. Menüyü açan taraf
    // söyler, menü de onu yalnızca soluklaştırmak için kullanır.
    void SetMenuState(const Settings& settings, bool hasLastRegion,
                      bool hasClipboardImage) noexcept {
        m_settings = &settings;
        m_hasLastRegion = hasLastRegion;
        m_hasClipboardImage = hasClipboardImage;
    }

    // Kılavuzdaki adım sayısı (0 ise "bitir/at" soluk) ve bilinen yeni sürüm
    // (boşsa "Güncelleme var" satırı hiç eklenmez).
    void SetExtraState(size_t guideSteps, std::wstring updateVersion) {
        m_guideSteps = guideSteps;
        m_updateVersion = std::move(updateVersion);
    }

    // Bağlam menüsünü imlecin konumunda açar ve seçilen komut kimliğini
    // döndürür (iptal edilirse 0).
    [[nodiscard]] int ShowMenu(HWND owner);

private:
    void UpdateIcon();

    HWND m_owner = nullptr;
    HINSTANCE m_instance = nullptr;
    bool m_added = false;
    bool m_darkTaskbar = true;
    bool m_themeKnown = false;
    bool m_hasLastRegion = false;
    bool m_hasClipboardImage = false;
    size_t m_guideSteps = 0;
    std::wstring m_updateVersion;
    const Settings* m_settings = nullptr;
};

}  // namespace crisp
