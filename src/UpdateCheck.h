// UpdateCheck.h — Sürüm karşılaştırma ve GitHub Releases yanıtının okunması.
//
// AĞA HİÇ ÇIKMAZ ve tam olarak bu yüzden ayrı bir dosya: bir güncelleme
// denetiminde yanlış gidebilecek şeylerin neredeyse tamamı iki soruda toplanıyor
// — "yanıt doğru okundu mu" ve "bu sürüm bizimkinden yeni mi" — ve ikisi de
// sunucuya sormadan sınanabiliyor. İsteği gönderen taraf HttpGet.cpp'de, karar
// veren ve kutuyu açan taraf AppUpdate.cpp'de.
//
// NEDEN GITHUB RELEASES: sürümün yayımlandığı tek yer orası ve ayrı bir sürüm
// sunucusu tutmak, bakılacak bir şey daha demek. `releases/latest` yalnızca
// taslak ve ön sürüm olmayan yayınları döndürüyor; kullanıcıya bir gece
// derlemesi önerilmez.
#pragma once

#include <string>

namespace crisp {

// Noktalı sayısal sürüm karşılaştırması: "0.9.0" ile "0.10.1" gibi.
// Negatif: a < b; sıfır: eşit; pozitif: a > b.
//
// SAYI SAYI KARŞILAŞTIRILIR, METİN DEĞİL. "0.9.10" metin olarak "0.9.9"dan
// küçüktür ve sürüm olarak büyüktür; ilk ondan sonraki yama bu hatayı yaşardı.
// Baştaki 'v'/'V' atlanır (GitHub etiketleri öyle yazılıyor), eksik parçalar
// sıfır sayılır ("1.2" == "1.2.0"), sayıdan sonraki harfler yok sayılır
// ("1.2.3-beta" == "1.2.3" — ön sürüm ayrımı yapılmıyor, bkz. üstteki not).
[[nodiscard]] int CompareVersions(const std::wstring& a, const std::wstring& b) noexcept;

// GitHub'ın `releases/latest` yanıtından çıkarılan iki alan.
struct UpdateInfo {
    std::wstring version;   // "0.9.1" — etiketin 'v' öneki atılmış hâli
    std::wstring url;       // yayının sayfası (html_url)
};

// JSON gövdesinden `tag_name` ile `html_url` alanlarını okur. Etiket boşsa ya
// da gövde JSON değilse false; url'nin boş olması ayrıştırmayı bozmaz.
[[nodiscard]] bool ParseLatestRelease(const std::string& json, UpdateInfo& out);

// Yayındaki sürüm, çalışan sürümden büyük mü.
[[nodiscard]] bool IsNewer(const UpdateInfo& info, const std::wstring& current) noexcept;

}  // namespace crisp
