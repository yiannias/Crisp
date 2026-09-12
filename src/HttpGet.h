// HttpGet.h — Tek bir HTTPS GET isteği, gövdesi metin olan yanıtlar için.
//
// NEDEN AYRI DOSYA: UploadHttp.cpp yalnızca yüklemenin ağ yarısı ve imzası
// tamamen bir servise PNG göndermek üzerine kurulu. Güncelleme denetimi aynı
// WinHTTP tutamaç desenini ister ama gövde göndermez, servis tablosuna bakmaz
// ve hatalarını `UploadError` diliyle anlatmaz. Yükleme kodunu "bazen GET de
// yapan" bir şeye çevirmek yerine, GET'i kendi küçük dosyasına koymak daha
// dürüst — ve ileride başka bir şey (kısa bağlantı, sürüm notu) aynı işlevi
// olduğu gibi kullanabilir.
//
// TAMAMI HTTPS, UploadHttp.cpp ile aynı gerekçeyle: sertifika doğrulaması
// gevşetilmiyor ve düz HTTP yolu yok. Bir güncelleme adresini şifresiz bir
// kanaldan öğrenmek, kullanıcıyı yoldaki bir ara sunucunun seçtiği bir sayfaya
// göndermek demek olurdu.
#pragma once

#include <string>

namespace crisp {

// `https://host/path` adresine GET gönderir ve gövdeyi `body`ye yazar.
//
// ÇAĞIRAN İŞ PARÇACIĞINI BLOKLAR — bağlantı, gönderme ve alma için onar
// saniyelik zaman aşımlarıyla. Arayüz iş parçacığından çağrılmaz; çağıran bunu
// ayrı bir iş parçacığında çalıştırıp sonucu pencereye mesajla döndürür.
//
// `extraHeaders`: CRLF ile ayrılmış ek başlıklar ("User-Agent: ..." dahil —
// GitHub API'si kullanıcı aracısı olmayan isteği 403 ile reddediyor). Boş
// olabilir. `status` sunucunun durum kodudur; ağa hiç ulaşılamadıysa 0 kalır.
//
// Dönüş: yanıt alındı ve gövde okundu (durum kodu ne olursa olsun). Durum
// koduna bakmak çağıranın işi: 404 de "yanıt alındı"dır.
//
// GÖVDE 1 MiB İLE SINIRLI. Beklenen yanıt birkaç kilobayt; sınır, bozuk ya da
// kötü niyetli bir sunucunun belleği tüketmesini engeller. Gövde günlüğe
// yazılmaz.
[[nodiscard]] bool HttpGetText(const std::wstring& host, const std::wstring& path,
                               const std::wstring& extraHeaders, std::string& body,
                               unsigned& status);

}  // namespace crisp
