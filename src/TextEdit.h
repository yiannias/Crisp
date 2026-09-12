// TextEdit.h — Düzenleyicinin metin aracı için imleçli, seçimli metin tamponu.
//
// NEDEN ÇEKİRDEKTE: metin aracı yalnızca SONA EKLİYORDU. Bir harfi yanlış
// yazan kullanıcı, ondan sonraki her şeyi silip yeniden yazmak zorundaydı;
// ok tuşları, Home/End ve seçim yoktu. Bu mantığın tamamı pencere gerektirmez
// ve pencere kodunun içinde sınanamazdı — burada ise her tuşun etkisi bir
// testle sabitlenir.
//
// DEĞİŞMEZLER (her üye işlevin girişinde ve çıkışında geçerlidir):
//   * `caret` ve `anchor` daima 0..text.size() aralığındadır.
//   * İkisi de bir vekil çiftin (surrogate pair) ORTASINA düşmez: bir emoji
//     UTF-16'da iki kod birimidir ve ortasına imleç koymak, sonraki silme ya
//     da eklemede yarım bir kod birimi bırakırdı. Her hareket bunu gözetir.
//   * Seçim `anchor` ile `caret` arasındaki aralıktır; ikisi eşitse seçim
//     yoktur. Hangisinin önde olduğu önemsizdir, SelectionRange sıralar.
//   * Satır ayracı yalnızca L'\n'; "\r\n" tampona girmeden önce düzeltilir.
//
// Alanlar bilerek açık: düzenleyici imleci fareyle yerleştirirken doğrudan
// yazar. Her üye işlev girişte alanları yeniden kelepçeler, dolayısıyla
// dışarıdan yazılan tutarsız bir değer tamponu bozamaz.
#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace crisp {

struct TextEdit {
    std::wstring text;
    size_t caret = 0;
    size_t anchor = 0;

    [[nodiscard]] bool HasSelection() const noexcept;
    // (başlangıç, bitiş) sıralı; seçim yoksa ikisi de caret'tir.
    [[nodiscard]] std::pair<size_t, size_t> SelectionRange() const noexcept;
    [[nodiscard]] std::wstring SelectedText() const;

    // Seçim varsa önce onu siler, sonra imlecin yerine ekler.
    void Insert(std::wstring_view piece);
    void InsertChar(wchar_t ch);
    // Seçim varsa onu, yoksa imleçten önceki/sonraki TEK KOD NOKTASINI siler.
    void Backspace();
    void Delete();
    void EraseSelection();
    void SelectAll() noexcept;
    void Clear() noexcept;

    // `extend` true ise anchor yerinde kalır (Shift basılı); değilse seçim
    // imlecin yeni yerinde çöker.
    void MoveLeft(bool extend) noexcept;
    void MoveRight(bool extend) noexcept;
    void MoveHome(bool extend) noexcept;   // satır başı
    void MoveEnd(bool extend) noexcept;    // satır sonu
    void MoveUp(bool extend) noexcept;     // sütunu korur, kısa satırda kırpar
    void MoveDown(bool extend) noexcept;
    void MoveWordLeft(bool extend) noexcept;
    void MoveWordRight(bool extend) noexcept;
    // İmleci verilen konuma taşır; fare tıklaması bunu kullanır.
    void MoveTo(size_t pos, bool extend) noexcept;

    // Satır/sütun hesabı. Sütun kod birimi sayısıdır, piksel değil; piksele
    // çevirmek yazı tipini bilen tarafın işidir.
    [[nodiscard]] int LineOf(size_t pos) const noexcept;
    [[nodiscard]] size_t LineStart(int line) const noexcept;
    [[nodiscard]] size_t LineEnd(size_t pos) const noexcept;
    [[nodiscard]] size_t ColumnOf(size_t pos) const noexcept;
    [[nodiscard]] int LineCount() const noexcept;

    // Konumu tamponun içine kelepçeler ve vekil çiftin ortasındaysa başına
    // çeker. Dışarıdan hesaplanan her konum buradan geçmeli.
    [[nodiscard]] size_t Snap(size_t pos) const noexcept;
    // Bir kod noktası geri/ileri; vekil çifti tek adımda geçer. Piksel
    // ölçen taraf da satırı bu adımlarla yürür, yoksa yarım emoji ölçerdi.
    [[nodiscard]] size_t PreviousBoundary(size_t pos) const noexcept;
    [[nodiscard]] size_t NextBoundary(size_t pos) const noexcept;

    // Panodan ya da başka yerden gelen ham metni tampona uygun hâle getirir:
    // "\r\n" ve yalnız '\r' → '\n', sekme → dört boşluk, diğer denetim
    // karakterleri düşer. Çizim tarafı sekmeyi genişletmez ve '\r' görünmez
    // bir kod birimi olarak imleç hesabını kaydırırdı.
    [[nodiscard]] static std::wstring Sanitize(std::wstring_view raw);

private:
    void Normalize() noexcept;
    void Place(size_t pos, bool extend) noexcept;
};

}  // namespace crisp
