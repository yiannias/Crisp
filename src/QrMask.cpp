// QrMask.cpp — Sekiz maske deseni ve dört ceza kuralı.
//
// MASKE NEDEN VAR: veri bitleri olduğu gibi yerleştirildiğinde büyük tek
// renkli alanlar ya da bulucu desenine benzeyen çizgiler oluşabiliyor ve
// tarayıcı kodu bulamıyor. Standart sekiz deseni sırayla dener, her sonucu
// dört kuralla puanlar ve en düşük cezalıyı seçer. Cezanın kendisi
// "yanlış" olamaz — her maske geçerli bir koddur — ama kuralları doğru
// uygulamak, okunması en kolay kodu vermek demek.
#include "QrInternal.h"

#include <cstdlib>

namespace crisp {
namespace qr {
namespace {

constexpr int kPenaltyN1 = 3;
constexpr int kPenaltyN2 = 3;
constexpr int kPenaltyN3 = 40;
constexpr int kPenaltyN4 = 10;

// Kural 1: satır ya da sütunda beş ve daha uzun tek renk koşusu.
// Koşu 5 modülse 3, her fazla modül için +1.
[[nodiscard]] int RunPenalty(const QrCode& code) {
    int penalty = 0;
    for (int y = 0; y < code.size; ++y) {
        int runColor = -1;
        int run = 0;
        for (int x = 0; x <= code.size; ++x) {
            const int color = x < code.size ? (code.Module(x, y) ? 1 : 0) : -2;
            if (color == runColor) {
                ++run;
                continue;
            }
            if (run >= 5) {
                penalty += kPenaltyN1 + (run - 5);
            }
            runColor = color;
            run = 1;
        }
    }
    for (int x = 0; x < code.size; ++x) {
        int runColor = -1;
        int run = 0;
        for (int y = 0; y <= code.size; ++y) {
            const int color = y < code.size ? (code.Module(x, y) ? 1 : 0) : -2;
            if (color == runColor) {
                ++run;
                continue;
            }
            if (run >= 5) {
                penalty += kPenaltyN1 + (run - 5);
            }
            runColor = color;
            run = 1;
        }
    }
    return penalty;
}

// Kural 2: tek renkli 2x2 blok başına 3. Büyük bir kare, örtüşen her 2x2
// için ayrı ayrı sayılır — standart bunu açıkça öyle tanımlıyor.
[[nodiscard]] int BlockPenalty(const QrCode& code) {
    int penalty = 0;
    for (int y = 0; y + 1 < code.size; ++y) {
        for (int x = 0; x + 1 < code.size; ++x) {
            const bool a = code.Module(x, y);
            if (a == code.Module(x + 1, y) && a == code.Module(x, y + 1) &&
                a == code.Module(x + 1, y + 1)) {
                penalty += kPenaltyN2;
            }
        }
    }
    return penalty;
}

// Kural 3: 1011101 deseni, bir yanında dört açık modülle. Bulucu desenin
// oranı bu; veri alanında tesadüfen oluşursa tarayıcı yanlış köşeyi bulur.
// `horizontal` false ise (x, y) yer değiştirir ve aynı kod sütunları tarar.
[[nodiscard]] bool At(const QrCode& code, int a, int b, bool horizontal) noexcept {
    return horizontal ? code.Module(a, b) : code.Module(b, a);
}

[[nodiscard]] int FinderLikePenalty(const QrCode& code) {
    constexpr bool kPattern[7] = {true, false, true, true, true, false, true};
    int penalty = 0;
    for (int pass = 0; pass < 2; ++pass) {
        const bool horizontal = pass == 0;
        for (int line = 0; line < code.size; ++line) {
            for (int start = 0; start + 7 <= code.size; ++start) {
                bool match = true;
                for (int i = 0; i < 7 && match; ++i) {
                    match = At(code, start + i, line, horizontal) == kPattern[i];
                }
                if (!match) {
                    continue;
                }
                bool lightBefore = start >= 4;
                for (int i = 1; i <= 4 && lightBefore; ++i) {
                    lightBefore = !At(code, start - i, line, horizontal);
                }
                bool lightAfter = start + 11 <= code.size;
                for (int i = 7; i < 11 && lightAfter; ++i) {
                    lightAfter = !At(code, start + i, line, horizontal);
                }
                if (lightBefore) {
                    penalty += kPenaltyN3;
                }
                if (lightAfter) {
                    penalty += kPenaltyN3;
                }
            }
        }
    }
    return penalty;
}

// Kural 4: koyu modül oranının %50'den sapması, %5'lik adımlarla. Oranın
// içine düştüğü iki katın 50'ye yakın olanı alınır; adım başına 10.
[[nodiscard]] int BalancePenalty(const QrCode& code) {
    const int total = code.size * code.size;
    int dark = 0;
    for (const uint8_t module : code.modules) {
        dark += module != 0 ? 1 : 0;
    }
    const int percent = dark * 100 / total;
    const int lower = percent / 5 * 5;
    const int upper = lower + 5;
    const int deviation = std::abs(lower - 50) < std::abs(upper - 50)
                              ? std::abs(lower - 50)
                              : std::abs(upper - 50);
    return deviation / 5 * kPenaltyN4;
}

}  // namespace

bool MaskBit(int mask, int x, int y) noexcept {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return (x * y) % 2 + (x * y) % 3 == 0;
        case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
        case 7: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
        default: return false;
    }
}

void ApplyMask(QrCode& code, const std::vector<uint8_t>& function, int mask) {
    if (function.size() != code.modules.size()) {
        return;
    }
    for (int y = 0; y < code.size; ++y) {
        for (int x = 0; x < code.size; ++x) {
            const size_t at = static_cast<size_t>(y) * static_cast<size_t>(code.size) +
                              static_cast<size_t>(x);
            if (function[at] == 0 && MaskBit(mask, x, y)) {
                code.modules[at] ^= 1;
            }
        }
    }
}

int PenaltyScore(const QrCode& code) {
    if (code.size <= 0 || code.modules.size() != static_cast<size_t>(code.size) *
                                                     static_cast<size_t>(code.size)) {
        return 0;
    }
    return RunPenalty(code) + BlockPenalty(code) + FinderLikePenalty(code) +
           BalancePenalty(code);
}

}  // namespace qr
}  // namespace crisp
