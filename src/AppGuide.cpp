// AppGuide.cpp — Adım adım kılavuz: ardışık yakalamaları numaralı tek bir
// görüntüde toplar.
//
// TASLAK: gövdeler kılavuz özelliğiyle birlikte dolacak.
#include "App.h"

namespace crisp {

void App::GuideAddStep(bool /*preferWindowPick*/) {}
void App::GuideFinish() {}
void App::GuideDiscard() { m_guideSteps.clear(); }

}  // namespace crisp
