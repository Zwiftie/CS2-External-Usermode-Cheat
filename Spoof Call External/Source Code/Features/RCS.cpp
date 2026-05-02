#include "RCS.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <cmath>

namespace RCS {

    static Vector3 oldAimPunch = { 0.0f, 0.0f, 0.0f };

    void Run() {
        SPOOF_FUNC;

        if (!config.bRcs) {
            oldAimPunch = { 0.0f, 0.0f, 0.0f };
            return;
        }

        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            oldAimPunch = { 0.0f, 0.0f, 0.0f };
            return;
        }

        uintptr_t localPawn = mem.Read<uintptr_t>(
            mem.client + offsets::client::dwLocalPlayerPawn
        );

        if (!localPawn) {
            oldAimPunch = { 0.0f, 0.0f, 0.0f };
            return;
        }

        int shotsFired = mem.Read<int>(
            localPawn + offsets::csPawn::m_iShotsFired
        );

        if (shotsFired < 2) {
            oldAimPunch = { 0.0f, 0.0f, 0.0f };
            return;
        }

        uintptr_t aimPunchServices = mem.Read<uintptr_t>(
            localPawn + offsets::csPawn::m_pAimPunchServices
        );

        if (!aimPunchServices) {
            oldAimPunch = { 0.0f, 0.0f, 0.0f };
            return;
        }

        Vector3 currentAimPunch = mem.Read<Vector3>(
            aimPunchServices +
            offsets::CCSPlayer_AimPunchServices::m_aimPunchAngle
        );

        Vector3 punchDelta = {
            currentAimPunch.x - oldAimPunch.x,
            currentAimPunch.y - oldAimPunch.y,
            currentAimPunch.z - oldAimPunch.z
        };

        oldAimPunch = currentAimPunch;

        if (punchDelta.x == 0.0f && punchDelta.y == 0.0f)
            return;

        Vector3 viewAngles = mem.Read<Vector3>(
            mem.client + offsets::client::dwViewAngles
        );

        float strength = config.rcsStrength;

        viewAngles.x -= punchDelta.x * strength;
        viewAngles.y -= punchDelta.y * strength;

        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;

        while (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        while (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        mem.Write<Vector3>(
            mem.client + offsets::client::dwViewAngles,
            viewAngles
        );
    }
}