// ScrollCapture.cpp — bkz. ScrollCapture.h.
#include "ScrollCapture.h"

#include "Geometry.h"
#include "Stitch.h"
#include "Util.h"

#include <algorithm>
#include <initializer_list>

namespace crisp {
namespace {

// Beklerken mesaj kuyruğunu boşaltır.
//
// DÜZ `Sleep` YETMİYOR. Bu döngü arayüz iş parçacığında çalışıyor ve saniyeler
// sürüyor; o süre boyunca hiçbir mesaj işlenmezse "Kaydırılıyor…" bildirimi
// zamanlayıcısını hiç almaz, opaklığı sıfırda kalır ve GÖRÜNMEZ. Kullanıcı da
// on saniye boyunca hiçbir şey olmuyormuş gibi bekler — tam da yükleme için
// düzeltilen sorun.
//
// Yeni bir yakalama başlatılmasına karşı koruma çağıranda: `m_busy` bu döngü
// boyunca kurulu kalıyor.
void SleepPumping(unsigned milliseconds) {
    // 64 bit sayaç: 32 bitlik GetTickCount 49,7 günde bir sarar ve sarma
    // anında deadline geçmişe düşer, bekleme sıfıra iner.
    const ULONGLONG deadline = ::GetTickCount64() + milliseconds;
    for (;;) {
        MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        const ULONGLONG now = ::GetTickCount64();
        if (now >= deadline) {
            return;
        }
        const DWORD remaining = static_cast<DWORD>(deadline - now);
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, remaining, QS_ALLINPUT);
    }
}

void SendWheel(int notches, ScrollDirection direction) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    if (direction == ScrollDirection::Horizontal) {
        // YALNIZCA HWHEEL, Shift+tekerlek değil. Shift+tekerlek bir tarayıcı
        // alışkanlığı; gerçek yatay tekerlek olayı ise her pencerede aynı
        // anlama geliyor ve Shift'i basılı tutmak, kullanıcının o sırada
        // bastığı tuşlarla karışırdı. Pozitif = sağa.
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData = static_cast<DWORD>(WHEEL_DELTA * notches);
    } else {
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        // Negatif = aşağı. Kaydırmalı yakalama aşağı iner; yukarı çıkan bir
        // sayfa yakalamak isteyen kullanıcı zaten başa gidip aşağı iner.
        input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA * notches);
    }
    ::SendInput(1, &input, sizeof(input));
}

// Bir adım kaydırır, sayfanın oturmasını bekler, yakalar.
//
// İMLEÇ ÇİZİLMEZ. Her karede aynı yerde duran bir imleç, birleştirilmiş
// görüntüde onlarca kez tekrarlanan bir ok bırakırdı.
[[nodiscard]] bool StepAndCapture(const RECT& region,
                                  const ScrollCaptureOptions& options,
                                  int notches, ScrollDirection direction,
                                  Image& frame) {
    SendWheel(notches, direction);
    SleepPumping(options.settleMs);
    return CaptureRect(region, frame, false);
}

// Yeni içerik var mı? Birleştirmenin kullanacağı aramanın aynısı, ki
// toplamada kabul edilen kare birleştirmede reddedilmesin.
[[nodiscard]] bool Moved(const Image& previous, const Image& next,
                         ScrollDirection direction) noexcept {
    const int shift =
        direction == ScrollDirection::Horizontal
            ? FindHorizontalShift(previous, next, kScrollOverlapRows)
            : FindVerticalShift(previous, next, kScrollOverlapRows);
    return shift > 0;
}

// İmleci bölgenin ortasına taşır; kapsam bitince eski yerine koyar.
//
// İMLEÇ BÖLGENİN ORTASINA. Tekerlek olayı imlecin altındaki pencereye
// gidiyor; kenara koymak, komşu bir panelin kaydırılmasına yol açardı.
class CursorParking {
public:
    explicit CursorParking(const RECT& region) noexcept {
        m_haveOriginal = ::GetCursorPos(&m_original) != FALSE;
        const POINT centre{region.left + geom::Width(region) / 2,
                           region.top + geom::Height(region) / 2};
        ::SetCursorPos(static_cast<int>(centre.x), static_cast<int>(centre.y));
    }
    ~CursorParking() {
        if (m_haveOriginal) {
            ::SetCursorPos(static_cast<int>(m_original.x),
                           static_cast<int>(m_original.y));
        }
    }
    CursorParking(const CursorParking&) = delete;
    CursorParking& operator=(const CursorParking&) = delete;

private:
    POINT m_original{};
    bool m_haveOriginal = false;
};

}  // namespace

bool CollectScrollFrames(const RECT& region, const ScrollCaptureOptions& options,
                         std::vector<Image>& frames, ScrollDirection* used) {
    frames.clear();
    if (used != nullptr) {
        *used = ScrollDirection::Vertical;
    }
    if (geom::IsEmpty(region)) {
        return false;
    }

    const int maxFrames = (std::max)(1, options.maxFrames);
    const int notches = (std::max)(1, options.notchesPerStep);

    const CursorParking parking{region};

    // İmleç taşındıktan sonra vurgu değişimlerinin oturması için kısa bir
    // bekleme: ilk kare, imlecin altındaki bağlantı henüz renk değiştirmemişken
    // yakalanırsa ikinciyle eşleşmesi zorlaşır.
    SleepPumping(120);

    Image first;
    if (!CaptureRect(region, first, false)) {
        return false;
    }
    frames.push_back(std::move(first));

    // YÖN KARARI: Auto'da bir dikey adım denenir; sayfa kıpırdamadıysa geri
    // alınacak bir şey yok, bir de yatay adım denenir. Kıpırdayan adımın
    // karesi ikinci kare olarak KALIR — atılsaydı aynı adım bir kez daha
    // gönderilir ve kullanıcı bir adımlık içerik kaybederdi.
    ScrollDirection direction = options.direction;
    bool ok = true;
    if (direction == ScrollDirection::Auto &&
        static_cast<int>(frames.size()) < maxFrames) {
        for (const ScrollDirection candidate :
             {ScrollDirection::Vertical, ScrollDirection::Horizontal}) {
            Image probe;
            if (!StepAndCapture(region, options, notches, candidate, probe)) {
                ok = false;
                break;
            }
            if (Moved(frames.back(), probe, candidate)) {
                direction = candidate;
                frames.push_back(std::move(probe));
                break;
            }
        }
    }
    if (direction == ScrollDirection::Auto) {
        // Karar verilemedi: sayfa iki yönde de kıpırdamadı ya da kare sınırı
        // bir. Tek kare teslim edilir; çağıran bunu "kaydırılamadı" diye
        // bildirir.
        LogV(L"Kaydırma: sayfa hiçbir yönde kıpırdamadı");
        return true;
    }
    if (used != nullptr) {
        *used = direction;
    }
    if (options.onDirection != nullptr) {
        options.onDirection(direction, options.onDirectionContext);
    }

    while (ok && static_cast<int>(frames.size()) < maxFrames) {
        Image frame;
        if (!StepAndCapture(region, options, notches, direction, frame)) {
            ok = false;
            break;
        }
        // YENİ İÇERİK YOKSA DUR. Sayfanın sonuna gelmiş olabiliriz ya da
        // pencere kaydırmayı bırakmış olabilir; ikisinde de devam etmek aynı
        // kareyi tekrar tekrar yakalamak demek.
        if (!Moved(frames.back(), frame, direction)) {
            LogV(L"Kaydırma durdu: %zu. karede eşleşme yok", frames.size());
            break;
        }
        frames.push_back(std::move(frame));
    }

    if (!ok) {
        LogV(L"Kaydırmalı yakalama: %zu karede kesildi", frames.size());
    }
    return true;
}

}  // namespace crisp
