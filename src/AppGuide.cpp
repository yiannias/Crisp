// AppGuide.cpp — Adım adım kılavuz: ardışık yakalamaları numaralı tek bir
// görüntüde toplar.
//
// AKIŞ: "Adım ekle" her seferinde normal bölge/pencere kaplamasını açar ve
// sonucu teslim etmek yerine biriktirir. "Bitir" birikeni GuideCompose ile
// tek görüntüye çevirip OLAĞAN teslimat yoluna sokar — pano, dosya, düzenleyici
// ve iğne kararları böylece kılavuz için ayrıca yazılmaz.
#include "App.h"

#include "GuideCompose.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Sound.h"
#include "Toast.h"
#include "Util.h"
#include "resource.h"

#include <mmsystem.h>

#include <cstdint>
#include <string>
#include <vector>

namespace crisp {
namespace {

// Üst sınır keyfî ama gerekli: her adım tam boyutlu bir yakalama olarak
// bellekte duruyor ve unutulmuş bir kılavuz sınırsız büyürdü. Otuz adımdan
// uzun bir kılavuz zaten tek görüntüye sığmaz — kMaxImageSide'a takılır.
constexpr size_t kMaxGuideSteps = 30;

// AppCapture.cpp'deki PlayShutter adsız ad alanında; aynı iki satır burada
// da duruyor. Ortak bir başlığa taşımak, iki çağıran için fazla tören.
void PlayGuideShutter() {
    // TAMPON ÇALMA BİTENE KADAR YAŞAMALI (SND_ASYNC belleği kopyalamaz).
    static const std::vector<uint8_t> shutter = BuildShutterWav();
    ::PlaySoundW(reinterpret_cast<LPCWSTR>(shutter.data()), nullptr,
                 SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

// "Step %u added to the guide" — kaynak dizgideki tek yer tutucu adım sayısı.
[[nodiscard]] std::wstring StepAddedText(size_t count) {
    std::wstring text = Loc::Str(IDS_GUIDE_STEP_ADDED);
    const size_t at = text.find(L"%u");
    if (at == std::wstring::npos) {
        return text;
    }
    return text.replace(at, 2, std::to_wstring(count));
}

}  // namespace

void App::GuideAddStep(bool preferWindowPick) {
    if (m_busy) {
        return;
    }
    const BusyScope busy{m_busy};

    if (m_guideSteps.size() >= kMaxGuideSteps) {
        LogV(L"Kılavuz adım sınırına ulaştı (%u)",
             static_cast<unsigned>(kMaxGuideSteps));
        return;
    }

    Image image;
    POINT origin{};
    ::Sleep(kMenuSettleMs);
    if (!RunRegionCapture(m_instance, m_settings, preferWindowPick, image,
                          origin)) {
        return;   // kullanıcı vazgeçti
    }

    // SES BURADA DA ÇALAR: kullanıcı her adımda "alındı" onayı bekler; sessiz
    // bir adım, kaplamanın kapanıp kapanmadığını tahmin etmeye bırakır.
    if (m_settings.playShutterSound) {
        PlayGuideShutter();
    }

    // Bildirim küçük resim için görüntüye ihtiyaç duyuyor; taşımadan önce.
    if (m_settings.showNotification) {
        ShowCaptureToast(m_instance, image, StepAddedText(m_guideSteps.size() + 1),
                         L"", L"");
    }
    m_guideSteps.push_back(std::move(image));
}

void App::GuideFinish() {
    if (m_busy) {
        return;
    }
    const BusyScope busy{m_busy};

    if (m_guideSteps.empty()) {
        ShowMessage(m_instance, m_window, Loc::Str(IDS_GUIDE_EMPTY),
                    MessageIcon::Information);
        return;
    }

    Image composed;
    size_t placed = 0;
    const bool ok = ComposeGuide(m_guideSteps, GuideOptions{}, composed, &placed);
    if (ok && placed < m_guideSteps.size()) {
        LogV(L"Kılavuz kısmen birleştirildi: %u/%u adım",
             static_cast<unsigned>(placed),
             static_cast<unsigned>(m_guideSteps.size()));
    }
    // ADIMLAR HER DURUMDA BOŞALIR: birleştirme başarısızsa aynı adımlarla
    // tekrar denemek de başarısız olur ve kullanıcı sıkışıp kalır.
    m_guideSteps.clear();
    if (!ok) {
        LogV(L"Kılavuz birleştirilemedi");
        return;
    }

    DeliverCapture(composed, POINT{0, 0}, nullptr);
}

void App::GuideDiscard() { m_guideSteps.clear(); }

}  // namespace crisp
