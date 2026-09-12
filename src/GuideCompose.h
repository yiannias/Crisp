// GuideCompose.h — Adım görüntülerini numaralı tek bir görüntüde birleştirir.
// TASLAK.
#pragma once

#include "Capture.h"

#include <vector>

namespace crisp {

[[nodiscard]] bool ComposeGuide(const std::vector<Image>& steps, Image& out);

}  // namespace crisp
