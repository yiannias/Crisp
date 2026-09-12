// HttpGet.h — Tek bir HTTPS GET, gövdeyi metin olarak döndürür.
//
// NEDEN UploadHttp.cpp'DE DEĞİL: oradaki fonksiyon bir PNG göndermek için
// kurulmuş — multipart gövde, servise göre başlık, yanıttan bağlantı çıkarma.
// Bağlantı kısaltma ve sürüm denetimi ise yalnızca "şu adresi getir" diyor.
// İkisini tek fonksiyona sıkıştırmak, birinin her hata yolunu diğerinin
// bayraklarıyla doldurmak olurdu.
//
// TAMAMI HTTPS ve sertifika doğrulaması gevşetilmiyor; UploadHttp.cpp'deki
// gerekçe burada da geçerli.
#pragma once

#include <string>

namespace crisp {

// `host` ("is.gd"), `path` ("/create.php?...") ve CRLF ile ayrılmış ek
// başlıklar (boş olabilir). Yanıt geldiyse true: `status` HTTP durum kodu,
// `body` ham gövde (1 MiB ile sınırlı). Ağ ya da TLS hatası false döner.
// 2xx dışı bir durum da true'dur — çağıran koda bakar.
[[nodiscard]] bool HttpGetText(const std::wstring& host, const std::wstring& path,
                               const std::wstring& extraHeaders, std::string& body,
                               unsigned& status);

}  // namespace crisp
