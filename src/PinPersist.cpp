// PinPersist.cpp — Açık iğnelerin diske yazılması ve geri yüklenmesi.
//
// AYRI DOSYA: PinWindow.cpp pencerenin davranışını (sürükleme, tekerlek,
// menü) anlatıyor ve 385 satırdı; ikisi bir arada ev kuralının 400 satır
// sınırını aşardı (docs §9).
//
// GÖRÜNTÜLER PNG OLARAK YAZILIR, kayıt defterine ya da tek bir ikili dosyaya
// değil. Kullanıcı klasörü açıp ne saklandığını görebilir, bir iğneyi elle
// silebilir, ya da klasörün tamamını silip her şeyi unutturabilir — aynı
// gerekçe geçmiş klasörü için de yazılı.
#include "PinWindow.h"

#include "ImageCodec.h"
#include "PinInternal.h"
#include "PinStore.h"
#include "Util.h"

#include <shlobj.h>

#include <cstdio>
#include <string>
#include <vector>

namespace crisp {
namespace {

// Açık iğneleri diske yazılacak kayıtlara çevirir; görüntüleri de yazar.
[[nodiscard]] std::vector<PinRecord> CollectOpenPins() {
    std::vector<PinRecord> records;
    const std::wstring folder = PinFolder();
    if (folder.empty()) {
        return records;
    }
    ::SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);

    size_t index = 0;
    for (const auto& entry : pin::Pins()) {
        if (entry == nullptr || entry->window == nullptr || !entry->image.Valid()) {
            continue;
        }

        // KONUM PENCEREDEN OKUNUR, saklanan bir alandan değil: kullanıcı iğneyi
        // sürükleyerek taşıyor ve o hareket hiçbir yere yazılmıyor. Görünürlük
        // de öyle: gizle/göster yalnızca ShowWindow çağırır.
        RECT bounds{};
        if (::GetWindowRect(entry->window, &bounds) == FALSE) {
            continue;
        }

        wchar_t name[32] = {};
        ::swprintf_s(name, L"pin-%02zu.png", index);
        const std::wstring path = folder + L"\\" + name;
        if (!SavePng(entry->image, path)) {
            LogV(L"İğne görüntüsü yazılamadı: %s", path.c_str());
            continue;
        }

        PinRecord record;
        record.imageFile = name;
        record.x = bounds.left;
        record.y = bounds.top;
        record.zoom = entry->zoom;
        record.opacity = entry->opacity;
        record.topMost = entry->topMost;
        record.frame = entry->frame;
        record.clickThrough = entry->clickThrough;
        record.hidden = ::IsWindowVisible(entry->window) == FALSE;
        records.push_back(std::move(record));
        ++index;
    }
    return records;
}

}  // namespace

void SaveOpenPins() {
    // ESKİSİ ÖNCE SİLİNİR, YENİSİ SONRA YAZILIR — VE SIRA BU YÜZDEN ÖNEMLİ.
    //
    // Kullanıcı üç iğneden ikisini kapattıysa, kalan tek iğneyi yazmak yetmez:
    // eski iki PNG orada durur ve klasör her oturumda büyür. Ama temizlik
    // `CollectOpenPins`ten SONRA çalıştırılırsa — ki bir süre öyleydi — az önce
    // yazılmış görüntüleri siler ve geriye yalnızca hiçbir dosyayı işaret
    // etmeyen bir indeks kalır.
    (void)ClearPinStore();

    const std::vector<PinRecord> records = CollectOpenPins();
    if (records.empty()) {
        return;
    }
    if (!WritePinIndex(records)) {
        LogV(L"İğneler kaydedilemedi");
    }
}

int RestorePins(HINSTANCE instance) {
    const std::vector<PinRecord> records = ReadPinIndex();
    if (records.empty()) {
        return 0;
    }

    const std::wstring folder = PinFolder();
    int restored = 0;
    for (const PinRecord& record : records) {
        Image image;
        const std::wstring path = folder + L"\\" + record.imageFile;
        if (!LoadImageFile(path, image) || !image.Valid()) {
            LogV(L"İğne geri yüklenemedi: %s", record.imageFile.c_str());
            continue;
        }
        // Kayıt görünüme birebir aktarılır; gizli bırakılmış bir iğne gizli
        // geri gelir ve tepsiden "göster" ile ortaya çıkar.
        PinView view;
        view.zoom = record.zoom;
        view.opacity = record.opacity;
        view.topMost = record.topMost;
        view.frame = record.frame;
        view.clickThrough = record.clickThrough;
        view.hidden = record.hidden;
        if (PinImageWithView(instance, image, POINT{record.x, record.y}, view)) {
            ++restored;
        }
    }

    // GERİ YÜKLENENLER DİSKTE KALIR. Uygulama bir daha kapandığında
    // `SaveOpenPins` her şeyi yeniden yazacak; burada silmek, açılıştan hemen
    // sonra çöken bir sürümde iğnelerin kaybolması demek olurdu.
    return restored;
}

}  // namespace crisp
