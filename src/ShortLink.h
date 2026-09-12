// ShortLink.h — is.gd ile bağlantı kısaltma. TASLAK.
#pragma once

#include <string>

namespace crisp {

// is.gd'nin düz metin yanıtını çözer; "Error: ..." ya da boş gövde false.
[[nodiscard]] bool ParseShortLinkResponse(const std::string& body, std::wstring& out);

}  // namespace crisp
