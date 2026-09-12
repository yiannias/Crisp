// HttpGet.cpp — Tek bir HTTPS GET isteği, WinHTTP ile.
//
// WinHTTP seçildi, WinINet değil; gerekçe UploadHttp.cpp'de: WinINet arayüzü
// olan bir süreç varsayar ve arka plan iş parçacığından kullanımı
// desteklenmiyor. Bu istek tam olarak arka plan iş parçacığından yapılıyor.
#include "HttpGet.h"

#include <windows.h>
#include <winhttp.h>

namespace crisp {
namespace {

// Yanıt için kabul edilen en büyük gövde (bkz. HttpGet.h).
constexpr size_t kMaxResponse = 1u << 20;   // 1 MiB

// Bağlantı, gönderme ve alma zaman aşımları. Sessiz açılış denetimi arka
// planda ve kullanıcı beklemiyor; elle başlatılan denetimde ise on saniye,
// "bir şey oluyor mu" sorusunun cevabını almak için yeterince kısa.
constexpr int kTimeoutMs = 10000;

// WinHTTP tutamacı — kapsamdan çıkınca kapanır.
//
// UploadHttp.cpp'DEKİNİN KÜÇÜK BİR KOPYASI, paylaşılan başlığa çıkarılmadı:
// oradaki sınıf dosyanın isimsiz ad alanında ve yükleme kodunun iç detayı.
// On satırlık bir sarmalayıcıyı iki dosyanın ortak diline çıkarmak, HttpGet'i
// UploadInternal.h'a bağlardı — GET'in yüklemeyle hiçbir ilgisi yok.
class Handle {
public:
    explicit Handle(HINTERNET handle) noexcept : m_handle(handle) {}
    ~Handle() {
        if (m_handle != nullptr) {
            ::WinHttpCloseHandle(m_handle);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    [[nodiscard]] HINTERNET get() const noexcept { return m_handle; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_handle != nullptr; }

private:
    HINTERNET m_handle = nullptr;
};

// Gövdeyi sınıra kadar okur. Sınıra ulaşılınca kalan atılır: yarım bir
// JSON zaten ayrıştırılamaz ve okumaya devam etmek yalnızca bant genişliği.
void ReadBody(HINTERNET http, std::string& body) {
    for (;;) {
        DWORD available = 0;
        if (::WinHttpQueryDataAvailable(http, &available) == FALSE || available == 0) {
            break;
        }
        if (body.size() + available > kMaxResponse) {
            available = static_cast<DWORD>(kMaxResponse - body.size());
            if (available == 0) {
                break;
            }
        }
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (::WinHttpReadData(http, body.data() + offset, available, &read) == FALSE) {
            body.resize(offset);
            break;
        }
        body.resize(offset + read);
        if (read == 0) {
            break;
        }
    }
}

}  // namespace

bool HttpGetText(const std::wstring& host, const std::wstring& path,
                 const std::wstring& extraHeaders, std::string& body,
                 unsigned& status) {
    body.clear();
    status = 0;
    if (host.empty()) {
        return false;
    }

    // Oturumun kendi kullanıcı aracısı YOK: gerçek olan `extraHeaders` içinde
    // çağırandan geliyor ve sürüm numarasını taşıyor. WinHTTP ad verilmeyince
    // başlığı hiç yazmıyor, çağıranınki tek kalıyor.
    Handle session(::WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return false;
    }
    ::WinHttpSetTimeouts(session.get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    Handle connection(::WinHttpConnect(session.get(), host.c_str(),
                                       INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
        return false;
    }

    Handle http(::WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     WINHTTP_FLAG_SECURE));
    if (!http) {
        return false;
    }

    // YÖNLENDİRME KAPALI. GitHub API'si `releases/latest` için yönlendirmez ve
    // otomatik takip, bir gün başka bir alan adına düşen bir isteğin oraya da
    // aynı başlıklarla gitmesi demek olurdu.
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    ::WinHttpSetOption(http.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
                       sizeof(redirectPolicy));

    const wchar_t* headers = extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                  : extraHeaders.c_str();
    const DWORD headerLength = extraHeaders.empty() ? 0 : static_cast<DWORD>(-1);
    if (::WinHttpSendRequest(http.get(), headers, headerLength, WINHTTP_NO_REQUEST_DATA,
                             0, 0, 0) == FALSE) {
        return false;
    }
    if (::WinHttpReceiveResponse(http.get(), nullptr) == FALSE) {
        return false;
    }

    DWORD code = 0;
    DWORD codeSize = sizeof(code);
    if (::WinHttpQueryHeaders(http.get(),
                              WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSize,
                              WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return false;
    }
    status = code;

    ReadBody(http.get(), body);
    return true;
}

}  // namespace crisp
