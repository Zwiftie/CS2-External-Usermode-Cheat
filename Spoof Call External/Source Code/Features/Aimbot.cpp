#include "Aimbot.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <cmath>
#include <random>
#include <chrono>

namespace Aimbot {

    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    // Persistent per-target state
    static uintptr_t s_lastTarget = 0;
    static float s_jitterX = 0.f, s_jitterY = 0.f;
    static float s_imperfectX = 0.f, s_imperfectY = 0.f;
    static auto s_lastUpdate = std::chrono::steady_clock::now();

    static bool IsVisible(uintptr_t targetPawn, int localEntityIndex) {
        if (!targetPawn || localEntityIndex < 0 || localEntityIndex > 63)
            return false;
        uint32_t spottedMask = mem.Read<uint32_t>(
            targetPawn + offsets::csPawn::m_entitySpottedState +
            offsets::csPawn::m_bSpottedByMask);
        if ((spottedMask & (1u << localEntityIndex)) != 0)
            return true;
        if (localEntityIndex > 0 && (spottedMask & (1u << (localEntityIndex - 1))) != 0)
            return true;
        return false;
    }

    void Run(const EntityData* entities, int count, uintptr_t localPawn, uintptr_t localController, int localEntityIndex, int screenWidth, int screenHeight) {
        SPOOF_FUNC;

        if (!config.bAimbot) return;

        // ---- Aim mode handling ----
        static bool toggleState = false;
        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;

        if (config.aimMode == 1) { // Hold mode
            if (!(SPOOF_CALL(pGetAsyncKeyState)(config.aimKey) & 0x8000))
                return;
        }
        else if (config.aimMode == 2) { // Toggle mode
            static bool lastKeyState = false;
            bool currentKeyState = (SPOOF_CALL(pGetAsyncKeyState)(config.aimKey) & 0x8000) != 0;
            if (currentKeyState && !lastKeyState)
                toggleState = !toggleState;
            lastKeyState = currentKeyState;
            if (!toggleState)
                return;
        }

        uintptr_t sceneNode = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
        if (!sceneNode) return;

        Vector3 localOrigin = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
        Vector3 viewOffset = mem.Read<Vector3>(localPawn + offsets::model::m_vecViewOffset);
        if (viewOffset.z < 1.0f || viewOffset.z > 100.0f)
            viewOffset = { 0.0f, 0.0f, 64.06f };
        Vector3 eyePos = localOrigin + viewOffset;
        Vector3 viewAngles = mem.Read<Vector3>(mem.client + offsets::client::dwViewAngles);

        Vector3 punchAngle = {};
        if (config.bRcs) {
            int shotsFired = mem.Read<int>(localPawn + offsets::csPawn::m_iShotsFired);
            if (shotsFired > 1)
                punchAngle = mem.Read<Vector3>(localPawn + offsets::csPawn::m_aimPunchAngle);
        }

        Vector3 visualAngles = viewAngles;
        visualAngles.x += punchAngle.x * 2.0f;
        visualAngles.y += punchAngle.y * 2.0f;
        NormalizeAngles(visualAngles);

        float bestFov = config.aimFov;
        Vector3 bestRawAngle = {};
        bool found = false;
        uintptr_t currentTarget = 0;

        for (int i = 0; i < count; i++) {
            const EntityData& ent = entities[i];
            if (!ent.valid || !ent.bonesValid) continue;

            for (int b = 0; b < BoneIndex::BONE_COUNT; b++) {
                if (!config.aimBones[b]) continue;
                Vector3 targetPos = ent.bones[b];
                if (targetPos.IsZero()) continue;

                if (config.bVisibleCheck) {
                    if (!IsVisible(ent.pawn, localEntityIndex)) continue;
                }

                Vector3 aimAngle = CalculateAngle(eyePos, targetPos);
                NormalizeAngles(aimAngle);

                float fov = GetFov(visualAngles, aimAngle);
                if (fov < bestFov) {
                    bestFov = fov;
                    bestRawAngle = aimAngle;
                    found = true;
                    currentTarget = ent.pawn;
                }
            }
        }

        if (!found) return;

        viewAngles = mem.Read<Vector3>(mem.client + offsets::client::dwViewAngles);
        Vector3 compensated = bestRawAngle;
        compensated.x -= punchAngle.x * 2.0f;
        compensated.y -= punchAngle.y * 2.0f;
        NormalizeAngles(compensated);

        // ---- Periodic updates for jitter and imperfect offsets (every 250ms) ----
        auto now = std::chrono::steady_clock::now();
        bool targetChanged = (currentTarget != s_lastTarget);
        bool timeToUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastUpdate).count() > 250;

        if (targetChanged || timeToUpdate) {
            s_lastUpdate = now;
            s_lastTarget = currentTarget;

            // Imperfect angles: random offset applied to the compensated angle
            if (config.bImperfectAngles && config.imperfectAngleMax > 0.0f) {
                std::uniform_real_distribution<float> sign(-1.f, 1.f);
                float range = config.imperfectAngleMax;
                float minVal = config.imperfectAngleMin;
                std::uniform_real_distribution<float> mag(minVal, range);
                s_imperfectX = (sign(rng) >= 0.f ? 1.f : -1.f) * mag(rng);
                s_imperfectY = (sign(rng) >= 0.f ? 1.f : -1.f) * mag(rng);
            }
            else {
                s_imperfectX = s_imperfectY = 0.f;
            }

            // Jitter: small, slow variation
            if (config.bAimJitter && config.aimJitterMax > 0.0f) {
                std::uniform_real_distribution<float> sign(-1.f, 1.f);
                float range = config.aimJitterMax;
                float minVal = config.aimJitterMin;
                std::uniform_real_distribution<float> mag(minVal, range);
                s_jitterX = (sign(rng) >= 0.f ? 1.f : -1.f) * mag(rng);
                s_jitterY = (sign(rng) >= 0.f ? 1.f : -1.f) * mag(rng);
            }
            else {
                s_jitterX = s_jitterY = 0.f;
            }
        }

        // Apply the offsets (both are zero if features disabled)
        compensated.x += s_imperfectX + s_jitterX;
        compensated.y += s_imperfectY + s_jitterY;
        NormalizeAngles(compensated);

        // ---- Randomized smooth (only when target changes) ----
        float smoothValue = config.aimSmooth;
        if (config.bRandomizedSmooth && config.randomSmoothMax > config.randomSmoothMin) {
            static float s_storedSmooth = config.aimSmooth;
            if (targetChanged) {
                std::uniform_real_distribution<float> smoothDist(config.randomSmoothMin, config.randomSmoothMax);
                s_storedSmooth = smoothDist(rng);
            }
            smoothValue = s_storedSmooth;
        }

        Vector3 delta = compensated - viewAngles;
        NormalizeAngles(delta);
        float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        Vector3 finalAngle;
        if (smoothValue <= 1.0f || dist < 0.15f) {
            finalAngle = compensated;
        }
        else {
            finalAngle = SmoothAngle(viewAngles, compensated, smoothValue);
        }
        NormalizeAngles(finalAngle);

        mem.Write<Vector3>(mem.client + offsets::client::dwViewAngles, finalAngle);
    }
}