// TestPinStore.cpp — İğnelerin diskteki kaydının satır biçimi.
//
// DOSYAYA DOKUNMADAN SINANIR. Biçimlendirme ve çözümleme açıkta duruyor tam da
// bunun için: bir iğnenin konumunun yanlış geri yüklenmesi ekranda kolayca
// gözden kaçar, ama gidiş-dönüş sınaması onu anında yakalar.
#include "TestFramework.h"

#include "PinStore.h"

using namespace crisp;

CRISP_TEST(PinStore, Gidis_donus_degeri_korur) {
    PinRecord record;
    record.imageFile = L"pin-03.png";
    record.x = -1920;   // soldaki ikinci monitör
    record.y = 240;
    record.zoom = 150;
    record.opacity = 140;

    PinRecord back;
    CHECK(ParsePinLine(FormatPinLine(record), back));
    CHECK(back.imageFile == record.imageFile);
    CHECK_EQ(back.x, record.x);
    CHECK_EQ(back.y, record.y);
    CHECK_EQ(back.zoom, record.zoom);
    CHECK_EQ(back.opacity, record.opacity);
}

CRISP_TEST(PinStore, Bozuk_satir_reddedilir) {
    PinRecord out;
    CHECK(!ParsePinLine(L"", out));
    CHECK(!ParsePinLine(L"yalnizca metin", out));
    // Alan eksik: dosya adı yok.
    CHECK(!ParsePinLine(L"10\t20\t100\t255", out));
    // Dosya adı boş.
    CHECK(!ParsePinLine(L"10\t20\t100\t255\t", out));
}

CRISP_TEST(PinStore, Klasor_disina_cikan_ad_reddedilir) {
    // İNDEKS ELLE DÜZENLENEBİLİR BİR METİN DOSYASI. İçine yol yazılmış bir
    // satır, uygulamayı iğne klasörünün dışındaki bir dosyayı okumaya ikna
    // edebilirdi.
    PinRecord out;
    CHECK(!ParsePinLine(L"0\t0\t100\t255\t..\\..\\gizli.png", out));
    CHECK(!ParsePinLine(L"0\t0\t100\t255\tC:\\Windows\\x.png", out));
    CHECK(!ParsePinLine(L"0\t0\t100\t255\talt/klasor.png", out));
}

CRISP_TEST(PinStore, Aralik_disi_degerler_duzeltilir) {
    // Bozuk bir sayı yüzünden iğneyi HİÇ göstermemektense makul bir değerle
    // göstermek yeğdir: kullanıcı görüntüsünü geri istiyor.
    PinRecord out;
    CHECK(ParsePinLine(L"0\t0\t99999\t99999\tpin-00.png", out));
    CHECK(out.zoom <= 800);
    CHECK(out.opacity <= 255u);

    CHECK(ParsePinLine(L"0\t0\t1\t0\tpin-00.png", out));
    CHECK(out.zoom >= 10);
    CHECK(out.opacity >= 20u);
}

CRISP_TEST(PinStore, Bayraklar_her_bilesimde_gidip_gelir) {
    // ON ALTI BİLEŞİMİN HEPSİ: dört bayrağın her biri kendi harfiyle yazılır ve
    // bir harfin bir başkasının yuvasına kayması yalnızca belirli bir
    // bileşimde görünürdü.
    for (int mask = 0; mask < 16; ++mask) {
        PinRecord record;
        record.imageFile = L"pin-00.png";
        record.topMost = (mask & 1) != 0;
        record.frame = (mask & 2) != 0;
        record.clickThrough = (mask & 4) != 0;
        record.hidden = (mask & 8) != 0;

        PinRecord back;
        CHECK(ParsePinLine(FormatPinLine(record), back));
        CHECK_EQ(back.topMost, record.topMost);
        CHECK_EQ(back.frame, record.frame);
        CHECK_EQ(back.clickThrough, record.clickThrough);
        CHECK_EQ(back.hidden, record.hidden);
        CHECK(back.imageFile == record.imageFile);
    }
}

CRISP_TEST(PinStore, Bayrak_alani_biciminde_dosya_adindan_sonra) {
    PinRecord record;
    record.imageFile = L"pin-01.png";
    record.x = 10;
    record.y = 20;
    record.zoom = 100;
    record.opacity = 255;
    CHECK_STR(FormatPinLine(record), L"10\t20\t100\t255\tpin-01.png\tT---");

    record.topMost = false;
    record.frame = true;
    record.clickThrough = true;
    record.hidden = true;
    CHECK_STR(FormatPinLine(record), L"10\t20\t100\t255\tpin-01.png\t-FCH");
}

CRISP_TEST(PinStore, Eski_bes_alanli_satir_varsayilanlarla_okunur) {
    // 0.8 SÜRÜMÜNÜN DOSYASI. Güncellemeden sonra iğnelerin kaybolması, yeni
    // bir bayrak eklemenin bedeli olamaz.
    PinRecord out;
    out.topMost = false;   // çözümleyici alanı sıfırlamalı, önceki değeri değil
    CHECK(ParsePinLine(L"10\t20\t150\t140\tpin-02.png", out));
    CHECK(out.imageFile == L"pin-02.png");
    CHECK_EQ(out.zoom, 150);
    CHECK_EQ(out.opacity, 140);
    CHECK(out.topMost);
    CHECK(!out.frame);
    CHECK(!out.clickThrough);
    CHECK(!out.hidden);
}

CRISP_TEST(PinStore, Bozuk_bayrak_alani_hosgorulur) {
    // Bayrak alanı elle bozulmuşsa satır YİNE kabul edilir; görüntüyü ve
    // konumu bir harf yüzünden atmak orantısız olurdu. Bayraklar varsayılana
    // döner — bozuk alandaki rastgele bir T'ye güvenilmez.
    PinRecord out;
    CHECK(ParsePinLine(L"10\t20\t100\t255\tpin-03.png\tTF?H", out));
    CHECK(out.imageFile == L"pin-03.png");
    CHECK(out.topMost);
    CHECK(!out.frame);
    CHECK(!out.hidden);

    CHECK(ParsePinLine(L"10\t20\t100\t255\tpin-03.png\t1234", out));
    CHECK(out.topMost);
    CHECK(!out.clickThrough);

    // Boş bayrak alanı (satır sonunda yalnız bir sekme): varsayılanlar.
    CHECK(ParsePinLine(L"10\t20\t100\t255\tpin-03.png\t", out));
    CHECK(out.topMost);
    CHECK(!out.frame);
}

CRISP_TEST(PinStore, Bayrak_sirasi_onemsiz) {
    // Dosya elle düzenlenebilir; "HCFT" ile "TFCH" aynı şeyi söylemeli. Yer
    // tutucu `-` de zorunlu değil.
    PinRecord out;
    CHECK(ParsePinLine(L"0\t0\t100\t255\tpin-04.png\tHCFT", out));
    CHECK(out.topMost);
    CHECK(out.frame);
    CHECK(out.clickThrough);
    CHECK(out.hidden);

    CHECK(ParsePinLine(L"0\t0\t100\t255\tpin-04.png\tC", out));
    CHECK(!out.topMost);
    CHECK(!out.frame);
    CHECK(out.clickThrough);
    CHECK(!out.hidden);

    CHECK(ParsePinLine(L"0\t0\t100\t255\tpin-04.png\t--F-", out));
    CHECK(!out.topMost);
    CHECK(out.frame);
}

CRISP_TEST(PinStore, Yol_uygulama_verisinin_altinda) {
    const std::wstring folder = PinFolder();
    const std::wstring index = PinIndexPath();
    CHECK(!folder.empty());
    CHECK(folder.find(L"\\Crisp\\Pins") != std::wstring::npos);
    // İndeks klasörün İÇİNDE olmalı: dışarıda bir dosya, klasörü silen
    // kullanıcının yarım bir durumla kalması demekti.
    CHECK(index.rfind(folder, 0) == 0);
}
