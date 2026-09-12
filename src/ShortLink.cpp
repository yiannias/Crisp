// ShortLink.cpp — is.gd isteği ve yanıtı.
#include "ShortLink.h"

#include "HttpGet.h"
#include "UploadInternal.h"
#include "Util.h"

#include <cstring>

namespace crisp {
namespace {

constexpr char kIsGdPrefix[] = "https://is.gd/";
constexpr char kTinyPrefix[] = "https://tinyurl.com/";

// Sorgu değeri için yüzde kodlaması. RFC 3986'nın ayrılmamış karakterleri
// (harf, rakam, "-._~") olduğu gibi kalır; gerisi — "/" ve ":" dahil —
// kodlanır. Bir bağlantının içindeki "&" ya da "#" kodlanmazsa is.gd
// adresi o karakterde keser ve yarım bir bağlantıyı kısaltır.
[[nodiscard]] bool Unreserved(unsigned char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

}  // namespace

[[nodiscard]] std::string PercentEncode(const std::wstring& url) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string path;
    for (const unsigned char ch : WideToUtf8(url)) {
        if (Unreserved(ch)) {
            path.push_back(static_cast<char>(ch));
        } else {
            path.push_back('%');
            path.push_back(kHex[ch >> 4]);
            path.push_back(kHex[ch & 0xF]);
        }
    }
    return path;
}

[[nodiscard]] bool StartsWith(const std::string& text, const char* prefix) noexcept {
    const size_t n = ::strlen(prefix);
    return text.size() > n && text.compare(0, n, prefix) == 0;
}

std::string BuildIsGdPath(const std::wstring& url) {
    return "/create.php?format=simple&url=" + PercentEncode(url);
}

std::string BuildTinyUrlPath(const std::wstring& url) {
    return "/api-create.php?url=" + PercentEncode(url);
}

bool ParseShortLinkResponse(const std::string& body, std::wstring& out) {
    size_t begin = 0;
    size_t end = body.size();
    while (begin < end && (body[begin] == ' ' || body[begin] == '\r' ||
                           body[begin] == '\n' || body[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (body[end - 1] == ' ' || body[end - 1] == '\r' ||
                           body[end - 1] == '\n' || body[end - 1] == '\t')) {
        --end;
    }
    const std::string trimmed = body.substr(begin, end - begin);
    if (!StartsWith(trimmed, kIsGdPrefix) && !StartsWith(trimmed, kTinyPrefix)) {
        return false;
    }
    // Kısa bağlantı tek bir belirteçtir; içinde boşluk varsa bu bir hata
    // cümlesi ya da beklenmeyen bir sayfadır.
    for (const char ch : trimmed) {
        if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
            return false;
        }
    }
    out = Utf8ToWide(trimmed);
    return true;
}

bool ShortenLink(const std::wstring& url, std::wstring& shortUrl) {
    struct Service {
        const wchar_t* host;
        std::string path;
    };
    const Service services[] = {
        {L"is.gd", BuildIsGdPath(url)},
        {L"tinyurl.com", BuildTinyUrlPath(url)},
    };
    for (const Service& service : services) {
        std::string body;
        unsigned status = 0;
        if (!HttpGetText(service.host, Utf8ToWide(service.path),
                         L"User-Agent: Crisp\r\n", body, status)) {
            LogV(L"Kısaltma: %s'ye ulaşılamadı", service.host);
            continue;
        }
        if (status < 200 || status >= 300 || !ParseShortLinkResponse(body, shortUrl)) {
            LogV(L"Kısaltma: %s %u döndü: %.120hs", service.host, status, body.c_str());
            continue;
        }
        return true;
    }
    return false;
}

}  // namespace crisp
