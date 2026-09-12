// TestQrCode.cpp — QR kodlayıcının kendi içinde tutarlılığı.
//
// Bu dosya tabloları, Reed-Solomon'u, sürüm seçimini ve çizimi sınar.
// Matrisin standarda göre bağımsız bir yoldan geri okunduğu gidiş-dönüş
// testleri TestQrRoundTrip.cpp'de.
#include "TestFramework.h"

#include "QrCode.h"
#include "QrInternal.h"

#include "ImageCodec.h"

#include <cstdlib>
#include <string>
#include <vector>

using namespace crisp;

// --- Tablolar -----------------------------------------------------------------

CRISP_TEST(QrCode, Toplam_kod_sozcugu_formulle_tutar) {
    // Tablo elle yazıldı; formül standardın modül sayımından. İkisi
    // birbirini denetler.
    for (int version = 1; version <= 40; ++version) {
        int modules = (16 * version + 128) * version + 64;
        if (version >= 2) {
            const int align = version / 7 + 2;
            modules -= (25 * align - 10) * align - 55;
            if (version >= 7) {
                modules -= 36;
            }
        }
        CHECK_EQ(qr::TotalCodewords(version), modules / 8);
    }
}

CRISP_TEST(QrCode, Hizalama_konumlari_formulle_tutar) {
    CHECK(qr::AlignmentPositions(1).empty());
    for (int version = 2; version <= 40; ++version) {
        const int size = 17 + 4 * version;
        const int count = version / 7 + 2;
        const int step = version == 32 ? 26 : (version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
        std::vector<int> expect;
        expect.push_back(6);
        for (int i = count - 2; i >= 0; --i) {
            expect.push_back(size - 7 - i * step);
        }
        CHECK(qr::AlignmentPositions(version) == expect);
    }
}

CRISP_TEST(QrCode, Blok_yapisi_tutarli) {
    for (int version = 1; version <= 40; ++version) {
        for (int level = 0; level < 4; ++level) {
            const qr::EcParams p = qr::EcParamsFor(version, static_cast<QrEcLevel>(level));
            CHECK(p.blocks >= 1);
            CHECK(p.ecPerBlock >= 7 && p.ecPerBlock <= 30);
            const int longBlocks = p.blocks - p.shortBlocks;
            CHECK_EQ(p.shortBlocks * p.shortDataLen + longBlocks * (p.shortDataLen + 1),
                     p.dataCodewords);
            CHECK(p.dataCodewords > 0);
        }
        // Daha yüksek düzey daha az veri taşır.
        CHECK(qr::EcParamsFor(version, QrEcLevel::L).dataCodewords >
              qr::EcParamsFor(version, QrEcLevel::M).dataCodewords);
        CHECK(qr::EcParamsFor(version, QrEcLevel::M).dataCodewords >
              qr::EcParamsFor(version, QrEcLevel::Q).dataCodewords);
        CHECK(qr::EcParamsFor(version, QrEcLevel::Q).dataCodewords >
              qr::EcParamsFor(version, QrEcLevel::H).dataCodewords);
    }
    // Standardın bilinen değerleri.
    CHECK_EQ(qr::EcParamsFor(1, QrEcLevel::M).dataCodewords, 16);
    CHECK_EQ(qr::EcParamsFor(5, QrEcLevel::Q).blocks, 4);
    CHECK_EQ(qr::EcParamsFor(5, QrEcLevel::Q).shortBlocks, 2);
    CHECK_EQ(qr::EcParamsFor(5, QrEcLevel::Q).shortDataLen, 15);
    CHECK_EQ(qr::EcParamsFor(40, QrEcLevel::H).dataCodewords, 1276);
    CHECK_EQ(qr::EcParamsFor(40, QrEcLevel::L).dataCodewords, 2956);
}

CRISP_TEST(QrCode, Bayt_kapasitesi) {
    CHECK_EQ(qr::ByteCapacity(1, QrEcLevel::M), 14);
    CHECK_EQ(qr::ByteCapacity(2, QrEcLevel::M), 26);
    CHECK_EQ(qr::ByteCapacity(3, QrEcLevel::M), 42);
    CHECK_EQ(qr::ByteCapacity(1, QrEcLevel::L), 17);
    CHECK_EQ(qr::ByteCapacity(1, QrEcLevel::H), 7);
    CHECK_EQ(qr::ByteCapacity(10, QrEcLevel::M), 213);
    CHECK_EQ(qr::ByteCapacity(40, QrEcLevel::L), 2953);
    CHECK_EQ(qr::ByteCapacity(40, QrEcLevel::H), 1273);
}

// --- Reed-Solomon ---------------------------------------------------------------

CRISP_TEST(QrCode, Galois_carpimi) {
    CHECK_EQ(qr::GfMultiply(0, 0x53), 0);
    CHECK_EQ(qr::GfMultiply(1, 0x53), 0x53);
    CHECK_EQ(qr::GfMultiply(2, 0x80), 0x1D);      // x^8 = x^4+x^3+x^2+1
    // α^255 = 1: alan 255 elemanlı çarpımsal grup.
    uint8_t power = 1;
    for (int i = 0; i < 255; ++i) {
        power = qr::GfMultiply(power, 2);
    }
    CHECK_EQ(power, 1);
    // α^8 = 0x1D, α^16 = ?; en azından α^i sıfır olmamalı ve 255'ten önce
    // 1'e dönmemeli (ilkel eleman).
    power = 2;
    bool primitive = true;
    for (int i = 1; i < 255; ++i) {
        if (power == 1) {
            primitive = false;
        }
        power = qr::GfMultiply(power, 2);
    }
    CHECK(primitive);
}

CRISP_TEST(QrCode, Uretec_polinomlari) {
    // Standardın Ek A'sındaki üreteçlerin tam sayı hâli (baş katsayı hariç).
    const std::vector<uint8_t> g7{127, 122, 154, 164, 11, 68, 117};
    CHECK(qr::ReedSolomonDivisor(7) == g7);
    const std::vector<uint8_t> g10{216, 194, 159, 111, 199, 94, 95, 113, 157, 193};
    CHECK(qr::ReedSolomonDivisor(10) == g10);
}

CRISP_TEST(QrCode, Bilinen_hata_duzeltme_vektoru) {
    // 1-M "HELLO WORLD" (alfasayısal) örneği: 16 veri sözcüğü ve 10 hata
    // düzeltme sözcüğü, standardın anlatım sırasına göre.
    const std::vector<uint8_t> data{32, 91, 11, 120, 209, 114, 220, 77,
                                    67, 64, 236, 17, 236, 17, 236, 17};
    const std::vector<uint8_t> expect{196, 35, 39, 119, 235, 215, 231, 226, 93, 23};
    CHECK(qr::ReedSolomonRemainder(data, qr::ReedSolomonDivisor(10)) == expect);
}

// --- Sürüm seçimi ---------------------------------------------------------------

CRISP_TEST(QrCode, Surum_secimi) {
    QrCode code;
    CHECK(EncodeQr(std::string(14, 'a'), code));
    CHECK_EQ(code.version, 1);
    CHECK(EncodeQr(std::string(15, 'a'), code));
    CHECK_EQ(code.version, 2);
    CHECK(EncodeQr(std::string(26, 'a'), code));
    CHECK_EQ(code.version, 2);
    CHECK(EncodeQr(std::string(27, 'a'), code));
    CHECK_EQ(code.version, 3);
    CHECK(EncodeQr(std::string(17, 'a'), code, QrEcLevel::L));
    CHECK_EQ(code.version, 1);
    CHECK(EncodeQr("", code));
    CHECK_EQ(code.version, 1);
}

CRISP_TEST(QrCode, Cok_uzun_girdi_reddedilir) {
    QrCode code;
    CHECK(EncodeQr(std::string(2953, 'x'), code, QrEcLevel::L));
    CHECK_EQ(code.version, 40);
    CHECK(!EncodeQr(std::string(2954, 'x'), code, QrEcLevel::L));
    CHECK(!EncodeQr(std::string(1274, 'x'), code, QrEcLevel::H));
}

// --- Çizim ---------------------------------------------------------------------

CRISP_TEST(QrCode, Cizim_boyutlari_ve_sessiz_bolge) {
    QrCode code;
    CHECK(EncodeQr("https://is.gd/abc", code));
    Image image;
    CHECK(RenderQr(code, 4, 3, image));
    CHECK_EQ(image.Width(), (code.size + 6) * 4);
    CHECK_EQ(image.Height(), (code.size + 6) * 4);

    // Sessiz bölge tümüyle beyaz.
    bool white = true;
    for (int y = 0; y < image.Height() && white; ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            const bool inQuiet = x < 12 || y < 12 || x >= image.Width() - 12 ||
                                 y >= image.Height() - 12;
            if (inQuiet && image.Pixel(x, y) != 0xFFFFFFFFu) {
                white = false;
                break;
            }
        }
    }
    CHECK(white);

    // Modüller 4x4 bloklar hâlinde, sol üst bulucunun köşesi siyah.
    CHECK_EQ(image.Pixel(12, 12), 0xFF000000u);
    CHECK_EQ(image.Pixel(15, 15), 0xFF000000u);
    CHECK_EQ(image.Pixel(16, 16), 0xFFFFFFFFu);   // (1,1) bulucunun açık halkası

    // Her modül görüntüye yansımış.
    bool consistent = true;
    for (int y = 0; y < code.size && consistent; ++y) {
        for (int x = 0; x < code.size; ++x) {
            const uint32_t pixel = image.Pixel((x + 3) * 4 + 1, (y + 3) * 4 + 2);
            if ((pixel == 0xFF000000u) != code.Module(x, y)) {
                consistent = false;
                break;
            }
        }
    }
    CHECK(consistent);

    Image sized;
    CHECK(RenderQrToSize(code, 220, 4, sized));
    CHECK(sized.Width() <= 220);
    CHECK_EQ(sized.Width() % (code.size + 8), 0);

    Image bad;
    CHECK(!RenderQr(code, 0, 4, bad));
    CHECK(!RenderQr(QrCode{}, 4, 4, bad));
}

// DIŞ ÇÖZÜCÜ İÇİN DÖKÜM. Buradaki testler kodun kendi kurallarıyla tutarlı
// olduğunu gösterir; bir telefonun onu OKUYABİLDİĞİNİ yalnızca bağımsız bir
// çözücü gösterir. CRISP_QR_DUMP_DIR ortam değişkeni verilirse üç kod PNG
// olarak oraya yazılır; verilmezse test hiçbir şey yapmaz.
CRISP_TEST(QrCode, Dis_cozucu_icin_png_dokumu) {
    wchar_t* dir = nullptr;
    size_t length = 0;
    if (::_wdupenv_s(&dir, &length, L"CRISP_QR_DUMP_DIR") != 0 || dir == nullptr) {
        CHECK(true);
        return;
    }
    const std::wstring folder{dir};
    ::free(dir);

    struct Sample {
        const wchar_t* file;
        std::string text;
        QrEcLevel ec;
    };
    const Sample samples[] = {
        {L"qr-url.png", "https://files.catbox.moe/ab12cd.png", QrEcLevel::M},
        {L"qr-v7.png", std::string(120, 'x') + "?id=ç-ü-ş", QrEcLevel::L},
        {L"qr-long.png", std::string(300, 'Q'), QrEcLevel::Q},
    };
    for (const Sample& sample : samples) {
        QrCode qr;
        CHECK(EncodeQr(sample.text, qr, sample.ec));
        Image image;
        CHECK(RenderQr(qr, 8, 4, image));
        CHECK(SavePng(image, folder + L"\\" + sample.file));
    }
}
