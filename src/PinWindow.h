// PinWindow.h — Yakalamayı ekranda üstte yüzen bir pencerede sabitler.
//
// Bu, aracın diğerlerinden ayrıldığı yer: iki şeyi yan yana karşılaştırmak için
// ikinci bir monitöre ya da bir düzenleyiciye gerek kalmıyor.
//
// Pencere KENDİ ÖMRÜNÜ YÖNETİR. Çağıran bir tutamaç almaz ve serbest bırakma
// sorumluluğu taşımaz; kullanıcı kapattığında pencere kendini yok eder. Aksi
// hâlde App, kaç iğne açıldığını ve hangisinin kapandığını izlemek zorunda
// kalırdı — iğnelerin App ile hiçbir alışverişi yok.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Görüntüyü iğneler. topLeft ekran koordinatıdır; pencere oraya, ekran
// sınırlarının içinde kalacak biçimde yerleştirilir.
// Görüntü KOPYALANIR: çağıranın Image'ı hemen yok edilebilir.
[[nodiscard]] bool PinImageToScreen(HINSTANCE instance, const Image& image,
                                    POINT topLeft);

// Aynısı, ama yakınlaştırma ve saydamlık da verilir. Diskten geri yükleme
// bunu kullanır: kullanıcının %150'de ve yarı saydam bıraktığı bir iğne, geri
// geldiğinde de öyle olmalı.
[[nodiscard]] bool PinImageWithView(HINSTANCE instance, const Image& image,
                                    POINT topLeft, int zoom, unsigned opacity);

// İğnenin görünümünün tamamı: yakınlaştırma ve saydamlığa ek olarak üç
// açma-kapama ve gizlilik. PinRecord'daki alanların birebir karşılığı, ama
// AYRI TÜR: PinRecord diskteki satırı (dosya adı, konum) anlatır, bu ise
// pencereye nasıl görüneceğini söyler; PinWindow.h PinStore.h'yi içermez.
struct PinView {
    int zoom = 100;
    unsigned opacity = 255;
    bool topMost = true;
    bool frame = false;
    bool clickThrough = false;
    bool hidden = false;   // pencere oluşturulur ama gösterilmez
};

[[nodiscard]] bool PinImageWithView(HINSTANCE instance, const Image& image,
                                    POINT topLeft, const PinView& view);

// Açık tüm iğneleri kapatır (uygulama çıkışında).
void CloseAllPins() noexcept;

// GRUP GİZLE/GÖSTER. Tepsi menüsü ve kısayol tüm iğneleri tek seferde saklar:
// ekran bir sunum ya da toplantı için boşaltılır, sonra hepsi geri gelir.
// Görünürlük PENCEREDE YAŞAR (IsWindowVisible); ayrıca bir alan tutulmaz,
// yoksa ikisi bir gün ayrışırdı. Tıklama-geçirgen bir iğne gizlenip
// gösterildiğinde geçirgenliğini korur: gizlemek yalnızca ShowWindow'dur.
void HideAllPins(bool hide) noexcept;
[[nodiscard]] bool AnyPinVisible() noexcept;
[[nodiscard]] int OpenPinCount() noexcept;

// AÇIK İĞNELERİ DİSKE YAZAR.
//
// İĞNELER ÇIKIŞTA YOK OLUYORDU: yan yana karşılaştırmak için ekrana konmuş bir
// görüntü, bir yeniden başlatmayla kayboluyordu ve hiçbir yere kaydedilmediği
// için geri getirmenin yolu da yoktu.
//
// `CloseAllPins`ten ÖNCE çağrılmalı: kapatılan pencerenin durumu silinir.
void SaveOpenPins();

// Diskteki iğneleri geri yükler; kaç tanesinin açıldığını döndürür.
[[nodiscard]] int RestorePins(HINSTANCE instance);

}  // namespace crisp
