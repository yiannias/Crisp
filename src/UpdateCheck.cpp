// UpdateCheck.cpp — Sürüm karşılaştırma ve GitHub Releases yanıtının okunması.
#include "UpdateCheck.h"

#include "Upload.h"   // JsonFindString

namespace crisp {
namespace {

// Metnin `at` konumundan bir sayısal parçayı okur ve konumu bir sonraki
// parçanın başına taşır. Parça yoksa 0 döner ve konum ilerlemez.
//
// SAYIDAN SONRAKİ HARFLER ATILIR, PARÇA DEĞİL: "3-beta" parçası 3 sayılır ve
// sonraki noktaya kadar olan her şey yok sayılır. Ondalık noktaya kadar
// sayı olmayan bir parça ("rc1") sıfırdır.
[[nodiscard]] unsigned long long ReadComponent(const std::wstring& text,
                                               size_t& at) noexcept {
    unsigned long long value = 0;
    while (at < text.size() && text[at] >= L'0' && text[at] <= L'9') {
        // TAŞMA KORUMASI: on sekiz haneden uzun bir parça gerçek bir sürüm
        // değil; sayıyı büyütmeyi bırakmak, sarmalamaktan iyidir.
        if (value < 100000000000000000ULL) {
            value = value * 10 + static_cast<unsigned long long>(text[at] - L'0');
        }
        ++at;
    }
    // Parçanın kalanı (harf, tire, boşluk) bir sonraki noktaya kadar atlanır.
    while (at < text.size() && text[at] != L'.') {
        ++at;
    }
    if (at < text.size()) {
        ++at;   // noktayı geç
    }
    return value;
}

// Baştaki boşlukları ve 'v' önekini atlar.
[[nodiscard]] size_t SkipPrefix(const std::wstring& text) noexcept {
    size_t at = 0;
    while (at < text.size() && (text[at] == L' ' || text[at] == L'\t')) {
        ++at;
    }
    if (at < text.size() && (text[at] == L'v' || text[at] == L'V')) {
        ++at;
    }
    return at;
}

}  // namespace

int CompareVersions(const std::wstring& a, const std::wstring& b) noexcept {
    size_t atA = SkipPrefix(a);
    size_t atB = SkipPrefix(b);

    // İki metin de tükenene kadar parça parça: eksik taraf sıfır okur, böylece
    // "1.2" ile "1.2.0" eşit, "1.2" ile "1.2.1" küçük çıkar.
    while (atA < a.size() || atB < b.size()) {
        const unsigned long long partA = ReadComponent(a, atA);
        const unsigned long long partB = ReadComponent(b, atB);
        if (partA < partB) {
            return -1;
        }
        if (partA > partB) {
            return 1;
        }
    }
    return 0;
}

bool ParseLatestRelease(const std::string& json, UpdateInfo& out) {
    out = UpdateInfo{};
    if (json.empty()) {
        return false;
    }

    std::wstring tag = JsonFindString(json, "tag_name");
    if (tag.empty()) {
        // Etiket olmadan sürüm yok. GitHub bir hata döndürdüğünde de gövde JSON
        // ama içinde "message" var, "tag_name" yok — o da buraya düşer.
        return false;
    }
    if (tag[0] == L'v' || tag[0] == L'V') {
        tag.erase(0, 1);
    }
    if (tag.empty()) {
        return false;
    }

    out.version = tag;
    out.url = JsonFindString(json, "html_url");
    return true;
}

bool IsNewer(const UpdateInfo& info, const std::wstring& current) noexcept {
    if (info.version.empty()) {
        return false;
    }
    return CompareVersions(info.version, current) > 0;
}

}  // namespace crisp
