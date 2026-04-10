#pragma once
#include <cstdint>
#include "SDK.h"   // for Vector3 etc. if needed

namespace Misc {
    void Bhop(uintptr_t localPawn);
    void NoFlash(uintptr_t localPawn);
    void Triggerbot(uintptr_t localPawn, uint8_t localTeam);
}

namespace Glow {
    void Run(uintptr_t localController);
}