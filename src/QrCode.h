// QrCode.h — QR kodu üretimi (bayt kipi). TASLAK.
#pragma once

#include "Capture.h"

#include <string>
#include <vector>

namespace crisp {

struct QrCode {
    int size = 0;                    // modül sayısı (kenar)
    std::vector<uint8_t> modules;    // size*size, 1 = koyu
};

[[nodiscard]] bool EncodeQr(const std::string& text, QrCode& out);
[[nodiscard]] bool RenderQr(const QrCode& qr, int moduleSize, int quietZone, Image& out);

}  // namespace crisp
