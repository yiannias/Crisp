// ShortLink.h — is.gd ile bağlantı kısaltma.
//
// NEDEN is.gd: anahtar istemiyor, hesap istemiyor, tek bir GET ile düz metin
// yanıt veriyor. Bir yükleme bağlantısını telefona QR koduyla geçirirken
// kısa bir adres daha küçük — dolayısıyla daha kolay okunan — bir kod demek.
//
// Ağa çıkan tek fonksiyon `ShortenLink`; isteğin kurulması ve yanıtın
// çözülmesi ayrı ve ağsız sınanabilir.
#pragma once

#include <string>

namespace crisp {

// is.gd API yolu: "/create.php?format=simple&url=<yüzde-kodlanmış UTF-8>".
[[nodiscard]] std::string BuildIsGdPath(const std::wstring& url);

// is.gd'nin düz metin yanıtını çözer. Başarıda gövde kısa bağlantının
// kendisidir (sonda satır sonu olabilir); hatada "Error: ..." döner ve o
// bir bağlantı değildir. "https://is.gd/" ile başlamayan her şey false.
[[nodiscard]] bool ParseShortLinkResponse(const std::string& body, std::wstring& out);

// Bağlantıyı kısaltır. Başarısızlık sessizdir: çağıran uzun bağlantıyla
// devam eder, çünkü yükleme zaten bitmiş ve bağlantı zaten elde.
[[nodiscard]] bool ShortenLink(const std::wstring& url, std::wstring& shortUrl);

}  // namespace crisp
