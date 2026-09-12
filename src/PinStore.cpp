// PinStore.cpp — bkz. PinStore.h.
//
// UploadLog.cpp ile aynı biçim ve aynı gerekçe: UTF-8, BOM'suz, sekmeyle
// ayrılmış. Elle açılıp okunabilen, elle silinebilen bir dosya.
#include "PinStore.h"

#include "Util.h"

#include <shlobj.h>

#include <cstdio>
#include <cstdlib>

namespace crisp {
namespace {

constexpr wchar_t kSeparator = L'\t';

[[nodiscard]] std::wstring LocalAppData() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        return std::wstring();
    }
    std::wstring path(raw);
    ::CoTaskMemFree(raw);
    return path;
}

// Bir alanı ayırıcıya kadar okur ve `at`i bir sonrakinin başına taşır.
[[nodiscard]] bool NextField(const std::wstring& line, size_t& at,
                             std::wstring& out) {
    if (at > line.size()) {
        return false;
    }
    const size_t end = line.find(kSeparator, at);
    out = line.substr(at, end == std::wstring::npos ? std::wstring::npos : end - at);
    at = end == std::wstring::npos ? line.size() + 1 : end + 1;
    return true;
}

}  // namespace

std::wstring PinFolder() {
    const std::wstring base = LocalAppData();
    if (base.empty()) {
        return std::wstring();
    }
    return base + L"\\Crisp\\Pins";
}

std::wstring PinIndexPath() {
    const std::wstring folder = PinFolder();
    if (folder.empty()) {
        return std::wstring();
    }
    return folder + L"\\pins.txt";
}

std::wstring FormatPinLine(const PinRecord& record) {
    wchar_t numbers[96] = {};
    ::swprintf_s(numbers, L"%ld%c%ld%c%d%c%u", record.x, kSeparator, record.y,
                 kSeparator, record.zoom, kSeparator, record.opacity);

    // DOSYA ADI SAYILARDAN SONRA. İçinde sekme bulunması beklenmez ama
    // bulunursa yalnızca kendi alanını (ve ardındaki bayrakları) bozar;
    // sayılar önde olduğu için hepsi okunmuş olur.
    std::wstring line = numbers;
    line += kSeparator;
    line += record.imageFile;

    // BAYRAKLAR EN SONDA, çünkü sonradan eklendi: 0.8'in çözümleyicisi beşinci
    // alandan sonrasını okumuyor, dolayısıyla bu dosyayı eski sürüm de
    // açabilir. Kapalı bayrak `-` olarak yazılır ki alan hiç boş kalmasın ve
    // dosyayı elle okuyan biri dört yuvanın hangisinin dolu olduğunu görsün.
    line += kSeparator;
    line += record.topMost ? L'T' : L'-';
    line += record.frame ? L'F' : L'-';
    line += record.clickThrough ? L'C' : L'-';
    line += record.hidden ? L'H' : L'-';
    return line;
}

namespace {

// Bayrak alanını çözer. Sıra önemsiz; yalnızca tanınan harfler ve `-` kabul
// edilir. Başka bir karakter varsa alan bir bütün olarak reddedilir ve `out`
// dokunulmadan (varsayılanlarla) bırakılır; çağıran satırı yine kabul eder.
void ParsePinFlags(const std::wstring& flags, PinRecord& out) {
    for (const wchar_t c : flags) {
        if (c != L'T' && c != L'F' && c != L'C' && c != L'H' && c != L'-') {
            return;
        }
    }
    out.topMost = flags.find(L'T') != std::wstring::npos;
    out.frame = flags.find(L'F') != std::wstring::npos;
    out.clickThrough = flags.find(L'C') != std::wstring::npos;
    out.hidden = flags.find(L'H') != std::wstring::npos;
}

}  // namespace

bool ParsePinLine(const std::wstring& line, PinRecord& out) {
    size_t at = 0;
    std::wstring x;
    std::wstring y;
    std::wstring zoom;
    std::wstring opacity;
    std::wstring file;

    if (!NextField(line, at, x) || !NextField(line, at, y) ||
        !NextField(line, at, zoom) || !NextField(line, at, opacity) ||
        !NextField(line, at, file)) {
        return false;
    }
    if (file.empty() || zoom.empty() || opacity.empty()) {
        return false;
    }

    // DOSYA ADI YALNIZCA AD OLMALI. İndeks kullanıcının elle açabileceği bir
    // metin dosyası ve içine `..\..\Windows\...` yazılmış bir satır, uygulamayı
    // klasörün dışındaki bir dosyayı okumaya ikna edebilirdi.
    if (file.find(L'\\') != std::wstring::npos ||
        file.find(L'/') != std::wstring::npos ||
        file.find(L':') != std::wstring::npos) {
        return false;
    }

    // ALTINCI ALAN İSTEĞE BAĞLI: yoksa eski sürümün satırıdır ve bayraklar
    // PinRecord'un varsayılanlarında kalır. Boş bir alan (satır sonunda
    // yalnız bir sekme) da aynı kapıya çıkar: hiçbir harf yok demek, üstte
    // durmayan bir iğne demek olurdu ve bu bilinçli bir seçim değil.
    out = PinRecord();
    std::wstring flags;
    if (NextField(line, at, flags) && !flags.empty()) {
        ParsePinFlags(flags, out);
    }

    out.imageFile = file;
    out.x = ::wcstol(x.c_str(), nullptr, 10);
    out.y = ::wcstol(y.c_str(), nullptr, 10);
    out.zoom = static_cast<int>(::wcstol(zoom.c_str(), nullptr, 10));
    out.opacity = static_cast<unsigned>(::wcstoul(opacity.c_str(), nullptr, 10));

    // Aralık dışı değerler DÜZELTİLİR, satır atılmaz: yakınlaştırması bozuk bir
    // iğneyi hiç göstermemektense makul bir değerle göstermek yeğdir.
    if (out.zoom < 10) {
        out.zoom = 10;
    } else if (out.zoom > 800) {
        out.zoom = 800;
    }
    if (out.opacity < 20) {
        out.opacity = 20;
    } else if (out.opacity > 255) {
        out.opacity = 255;
    }
    return true;
}

std::vector<PinRecord> ReadPinIndex() {
    std::vector<PinRecord> records;
    const std::wstring path = PinIndexPath();
    if (path.empty()) {
        return records;
    }

    const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                      nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return records;   // dosya yok: iğne yok
    }

    LARGE_INTEGER size{};
    ::GetFileSizeEx(file, &size);
    std::string utf8(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    ::ReadFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &read, nullptr);
    ::CloseHandle(file);
    utf8.resize(read);

    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                             static_cast<int>(utf8.size()),
                                             nullptr, 0);
    std::wstring text(static_cast<size_t>(needed > 0 ? needed : 0), L'\0');
    if (needed > 0) {
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                              static_cast<int>(utf8.size()), text.data(), needed);
    }

    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) {
            end = text.size();
        }
        std::wstring line = text.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) {
            line.pop_back();
        }
        PinRecord record;
        if (!line.empty() && ParsePinLine(line, record)) {
            records.push_back(std::move(record));
        }
    }
    return records;
}

bool WritePinIndex(const std::vector<PinRecord>& records) {
    const std::wstring path = PinIndexPath();
    if (path.empty()) {
        return false;
    }
    const std::wstring folder = PinFolder();
    ::SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);

    if (records.empty()) {
        ::DeleteFileW(path.c_str());
        return true;
    }

    std::wstring text;
    for (const PinRecord& record : records) {
        text += FormatPinLine(record);
        text += L"\r\n";
    }

    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(needed > 0 ? needed : 0), '\0');
    if (needed > 0) {
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                              static_cast<int>(text.size()), utf8.data(), needed,
                              nullptr, nullptr);
    }

    const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                      nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        LogV(L"İğne indeksi yazılamadı: %s", path.c_str());
        return false;
    }
    DWORD written = 0;
    const bool ok = ::WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()),
                                &written, nullptr) != FALSE;
    ::CloseHandle(file);
    return ok;
}

bool ClearPinStore() noexcept {
    const std::wstring folder = PinFolder();
    if (folder.empty()) {
        return false;
    }

    // Yalnızca BİZİM yazdıklarımız silinir: .png dosyaları ve indeks. Klasörün
    // tamamını silen bir çağrı, kullanıcının oraya koyduğu bir şeyi de
    // götürürdü.
    const std::wstring pattern = folder + L"\\*.png";
    WIN32_FIND_DATAW found{};
    const HANDLE search = ::FindFirstFileW(pattern.c_str(), &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            ::DeleteFileW((folder + L"\\" + found.cFileName).c_str());
        } while (::FindNextFileW(search, &found) != FALSE);
        ::FindClose(search);
    }
    ::DeleteFileW(PinIndexPath().c_str());
    return true;
}

}  // namespace crisp
