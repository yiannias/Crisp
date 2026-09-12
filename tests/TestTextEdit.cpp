// TestTextEdit.cpp — Metin aracının tamponu: ekleme, silme, hareket, seçim.
//
// VEKİL ÇİFTLER HER TESTİN İÇİNDE: emoji (U+1F600 = D83D DE00) yazıp geri
// silmek eskiden yarım kod birimi bırakıyordu; buradaki her hareket ve silme,
// imlecin bir çiftin ortasına düşmediğini de doğrular.
#include "TestFramework.h"

#include "TextEdit.h"

#include <string>

using namespace crisp;

namespace {

constexpr const wchar_t* kSmile = L"\xD83D\xDE00";

// İmleç ve çıpa hiçbir zaman bir vekil çiftin ortasında olmamalı.
[[nodiscard]] bool Intact(const TextEdit& edit) {
    for (const size_t pos : {edit.caret, edit.anchor}) {
        if (pos > edit.text.size()) {
            return false;
        }
        if (pos > 0 && pos < edit.text.size()) {
            const wchar_t before = edit.text[pos - 1];
            const wchar_t at = edit.text[pos];
            if (before >= 0xD800 && before <= 0xDBFF && at >= 0xDC00 &&
                at <= 0xDFFF) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

CRISP_TEST(TextEdit, Ekleme_imleci_ilerletir) {
    TextEdit edit;
    edit.InsertChar(L'a');
    edit.InsertChar(L'c');
    edit.MoveLeft(false);
    edit.InsertChar(L'b');
    CHECK_STR(edit.text, L"abc");
    CHECK_EQ(edit.caret, 2);
    CHECK_EQ(edit.anchor, 2);
    CHECK(!edit.HasSelection());
}

CRISP_TEST(TextEdit, Geri_silme_vekil_cifti_butun_siler) {
    TextEdit edit;
    edit.Insert(L"a");
    edit.Insert(kSmile);
    edit.Insert(L"b");
    CHECK_EQ(edit.text.size(), 4);
    edit.Backspace();
    CHECK_STR(edit.text, std::wstring(L"a") + kSmile);
    edit.Backspace();
    CHECK_STR(edit.text, L"a");
    CHECK_EQ(edit.caret, 1);
    edit.Backspace();
    edit.Backspace();   // boş tamponda zararsız
    CHECK_STR(edit.text, L"");
    CHECK_EQ(edit.caret, 0);
}

CRISP_TEST(TextEdit, Ileri_silme_vekil_cifti_butun_siler) {
    TextEdit edit;
    edit.Insert(std::wstring(kSmile) + L"x");
    edit.MoveTo(0, false);
    edit.Delete();
    CHECK_STR(edit.text, L"x");
    CHECK_EQ(edit.caret, 0);
    edit.Delete();
    edit.Delete();
    CHECK_STR(edit.text, L"");
    CHECK(Intact(edit));
}

CRISP_TEST(TextEdit, Sol_sag_vekil_cifti_tek_adimda_gecer) {
    TextEdit edit;
    edit.Insert(std::wstring(L"a") + kSmile + L"b");
    edit.MoveTo(0, false);
    edit.MoveRight(false);
    CHECK_EQ(edit.caret, 1);
    edit.MoveRight(false);
    CHECK_EQ(edit.caret, 3);   // çiftin üstünden atladı
    CHECK(Intact(edit));
    edit.MoveLeft(false);
    CHECK_EQ(edit.caret, 1);
    CHECK(Intact(edit));
}

CRISP_TEST(TextEdit, Dogrudan_yazilan_konum_kelepcelenir) {
    TextEdit edit;
    edit.Insert(std::wstring(L"a") + kSmile + L"b");
    edit.caret = 2;     // çiftin ortası
    edit.anchor = 99;   // tamponun dışı
    edit.MoveRight(true);
    CHECK(Intact(edit));
    CHECK_EQ(edit.anchor, 4);
    CHECK_EQ(edit.caret, 3);
    CHECK_EQ(edit.Snap(2), 1);
    CHECK_EQ(edit.Snap(50), 4);
}

CRISP_TEST(TextEdit, Ekleme_secimin_yerine_gecer) {
    TextEdit edit;
    edit.Insert(L"hello world");
    edit.MoveTo(0, false);
    edit.MoveWordRight(true);   // "hello " seçili
    CHECK_STR(edit.SelectedText(), L"hello ");
    edit.Insert(L"bye ");
    CHECK_STR(edit.text, L"bye world");
    CHECK_EQ(edit.caret, 4);
    CHECK(!edit.HasSelection());
}

CRISP_TEST(TextEdit, Secimli_geri_ve_ileri_silme_secimi_siler) {
    TextEdit edit;
    edit.Insert(L"abcdef");
    edit.MoveTo(1, false);
    edit.MoveTo(4, true);
    edit.Backspace();
    CHECK_STR(edit.text, L"aef");
    CHECK_EQ(edit.caret, 1);

    edit.MoveTo(3, false);
    edit.MoveTo(1, true);   // çıpa imlecin arkasında da olabilir
    edit.Delete();
    CHECK_STR(edit.text, L"a");
    CHECK_EQ(edit.caret, 1);
    CHECK_EQ(edit.anchor, 1);
}

CRISP_TEST(TextEdit, Home_ve_End_satira_gore) {
    TextEdit edit;
    edit.Insert(L"one\ntwo three\nfour");
    edit.MoveTo(6, false);   // "two" içinde
    edit.MoveHome(false);
    CHECK_EQ(edit.caret, 4);
    edit.MoveEnd(false);
    CHECK_EQ(edit.caret, 13);
    edit.MoveHome(true);
    CHECK_STR(edit.SelectedText(), L"two three");
    edit.MoveTo(edit.text.size(), false);
    edit.MoveEnd(false);
    CHECK_EQ(edit.caret, edit.text.size());
}

CRISP_TEST(TextEdit, Yukari_asagi_sutunu_korur_ve_kirpar) {
    TextEdit edit;
    edit.Insert(L"abcdef\nxy\nklmnop");
    edit.MoveTo(4, false);   // ilk satır, sütun 4
    edit.MoveDown(false);
    CHECK_EQ(edit.caret, 9);   // "xy" satırı kısa: satır sonunda kırpıldı
    edit.MoveDown(false);
    CHECK_EQ(edit.caret, 12);   // sütun 2 (kırpılan sütun taşınır)
    edit.MoveUp(false);
    CHECK_EQ(edit.caret, 9);
    edit.MoveUp(false);
    CHECK_EQ(edit.caret, 2);
    edit.MoveUp(false);   // ilk satırda: metnin başı
    CHECK_EQ(edit.caret, 0);
    edit.MoveTo(14, false);
    edit.MoveDown(false);   // son satırda: metnin sonu
    CHECK_EQ(edit.caret, edit.text.size());
}

CRISP_TEST(TextEdit, Yukari_asagi_vekil_cifti_bolmez) {
    TextEdit edit;
    edit.Insert(std::wstring(L"ab\n") + kSmile + L"cd");
    edit.MoveTo(2, false);   // ilk satır, sütun 2
    edit.MoveDown(false);
    CHECK(Intact(edit));
    CHECK_EQ(edit.caret, 5);   // 3 + 2 = çiftin tam sonu
    edit.MoveTo(1, false);
    edit.MoveDown(false);      // sütun 1 çiftin ortasına düşerdi
    CHECK(Intact(edit));
    CHECK_EQ(edit.caret, 3);
}

CRISP_TEST(TextEdit, Shift_ile_yukari_asagi_secer) {
    TextEdit edit;
    edit.Insert(L"ab\ncd\nef");
    edit.MoveTo(1, false);
    edit.MoveDown(true);
    CHECK_EQ(edit.anchor, 1);
    CHECK_EQ(edit.caret, 4);
    CHECK_STR(edit.SelectedText(), L"b\nc");
    edit.MoveUp(false);
    CHECK(!edit.HasSelection());
    CHECK_EQ(edit.caret, 1);
}

CRISP_TEST(TextEdit, Kelime_hareketleri) {
    TextEdit edit;
    edit.Insert(L"alpha  beta\ngamma");
    edit.MoveTo(0, false);
    edit.MoveWordRight(false);
    CHECK_EQ(edit.caret, 7);   // "beta" başı
    edit.MoveWordRight(false);
    CHECK_EQ(edit.caret, 12);   // satır sonu ayraç: "gamma" başı
    edit.MoveWordRight(false);
    CHECK_EQ(edit.caret, 17);
    edit.MoveWordRight(false);   // sonda kalır
    CHECK_EQ(edit.caret, 17);

    edit.MoveWordLeft(false);
    CHECK_EQ(edit.caret, 12);
    edit.MoveWordLeft(false);
    CHECK_EQ(edit.caret, 7);
    edit.MoveWordLeft(true);
    CHECK_EQ(edit.caret, 0);
    CHECK_STR(edit.SelectedText(), L"alpha  ");
}

CRISP_TEST(TextEdit, Tumunu_sec_ve_secili_metin) {
    TextEdit edit;
    edit.Insert(std::wstring(L"x\ny") + kSmile);
    edit.SelectAll();
    CHECK(edit.HasSelection());
    CHECK_EQ(edit.anchor, 0);
    CHECK_EQ(edit.caret, edit.text.size());
    CHECK_STR(edit.SelectedText(), std::wstring(L"x\ny") + kSmile);
    const auto [from, to] = edit.SelectionRange();
    CHECK_EQ(from, 0);
    CHECK_EQ(to, 5);
    edit.EraseSelection();
    CHECK_STR(edit.text, L"");
    CHECK_EQ(edit.caret, 0);
    edit.EraseSelection();   // seçim yokken hiçbir şey olmaz
    CHECK_STR(edit.text, L"");
}

CRISP_TEST(TextEdit, Secim_sirasi_yonden_bagimsiz) {
    TextEdit edit;
    edit.Insert(L"abcdef");
    edit.MoveTo(5, false);
    edit.MoveLeft(true);
    edit.MoveLeft(true);
    const auto [from, to] = edit.SelectionRange();
    CHECK_EQ(from, 3);
    CHECK_EQ(to, 5);
    CHECK_STR(edit.SelectedText(), L"de");
    // Shift'siz sol/sağ seçimi ilgili uca çökertir, bir adım daha gitmez.
    edit.MoveLeft(false);
    CHECK_EQ(edit.caret, 3);
    CHECK(!edit.HasSelection());
    edit.MoveTo(5, true);
    edit.MoveRight(false);
    CHECK_EQ(edit.caret, 5);
}

CRISP_TEST(TextEdit, Satir_ve_sutun_hesabi) {
    TextEdit edit;
    edit.Insert(L"ab\n\ncde");
    CHECK_EQ(edit.LineCount(), 3);
    CHECK_EQ(edit.LineOf(0), 0);
    CHECK_EQ(edit.LineOf(2), 0);   // '\n' hâlâ ilk satırın sonu
    CHECK_EQ(edit.LineOf(3), 1);
    CHECK_EQ(edit.LineOf(4), 2);
    CHECK_EQ(edit.LineOf(99), 2);
    CHECK_EQ(edit.LineStart(0), 0);
    CHECK_EQ(edit.LineStart(1), 3);
    CHECK_EQ(edit.LineStart(2), 4);
    CHECK_EQ(edit.LineStart(9), 4);   // olmayan satır: son satırın başı
    CHECK_EQ(edit.ColumnOf(6), 2);
    CHECK_EQ(edit.ColumnOf(3), 0);
    CHECK_EQ(edit.LineEnd(0), 2);
    CHECK_EQ(edit.LineEnd(4), 7);
}

CRISP_TEST(TextEdit, Pano_metni_duzeltilir) {
    CHECK_STR(TextEdit::Sanitize(L"a\r\nb\rc\nd"), L"a\nb\nc\nd");
    CHECK_STR(TextEdit::Sanitize(L"x\ty"), L"x    y");
    CHECK_STR(TextEdit::Sanitize(L"p\x01q\x1b"), L"pq");
    CHECK_STR(TextEdit::Sanitize(std::wstring(L"e") + kSmile),
              std::wstring(L"e") + kSmile);
}

CRISP_TEST(TextEdit, Temizleme_sifirlar) {
    TextEdit edit;
    edit.Insert(L"abc");
    edit.SelectAll();
    edit.Clear();
    CHECK_STR(edit.text, L"");
    CHECK_EQ(edit.caret, 0);
    CHECK_EQ(edit.anchor, 0);
    CHECK(!edit.HasSelection());
}
