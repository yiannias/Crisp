// TestUpdateCheck.cpp — Güncelleme denetiminin ağ gerektirmeyen yarısı.
//
// HİÇBİR TEST AĞA ÇIKMAZ: sınanan şey, GitHub'ın yanıtının doğru okunduğu ve
// iki sürümün doğru karşılaştırıldığı. Sunucunun o an ayakta olması bir test
// paketinin koşulu olamaz.
#include "TestFramework.h"

#include "UpdateCheck.h"

#include <string>

using namespace crisp;

namespace {

// GitHub'ın `releases/latest` yanıtının kısaltılmış ama gerçekçi bir hâli:
// üstte `url` alanı, iç içe `author` nesnesinde ikinci bir `html_url`, ve
// `assets` dizisi. Ayrıştırıcının iç içe olanı değil üst düzeydekini bulması
// gerekiyor.
[[nodiscard]] std::string GitHubJson() {
    return R"({
  "url": "https://api.github.com/repos/shadesofdeath/Crisp/releases/1",
  "html_url": "https://github.com/shadesofdeath/Crisp/releases/tag/v0.9.1",
  "id": 1,
  "author": {
    "login": "shadesofdeath",
    "html_url": "https://github.com/shadesofdeath",
    "type": "User"
  },
  "tag_name": "v0.9.1",
  "target_commitish": "main",
  "name": "Crisp 0.9.1",
  "draft": false,
  "prerelease": false,
  "assets": [
    {
      "name": "Crisp.exe",
      "browser_download_url": "https://github.com/shadesofdeath/Crisp/releases/download/v0.9.1/Crisp.exe"
    }
  ],
  "body": "Notes with \"quotes\" and {braces}"
})";
}

}  // namespace

CRISP_TEST(UpdateCheck, Esit_surumler_sifir) {
    CHECK_EQ(CompareVersions(L"0.8.0", L"0.8.0"), 0);
    CHECK_EQ(CompareVersions(L"1.2.3", L"1.2.3"), 0);
}

CRISP_TEST(UpdateCheck, Buyuk_kucuk_yama) {
    CHECK(CompareVersions(L"1.0.0", L"0.9.9") > 0);
    CHECK(CompareVersions(L"0.9.9", L"1.0.0") < 0);
    CHECK(CompareVersions(L"0.9.0", L"0.8.5") > 0);
    CHECK(CompareVersions(L"0.8.5", L"0.9.0") < 0);
    CHECK(CompareVersions(L"0.8.1", L"0.8.0") > 0);
    CHECK(CompareVersions(L"0.8.0", L"0.8.1") < 0);
}

CRISP_TEST(UpdateCheck, Sayisal_karsilastirma_metinsel_degil) {
    // Metin olarak "0.9.10" < "0.9.9"; sürüm olarak tersi.
    CHECK(CompareVersions(L"0.9.10", L"0.9.9") > 0);
    CHECK(CompareVersions(L"0.10.0", L"0.9.0") > 0);
    CHECK(CompareVersions(L"2.0.0", L"10.0.0") < 0);
}

CRISP_TEST(UpdateCheck, v_oneki_atlanir) {
    CHECK_EQ(CompareVersions(L"v0.8.0", L"0.8.0"), 0);
    CHECK_EQ(CompareVersions(L"V1.2.3", L"v1.2.3"), 0);
    CHECK(CompareVersions(L"v0.9.0", L"0.8.0") > 0);
}

CRISP_TEST(UpdateCheck, Eksik_parcalar_sifir) {
    CHECK_EQ(CompareVersions(L"1.2", L"1.2.0"), 0);
    CHECK_EQ(CompareVersions(L"1", L"1.0.0"), 0);
    CHECK(CompareVersions(L"1.2", L"1.2.1") < 0);
    CHECK(CompareVersions(L"1.2.1", L"1.2") > 0);
    CHECK_EQ(CompareVersions(L"", L"0.0.0"), 0);
}

CRISP_TEST(UpdateCheck, Sayidan_sonraki_cop_yok_sayilir) {
    CHECK_EQ(CompareVersions(L"1.2.3-beta", L"1.2.3"), 0);
    CHECK_EQ(CompareVersions(L"1.2.3rc1", L"1.2.3"), 0);
    CHECK(CompareVersions(L"1.2.4-beta", L"1.2.3") > 0);
    // Sayı olmayan bir parça sıfırdır, ayrıştırmayı bozmaz.
    CHECK(CompareVersions(L"1.x.5", L"1.0.4") > 0);
    CHECK(CompareVersions(L"abc", L"0.0.1") < 0);
}

CRISP_TEST(UpdateCheck, GitHub_yaniti_okunur) {
    UpdateInfo info;
    CHECK(ParseLatestRelease(GitHubJson(), info));
    CHECK_STR(info.version, L"0.9.1");
    CHECK_STR(info.url, L"https://github.com/shadesofdeath/Crisp/releases/tag/v0.9.1");
}

CRISP_TEST(UpdateCheck, v_siz_etiket_de_okunur) {
    UpdateInfo info;
    CHECK(ParseLatestRelease(R"({"tag_name":"1.0.0","html_url":"https://github.com/x"})",
                             info));
    CHECK_STR(info.version, L"1.0.0");
    CHECK_STR(info.url, L"https://github.com/x");
}

CRISP_TEST(UpdateCheck, Bozuk_json_reddedilir) {
    UpdateInfo info;
    CHECK(!ParseLatestRelease("", info));
    CHECK(!ParseLatestRelease("not json at all", info));
    CHECK(!ParseLatestRelease(R"({"tag_name": )", info));
    // GitHub'ın hata gövdesi: JSON ama etiket yok.
    CHECK(!ParseLatestRelease(R"({"message":"Not Found","status":"404"})", info));
    // Etiket var ama dize değil.
    CHECK(!ParseLatestRelease(R"({"tag_name": 42})", info));
    // Yalnızca 'v': sürüm yok.
    CHECK(!ParseLatestRelease(R"({"tag_name":"v"})", info));
    CHECK(info.version.empty());
}

CRISP_TEST(UpdateCheck, Url_olmadan_da_surum_okunur) {
    UpdateInfo info;
    CHECK(ParseLatestRelease(R"({"tag_name":"v0.9.2"})", info));
    CHECK_STR(info.version, L"0.9.2");
    CHECK(info.url.empty());
}

CRISP_TEST(UpdateCheck, IsNewer_calisan_surume_gore) {
    UpdateInfo info;
    CHECK(ParseLatestRelease(GitHubJson(), info));   // 0.9.1
    CHECK(IsNewer(info, L"0.8.0"));
    CHECK(IsNewer(info, L"0.9.0"));
    CHECK(!IsNewer(info, L"0.9.1"));
    CHECK(!IsNewer(info, L"0.9.2"));
    CHECK(!IsNewer(info, L"1.0.0"));

    // Boş bilgi asla "yeni" değildir; kutu boşuna açılmasın.
    const UpdateInfo none;
    CHECK(!IsNewer(none, L"0.0.0"));
}
