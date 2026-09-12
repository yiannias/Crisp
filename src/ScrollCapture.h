// ScrollCapture.h — Bir alanı kaydıra kaydıra defalarca yakalar.
//
// UZUN BİR SAYFANIN EKRANA SIĞMAYAN KISMI. Crisp'in yakalayabildiği her şey
// şimdiye kadar ekranda görünen şeydi; bir sohbet geçmişini ya da uzun bir
// sayfayı almak için ekran görüntüsünü üç kez alıp elle birleştirmek
// gerekiyordu.
//
// BURASI YALNIZCA KARELERİ TOPLAR. Onları tek bir görüntüde birleştiren
// Stitch.h, hangi tuşun bunu tetiklediği uygulama katmanı. Ayrım işe yarıyor:
// birleştirme ağır iş ve tamamen sınanabilir, toplama ise sınanamaz çünkü
// gerçek bir pencerenin gerçekten kaydırılmasını gerektiriyor.
//
// KAYDIRMA GERÇEK GİRDİYLE YAPILIR (`SendInput`), pencereye mesaj
// gönderilerek değil. `WM_MOUSEWHEEL`i doğrudan almak birçok uygulamada
// çalışıyor ama tarayıcılar ve Electron pencereleri onu yok sayıyor; gerçek
// tekerlek olayını ise kullanıcının kendi kaydırmasından ayırt edemiyorlar.
#pragma once

#include "Capture.h"

#include <vector>

namespace crisp {

// Kareler arasında aranacak en az örtüşme, satır (yatayda sütun). Bir
// tekerlek çentiği genelde ~50 piksel; üç çentik ~150. Yüz satırlık bir şerit
// o hızda bile paylaşılıyor ve iki kareyi ayırt etmeye fazlasıyla yetiyor.
//
// BAŞLIKTA, ÇÜNKÜ İKİ TARAF DA OKUYOR: kareleri toplayan taraf "yeni içerik
// var mı" diye sorarken, birleştiren taraf da aynı sayıyı kullanmalı. İkisi
// ayrı olsaydı, toplama sırasında kabul edilen bir kare birleştirmede
// reddedilebilirdi.
inline constexpr int kScrollOverlapRows = 100;

// Kaydırmanın yönü.
//
// AUTO ÖNCE DİKEY DENER: kaydırarak yakalanan şey neredeyse her zaman uzun
// bir sayfa. Sayfa dikeyde kıpırdamazsa bir de yatay tekerlek denenir —
// geniş tablolar, zaman çizelgeleri, yatay galeriler. İkisi de kıpırdatmazsa
// tek kare teslim edilir. İki ekseni aynı anda aramak yanlış eşleşme
// ihtimalini artırırdı; sırayla denemek bir adımlık bir gecikmeye mal oluyor.
enum class ScrollDirection { Auto, Vertical, Horizontal };

// Kaydırmalı yakalamanın ayarları.
struct ScrollCaptureOptions {
    // En fazla kaç kare alınacağı. Sınır var çünkü sonsuz kaydıran sayfalar
    // (sonsuz akış) var ve onlarda durma koşulu hiç gelmez.
    int maxFrames = 30;

    // Her adımda kaç tekerlek çentiği. Bir çentik genelde üç satır; büyük
    // değerler hızlandırır ama örtüşmeyi azaltıp eşleşmeyi riske atar.
    int notchesPerStep = 3;

    // Kaydırmadan sonra beklenecek süre. Yumuşak kaydırma animasyonu biten
    // sayfayı yakalamak için; kısa tutulursa hareket hâlindeki bir kare
    // yakalanır ve o kare hiçbir şeyle eşleşmez.
    unsigned settleMs = 260;

    // Hangi yönde kaydırılacağı; Auto yukarıda anlatıldığı gibi karar verir.
    ScrollDirection direction = ScrollDirection::Auto;

    // Yön KESİNLEŞTİĞİNDE bir kez çağrılır (Auto'da ilk hareket eden adımdan
    // sonra, belirli bir yönde hemen). Toplama saniyeler sürüyor ve bu
    // süreyi çağıran göremiyor; "yana kaydırılıyor" bildirimini toplama
    // bittikten sonra göstermek anlamsız olurdu. Ham işlev göstericisi +
    // bağlam: std::function'ın bu tek çağrı için getireceği bir şey yok.
    void (*onDirection)(ScrollDirection direction, void* context) = nullptr;
    void* onDirectionContext = nullptr;
};

// Kareleri toplar. `region` ekran koordinatında ve kaydırılacak pencerenin
// üstünde olmalı.
//
// İMLEÇ BÖLGENİN İÇİNE TAŞINIR ve orada kalır: tekerlek olayı imlecin
// altındaki pencereye gider. Dönüşte imleç çağıranın bıraktığı yere geri
// konur — kaydırma bittiğinde farenin ekranın ortasında durması, kullanıcının
// yapmadığı bir hareket olurdu.
//
// Kareler arasında yeni içerik kalmadığında ERKEN DURUR: aynı görüntüyü
// otuz kez yakalamanın kimseye faydası yok.
//
// `used` doluysa, karelerin hangi yönde toplandığı yazılır; çağıran buna göre
// StitchVertical ya da StitchHorizontal seçer. Hiç kaydırılamayan (tek kare)
// bir toplama Vertical der: birleştirme tek kareyi olduğu gibi verir.
[[nodiscard]] bool CollectScrollFrames(const RECT& region,
                                       const ScrollCaptureOptions& options,
                                       std::vector<Image>& frames,
                                       ScrollDirection* used = nullptr);

}  // namespace crisp
