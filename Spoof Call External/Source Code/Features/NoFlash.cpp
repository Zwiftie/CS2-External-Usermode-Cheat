#include "NoFlash.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"

namespace NoFlash {
    void Run(uintptr_t localPawn) {
        if (!config.bNoFlash) return;

        float flashAlpha = mem.Read<float>(localPawn + offsets::csPawnBase::m_flFlashMaxAlpha);
        if (flashAlpha > 0.0f) {
            mem.Write<float>(localPawn + offsets::csPawnBase::m_flFlashMaxAlpha, 0.0f);
        }
    }
}