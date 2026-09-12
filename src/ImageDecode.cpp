// ImageDecode.cpp — PNG çözme.
//
// AYRI DOSYA: kodlamayla birlikte ImageCodec.cpp ev kuralının 400 satır
// sınırını aşıyordu (docs §9).
#include "ImageCodec.h"

#include "ImageCodecInternal.h"
#include "Util.h"

#include <shlwapi.h>
#include <wincodec.h>

#include <vector>

namespace crisp {
namespace {


[[nodiscard]] bool DecodeFromStream(IStream* stream, Image& out) {
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory)) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromStream(stream, nullptr,
                                                  WICDecodeMetadataCacheOnDemand,
                                                  decoder.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    return CopyToImage(factory.Get(), frame.Get(), out);
}

}  // namespace

bool DecodePng(const uint8_t* data, size_t size, Image& out) {
    out.Reset();
    if (data == nullptr || size == 0 || size > MAXUINT32) {
        return false;
    }
    if (!LooksLikeCompletePng(data, size)) {
        LogV(L"PNG eksik ya da bozuk (%zu bayt); çözme denenmedi", size);
        return false;
    }

    ComPtr<IStream> stream;
    *stream.GetAddressOf() = ::SHCreateMemStream(data, static_cast<UINT>(size));
    if (!stream) {
        return false;
    }

    return DecodeFromStream(stream.Get(), out);
}

bool LoadPng(const std::wstring& path, Image& out) {
    out.Reset();
    if (path.empty()) {
        return false;
    }

    // DOSYA ÖNCE BELLEĞE OKUNUR: WIC'e doğrudan bir dosya akışı vermek, eksik
    // yazılmış bir geçmiş PNG'sini yarım görüntü olarak kabul ettirirdi.
    // DecodePng imzayı ve IEND parçasını doğrular; tek doğrulama noktası odur.
    const unique_handle file{::CreateFileW(path.c_str(), GENERIC_READ,
                                           FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                           FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (!file.valid()) {
        return false;
    }
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 ||
        size.QuadPart > MAXUINT32) {
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!::ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()),
                    &read, nullptr) ||
        read != bytes.size()) {
        return false;
    }
    return DecodePng(bytes.data(), bytes.size(), out);
}

bool LoadImageFile(const std::wstring& path, Image& out) {
    out.Reset();
    if (path.empty()) {
        return false;
    }

    // WIC biçimi imzadan tanır ve uzantıya bakmaz. LoadPng'den ayrı: burada
    // bilinmeyen bir dosya açılıyor ve PNG bütünlük denetimi uygulanamaz.
    ComPtr<IStream> stream;
    const HRESULT hr = ::SHCreateStreamOnFileEx(
        path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL,
        FALSE, nullptr, stream.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    return DecodeFromStream(stream.Get(), out);
}

}  // namespace crisp
