// Stitch.h — Üst üste binen kareleri tek bir uzun görüntüde birleştirir.
//
// KAYDIRMALI YAKALAMANIN ZOR YARISI BURASI ve tamamı ağ, pencere ya da ekran
// olmadan çalışır: iki bitmap alır, ikincisinin birincinin neresinden devam
// ettiğini bulur, ve alt alta (ya da yan yana) ekler. Pencereyi kimin
// kaydırdığı, kaç kare alındığı, kullanıcının ne gördüğü — hiçbiri bu
// dosyanın meselesi değil.
//
// SINANABİLİR OLMASI TESADÜF DEĞİL, TASARIM. Bir hizalama hatası çıplak gözle
// "birleştirme biraz kaymış" diye görünür ve elle ayıklanması saatler alır;
// sentetik bir görüntüyü bilinen bir miktar kaydırıp aynı sayıyı geri
// isteyen bir sınama, aynı hatayı bir saniyede yakalar.
//
// ÜÇ DOSYAYA BÖLÜNDÜ, ev kuralı gereği: Stitch.cpp kaydırma miktarını arar
// (her iki eksen için ortak çekirdek), StitchVertical.cpp alt alta ekler ve
// yapışık başlık/altlığı ayıklar, StitchHorizontal.cpp yan yana ekler.
#pragma once

#include "Capture.h"

#include <vector>

namespace crisp {

// İki kare arasındaki DİKEY kaydırma miktarı.
//
// `next`in üst kısmı `previous`ın alt kısmıyla örtüşür; dönen değer, `next`in
// `previous`a göre kaç piksel AŞAĞI kaydığıdır. Bulunamazsa 0.
//
// `minOverlap` karşılaştırılacak şeridin satır sayısı. Küçük tutulursa
// rastgele benzeyen iki şerit eşleşebilir; büyük tutulursa hızlı kaydırmada
// hiç eşleşme bulunamaz.
//
// ŞERİT KARENİN ÜÇTE BİRİNDEN BAŞLAR, en üstünden değil: seçilen alanın
// üstünde kaydırılmayan bir başlık ya da araç çubuğu olabilir ve sabit bir
// bölgeyi karşılaştırmak yalnızca "hiç kaymamış" cevabını verir. Bu yüzden
// bulunabilecek en büyük kaydırma, karenin üçte ikisinden şerit boyu kadar
// azdır — bir tekerlek adımının çok üstünde.
[[nodiscard]] int FindVerticalShift(const Image& previous, const Image& next,
                                    int minOverlap) noexcept;

// Aynısı, ama YAPIŞIK BAŞLIK VE ALTLIĞI BİLEREK: arama yalnızca
// [headerRows, height - footerRows) satırlarında yapılır, şerit de bu
// aralığın üçte birinden başlar.
//
// ALTLIK ARAMAYA KATILMAMALI, çünkü hiç kıpırdamıyor: yüksek bir altlıkta
// şeridin `previous`taki karşılığı büyük kaydırmalarda altlığa taşıyor ve o
// adaylar boşa eleniyordu — bulunabilir en büyük kaydırma altlık boyu kadar
// küçülüyordu. Sıfır/sıfır ile çağrılınca üç parametreli sürümün TIPATIP
// aynısıdır.
[[nodiscard]] int FindVerticalShift(const Image& previous, const Image& next,
                                    int minOverlap, int headerRows,
                                    int footerRows) noexcept;

// İki kare arasındaki YATAY kaydırma miktarı: `next`in `previous`a göre kaç
// piksel SAĞA kaydığı. Dikeyin aynadaki görüntüsü — şerit `minOverlap`
// sütun, karenin üçte birinden başlar, eşiği geçmeyen aday 0 döner.
//
// Geniş tablolar, zaman çizelgeleri ve yatay galeriler için. Dikeyle aynı
// çekirdeği paylaşır; iki eksen için iki ayrı arama yazmak, düzeltilen her
// hatayı iki kez düzeltmek olurdu.
[[nodiscard]] int FindHorizontalShift(const Image& previous, const Image& next,
                                      int minOverlap) noexcept;

// Bütün karelerde BİREBİR AYNI kalan alt satır sayısı (yapışık altlık: sohbet
// giriş kutusu, çerez uyarısı, sabit araç çubuğu). İkiden az kare, ölçü
// uyuşmazlığı ya da hiç ortak satır yoksa 0.
//
// YÜKSEKLİĞİN ÜÇTE BİRİYLE SINIRLI: aynı olan satırlar ondan fazlaysa kareler
// zaten kaymıyordur ve "altlık" diye içeriğin kendisini ayıklamak yanlış
// olurdu.
[[nodiscard]] int DetectStickyFooter(const std::vector<Image>& frames) noexcept;

// Aynısı üst satırlar için (yapışık başlık, sekme çubuğu, pencere başlığı).
[[nodiscard]] int DetectStickyHeader(const std::vector<Image>& frames) noexcept;

// Birleştirme seçenekleri.
struct StitchOptions {
    // Yapışık başlık tespit edilsin ve arama şeridi onun altında tutulsun.
    // Başlık zaten hiçbir kareden ikinci kez kopyalanmıyor (bkz.
    // StitchVertical.cpp); bu yalnızca aramayı ilgilendirir.
    bool trimStickyHeader = true;

    // Yapışık altlık tespit edilsin: aramaya katılmasın, ara karelerden
    // kopyalanmasın ve yalnızca SON kareden, bir kez, en alta eklensin.
    bool trimStickyFooter = true;
};

// Kareleri sırayla alt alta birleştirir. İlk kare tamamen, sonrakiler
// yalnızca yeni kısımları alınarak eklenir.
//
// HİÇBİR KARE ATLANMAZ, ama hiçbiri de İKİ KEZ EKLENMEZ: eşleşme
// bulunamayan bir kare, bir öncekinin devamı sayılamayacağı için birleştirmeyi
// orada BİTİRİR. Yanlış yere eklenmiş bir şerit, eksik bir şeritten çok daha
// kötüdür — ilki sessizce yanlış bir görüntü üretir.
//
// `stopped` doluysa: birleştirme kare bitmeden durdu ve kaçıncı karede
// durduğu yazılır. Çağıran bunu kullanıcıya söyleyebilir.
[[nodiscard]] bool StitchVertical(const std::vector<Image>& frames,
                                  int minOverlap, const StitchOptions& options,
                                  Image& out, size_t* stopped = nullptr);

// Varsayılan seçeneklerle (başlık ve altlık ayıklanır).
[[nodiscard]] bool StitchVertical(const std::vector<Image>& frames,
                                  int minOverlap, Image& out,
                                  size_t* stopped = nullptr);

// Kareleri yan yana birleştirir; dikeyin aynadaki görüntüsü.
//
// YAPIŞIK KENAR AYIKLAMASI YOK. Sola ya da sağa yapışık bir sütun (satır
// başlığı, kenar çubuğu) yatayda dikeydeki kadar yaygın değil ve dikeyin
// seçenek/tespit mekanizmasını buraya taşımak, sınanacak yüzeyi bir kullanım
// için ikiye katlardı. Gerekirse StitchVertical'ın deseniyle eklenir.
[[nodiscard]] bool StitchHorizontal(const std::vector<Image>& frames,
                                    int minOverlap, Image& out,
                                    size_t* stopped = nullptr);

// İki satırın ne kadar benzediği: 0 = birebir aynı, büyüdükçe farklı.
//
// Dışarıda, çünkü eşiğin ne anlama geldiğini sınamak için gerekiyor.
[[nodiscard]] uint64_t RowDifference(const Image& a, int rowA, const Image& b,
                                     int rowB) noexcept;

// Aynısı iki sütun için; yatay birleştirmenin ölçütü.
[[nodiscard]] uint64_t ColumnDifference(const Image& a, int columnA,
                                        const Image& b, int columnB) noexcept;

}  // namespace crisp
