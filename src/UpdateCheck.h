// UpdateCheck.h — Sürüm karşılaştırma ve GitHub Releases yanıtının okunması.
// TASLAK.
#pragma once

#include <string>

namespace crisp {

// Semantik sürüm karşılaştırması: "0.9.0" ile "0.10.1" gibi. Negatif: a < b.
[[nodiscard]] int CompareVersions(const std::wstring& a, const std::wstring& b) noexcept;

}  // namespace crisp
