#include "Triggerbot.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <chrono>

static void MouseDown() {
    SPOOF_FUNC;
    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    UINT(WINAPI * pSendInput)(UINT, LPINPUT, int) = &SendInput;
    SPOOF_CALL(pSendInput)(1, &inp, sizeof(INPUT));
}

static void MouseUp() {
    SPOOF_FUNC;
    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;

    UINT(WINAPI * pSendInput)(UINT, LPINPUT, int) = &SendInput;
    SPOOF_CALL(pSendInput)(1, &inp, sizeof(INPUT));
}

namespace Triggerbot {
    void Run(uintptr_t localPawn, uint8_t localTeam) {
        SPOOF_FUNC;

        static bool mouseHeld = false;
        static std::chrono::steady_clock::time_point delayStart;
        static bool delayActive = false;

        auto release = [&]() {
            if (mouseHeld) {
                MouseUp();
                mouseHeld = false;
            }
            delayActive = false;
            };

        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;
        if (!config.bTriggerbot || !(SPOOF_CALL(pGetAsyncKeyState)(config.triggerKey) & 0x8000)) {
            release();
            return;
        }

        int crosshairEntityId = mem.Read<int>(localPawn + offsets::csPawn::m_iIDEntIndex);
        if (crosshairEntityId <= 0) {
            release();
            return;
        }

        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) { release(); return; }

        uintptr_t listEntry = mem.Read<uintptr_t>(
            entityList + (8 * ((crosshairEntityId & 0x7FFF) >> 9) + 16)
        );
        if (!listEntry) { release(); return; }

        uintptr_t entity = mem.Read<uintptr_t>(listEntry + 112 * ((crosshairEntityId & 0x7FFF) & 0x1FF));
        if (!entity) { release(); return; }

        int health = mem.Read<int>(entity + offsets::entity::m_iHealth);
        uint8_t team = mem.Read<uint8_t>(entity + offsets::entity::m_iTeamNum);

        if (health <= 0 || team == localTeam) {
            release();
            return;
        }

        if (config.triggerDelay > 0) {
            if (!delayActive) {
                delayStart = std::chrono::steady_clock::now();
                delayActive = true;
                return;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - delayStart
            ).count();
            if (elapsed < config.triggerDelay) return;
        }

        if (!mouseHeld) {
            MouseDown();
            mouseHeld = true;
        }
    }
}