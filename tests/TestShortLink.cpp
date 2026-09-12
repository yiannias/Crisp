// TestShortLink.cpp — is.gd isteğinin kurulması ve yanıtının çözülmesi.
//
// AĞA ÇIKILMAZ: `ShortenLink` burada çağrılmıyor. Sınanan, isteğin doğru
// kurulduğu ve dönen metnin doğru okunduğu — kısaltma özelliğinde yanlış
// gidebilecek her şey bu ikisinde.
#include "TestFramework.h"

#include "ShortLink.h"

#include <string>

using namespace crisp;

CRISP_TEST(ShortLink, Yol_duz_baglantiyi_kodlar) {
    const std::string path = BuildIsGdPath(L"https://files.catbox.moe/ab12.png");
    CHECK(path == "/create.php?format=simple&url=https%3A%2F%2Ffiles.catbox.moe%2Fab12.png");
}

CRISP_TEST(ShortLink, Yol_ayrilmis_karakterleri_kodlar) {
    // "&" ve "#" kodlanmazsa is.gd adresi orada keser; "=" ve "?" de sorgu
    // ayrıştırıcısını şaşırtır. Ayrılmamış "-._~" olduğu gibi kalmalı.
    const std::string path = BuildIsGdPath(L"https://x.io/a-b_c.d~e?k=v&m=n#frag x");
    CHECK(path == "/create.php?format=simple&url="
                  "https%3A%2F%2Fx.io%2Fa-b_c.d~e%3Fk%3Dv%26m%3Dn%23frag%20x");
}

CRISP_TEST(ShortLink, Yol_utf8_kodlar) {
    // "ç" (U+00E7) UTF-8'de C3 A7, "ı" (U+0131) C4 B1.
    const std::string path = BuildIsGdPath(L"https://x.io/çı");
    CHECK(path == "/create.php?format=simple&url=https%3A%2F%2Fx.io%2F%C3%A7%C4%B1");
}

CRISP_TEST(ShortLink, TinyUrl_yolu_ve_yaniti) {
    CHECK(BuildTinyUrlPath(L"https://x.io/a?b=c") ==
          "/api-create.php?url=https%3A%2F%2Fx.io%2Fa%3Fb%3Dc");
    std::wstring out;
    CHECK(ParseShortLinkResponse("https://tinyurl.com/28ecpl9z", out));
    CHECK_STR(out, L"https://tinyurl.com/28ecpl9z");
    CHECK(!ParseShortLinkResponse("https://tinyurl.com/", out));
    CHECK(!ParseShortLinkResponse("Error", out));
}

CRISP_TEST(ShortLink, Yanit_kisa_baglanti) {
    std::wstring out;
    CHECK(ParseShortLinkResponse("https://is.gd/abcXYZ\n", out));
    CHECK_STR(out, L"https://is.gd/abcXYZ");

    CHECK(ParseShortLinkResponse("  https://is.gd/q1\r\n", out));
    CHECK_STR(out, L"https://is.gd/q1");

    CHECK(ParseShortLinkResponse("https://is.gd/noNewline", out));
    CHECK_STR(out, L"https://is.gd/noNewline");
}

CRISP_TEST(ShortLink, Yanit_hata_reddedilir) {
    std::wstring out = L"unchanged";
    CHECK(!ParseShortLinkResponse("Error: Please enter a valid URL to shorten", out));
    CHECK(!ParseShortLinkResponse("", out));
    CHECK(!ParseShortLinkResponse("\r\n", out));
    CHECK(!ParseShortLinkResponse("http://is.gd/abc", out));
    CHECK(!ParseShortLinkResponse("https://is.gd/", out));
    CHECK(!ParseShortLinkResponse("https://example.com/abc", out));
    CHECK(!ParseShortLinkResponse("https://is.gd/abc def", out));
    CHECK(!ParseShortLinkResponse("<html>https://is.gd/abc</html>", out));
    CHECK_STR(out, L"unchanged");
}
