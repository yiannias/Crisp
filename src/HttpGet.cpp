// HttpGet.cpp — WinHTTP ile tek bir GET.
//
// UploadHttp.cpp ile aynı kalıp: RAII tutamaç, açık zaman aşımları, gövde
// için üst sınır. Yükleme 60 saniye bekleyebiliyor çünkü büyük bir dosya
// gönderiyor; burada gönderilen bir satır ve alınan birkaç yüz bayt — on
// saniye bile cömert.
#include "HttpGet.h"

#include <windows.h>
#include <winhttp.h>

namespace crisp {
namespace {

constexpr wchar_t kUserAgent[] = L"Crisp";
constexpr DWORD kMaxBody = 1u << 20;   // 1 MiB
constexpr int kTimeoutMs = 10000;

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

}  // namespace

bool HttpGetText(const std::wstring& host, const std::wstring& path,
                 const std::wstring& extraHeaders, std::string& body,
                 unsigned& status) {
    body.clear();
    status = 0;

    Handle session(::WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
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

    Handle request(::WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE));
    if (!request) {
        return false;
    }

    const wchar_t* headers = extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                  : extraHeaders.c_str();
    const DWORD headersLength = extraHeaders.empty() ? 0 : static_cast<DWORD>(-1);
    if (::WinHttpSendRequest(request.get(), headers, headersLength,
                             WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == FALSE ||
        ::WinHttpReceiveResponse(request.get(), nullptr) == FALSE) {
        return false;
    }

    DWORD code = 0;
    DWORD codeSize = sizeof(code);
    if (::WinHttpQueryHeaders(request.get(),
                              WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSize,
                              WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return false;
    }
    status = code;

    for (;;) {
        DWORD available = 0;
        if (::WinHttpQueryDataAvailable(request.get(), &available) == FALSE ||
            available == 0) {
            break;
        }
        if (body.size() + available > kMaxBody) {
            available = static_cast<DWORD>(kMaxBody - body.size());
            if (available == 0) {
                break;
            }
        }
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (::WinHttpReadData(request.get(), body.data() + offset, available, &read) ==
            FALSE) {
            body.resize(offset);
            break;
        }
        body.resize(offset + read);
        if (read == 0) {
            break;
        }
    }
    return true;
}

}  // namespace crisp
