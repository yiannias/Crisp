// Upload.cpp — Yüklemenin hiçbir servise özel olmayan yarısı.
//
// multipart zarfı, JSON okuma ve UTF-8 dönüşümü. Hangi servisin nereye ne
// gönderdiği UploadServices.cpp'de, göndermenin kendisi UploadHttp.cpp'de.
//
// AĞA HİÇ ÇIKMAZ ve tam olarak bu yüzden ayrı duruyor: buradaki her şey bir
// sunucuya bağlanmadan sınanabiliyor, ve bir yükleme özelliğinin güvenilirliği
// büyük ölçüde "isteği doğru kuruyor muyuz, yanıtı doğru okuyor muyuz"
// sorularının cevabı.
#include "Upload.h"

#include "UploadInternal.h"

#include <windows.h>

namespace crisp {

// ---------------------------------------------------------------------------
// multipart/form-data
// ---------------------------------------------------------------------------

std::string BuildMultipartBody(
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& fileField, const std::string& fileName,
    const std::string& contentType, const std::vector<unsigned char>& fileBytes,
    const std::string& boundary) {
    std::string body;
    // Kabaca doğru bir ilk ayırma: gövdenin neredeyse tamamı dosya.
    body.reserve(fileBytes.size() + 512 + fields.size() * 128);

    for (const auto& field : fields) {
        body += "--";
        body += boundary;
        body += "\r\nContent-Disposition: form-data; name=\"";
        body += field.first;
        body += "\"\r\n\r\n";
        body += field.second;
        body += "\r\n";
    }

    // DOSYA EN SONDA. Bazı sunucular alanları akış hâlinde okuyor ve dosyayı
    // gördüğünde önceki alanların hepsinin gelmiş olmasını bekliyor — Chevereto
    // tabanlı freeimage.host'ta `key` dosyadan sonra gelirse istek reddediliyor.
    body += "--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"";
    body += fileField;
    body += "\"; filename=\"";
    body += fileName;
    body += "\"\r\nContent-Type: ";
    body += contentType;
    body += "\r\n\r\n";
    body.append(reinterpret_cast<const char*>(fileBytes.data()), fileBytes.size());
    body += "\r\n--";
    body += boundary;
    body += "--\r\n";

    return body;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] bool ReadHex4(const std::string& json, size_t at, unsigned& out) noexcept {
    if (at + 4 > json.size()) {
        return false;
    }
    unsigned value = 0;
    for (size_t i = 0; i < 4; ++i) {
        const char c = json[at + i];
        unsigned digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }
    out = value;
    return true;
}

void AppendUtf8(std::string& out, unsigned code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

// `json[at]` bir dizenin açılış tırnağındayken, dizeyi okur ve `at`'i kapanış
// tırnağının bir sonrasına taşır. Kaçış dizileri çözülür.
[[nodiscard]] std::string ReadJsonString(const std::string& json, size_t& at) {
    std::string out;
    ++at;   // açılış tırnağı
    while (at < json.size()) {
        const char c = json[at];
        if (c == '"') {
            ++at;
            return out;
        }
        if (c == '\\' && at + 1 < json.size()) {
            const char next = json[at + 1];
            switch (next) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    // \uXXXX ÇÖZÜLÜR. Bazı kodlayıcılar (Go'nun encoding/json'ı
                    // gibi) URL'deki & < > karakterlerini böyle kaçırır; eski
                    // hâli ters bölüyü düşürüp "u0026"yı bırakıyor, sorgu
                    // dizesi olan her bağlantıyı bozuyordu.
                    unsigned unit = 0;
                    if (!ReadHex4(json, at + 2, unit)) {
                        out += next;   // bozuk kaçış: olduğu gibi
                        break;
                    }
                    at += 4;
                    // Vekil çift: yüksek yarıyı takip eden \uDC00..DFFF ile
                    // birleştir; eşleşmeyen bir yarı U+FFFD olur.
                    unsigned code = unit;
                    if (unit >= 0xD800 && unit <= 0xDBFF) {
                        unsigned low = 0;
                        if (at + 7 < json.size() && json[at + 2] == '\\' &&
                            json[at + 3] == 'u' && ReadHex4(json, at + 4, low) &&
                            low >= 0xDC00 && low <= 0xDFFF) {
                            code = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                            at += 6;
                        } else {
                            code = 0xFFFD;
                        }
                    } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                        code = 0xFFFD;
                    }
                    AppendUtf8(out, code);
                    break;
                }
                default:  out += next; break;
            }
            at += 2;
            continue;
        }
        out += c;
        ++at;
    }
    return out;   // kapanmamış dize: elimizdekini veririz
}

// `at`'ten başlayan değeri atlar. Dizeleri ve iç içe geçmeyi sayar.
void SkipJsonValue(const std::string& json, size_t& at) {
    while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                json[at] == '\r' || json[at] == '\t')) {
        ++at;
    }
    if (at >= json.size()) {
        return;
    }
    if (json[at] == '"') {
        (void)ReadJsonString(json, at);
        return;
    }
    if (json[at] == '{' || json[at] == '[') {
        int depth = 0;
        while (at < json.size()) {
            const char c = json[at];
            if (c == '"') {
                (void)ReadJsonString(json, at);
                continue;
            }
            if (c == '{' || c == '[') {
                ++depth;
            } else if (c == '}' || c == ']') {
                --depth;
                if (depth == 0) {
                    ++at;
                    return;
                }
            }
            ++at;
        }
        return;
    }
    // Sayı, true/false/null: bir sonraki ayraca kadar.
    while (at < json.size() && json[at] != ',' && json[at] != '}' && json[at] != ']') {
        ++at;
    }
}

// Bir nesnenin İÇİNDE, verilen adlı anahtarı bulur ve `at`'i değerinin başına
// taşır. Yalnızca O SEVİYEDEKİ anahtarlara bakar — iç içe bir nesnedeki aynı
// adlı anahtar bulunmaz, ki `data.url` ile `data.thumb.url` karışmasın.
[[nodiscard]] bool EnterObjectKey(const std::string& json, size_t& at,
                                  const std::string& key) {
    auto skipSpace = [&] {
        while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                    json[at] == '\r' || json[at] == '\t')) {
            ++at;
        }
    };
    // DEĞER TAM BURADA BİR NESNE OLMALI. Eskiden bir sonraki '{' aranıyordu:
    // `data` bir dize çıkarsa arama belgedeki ilk alakasız nesneye giriyor ve
    // `data.link`, başka bir nesnenin `link`ini döndürüyordu.
    //
    // Bir DİZİ ise ilk öğesine girilir: qu.ax `files[0].url` biçiminde yanıt
    // veriyor ve yol sözdiziminde dizin yok.
    skipSpace();
    if (at < json.size() && json[at] == '[') {
        ++at;
        skipSpace();
    }
    if (at >= json.size() || json[at] != '{') {
        return false;
    }
    ++at;   // '{'

    while (at < json.size()) {
        while (at < json.size() && json[at] != '"' && json[at] != '}') {
            ++at;
        }
        if (at >= json.size() || json[at] == '}') {
            return false;
        }
        const std::string name = ReadJsonString(json, at);
        while (at < json.size() && json[at] != ':') {
            ++at;
        }
        if (at >= json.size()) {
            return false;
        }
        ++at;   // ':'
        while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                    json[at] == '\r' || json[at] == '\t')) {
            ++at;
        }
        if (name == key) {
            return true;
        }
        SkipJsonValue(json, at);
    }
    return false;
}

}  // namespace

std::wstring JsonFindString(const std::string& json, const std::string& dottedPath) {
    size_t at = 0;
    size_t start = 0;

    while (start <= dottedPath.size()) {
        const size_t dot = dottedPath.find('.', start);
        const std::string part = dottedPath.substr(
            start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!EnterObjectKey(json, at, part)) {
            return std::wstring();
        }
        if (dot == std::string::npos) {
            if (at >= json.size() || json[at] != '"') {
                return std::wstring();   // yol bir dizede bitmiyor
            }
            return Utf8ToWide(ReadJsonString(json, at));
        }
        start = dot + 1;
    }
    return std::wstring();
}


// ---------------------------------------------------------------------------
// Kodlama
// ---------------------------------------------------------------------------

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return std::string();
    }
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          out.data(), needed);
    return out;
}

std::string MakeBoundary() {
    // Zaman + sayaçtan türetilir. Kriptografik olması gerekmiyor; gereken tek
    // şey, gövdedeki PNG baytlarının içinde geçmemesi — ve "----CrispBoundary"
    // öneki zaten bir PNG'de bulunmayacak bir dizi.
    static unsigned counter = 0;
    LARGE_INTEGER now{};
    ::QueryPerformanceCounter(&now);

    char tail[40] = {};
    ::sprintf_s(tail, "%016llx%08x",
                static_cast<unsigned long long>(now.QuadPart), ++counter);

    std::string boundary = "----CrispBoundary";
    boundary += tail;
    return boundary;
}

}  // namespace crisp
