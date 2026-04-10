#include "Aimbot.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <cmath>

namespace Aimbot {

    static bool IsVisible(uintptr_t targetPawn, int localEntityIndex) {
        // No spoof needed here – it only calls mem.Read (already spoofed inside Memory class)
        if (!targetPawn || localEntityIndex < 1 || localEntityIndex > 64) return false;

        uint64_t spottedMask = mem.Read<uint64_t>(targetPawn + offsets::csPawn::m_entitySpottedState + 0xC);
        return (spottedMask & (1ULL << localEntityIndex)) != 0;
    }

    void Run(const EntityData* entities, int count, uintptr_t localPawn, uintptr_t localController, int localEntityIndex, int screenWidth, int screenHeight) {
        SPOOF_FUNC;

        if (!config.bAimbot) return;

        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;
        if (!(SPOOF_CALL(pGetAsyncKeyState)(config.aimKey) & 0x8000)) return;

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
            if (shotsFired > 1) {
                punchAngle = mem.Read<Vector3>(localPawn + offsets::csPawn::m_aimPunchAngle);
            }
        }

        Vector3 visualAngles = viewAngles;
        visualAngles.x += punchAngle.x * 2.0f;
        visualAngles.y += punchAngle.y * 2.0f;
        NormalizeAngles(visualAngles);

        float bestFov = config.aimFov;
        Vector3 bestRawAngle = {};
        bool found = false;

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
                }
            }
        }

        if (found) {
            viewAngles = mem.Read<Vector3>(mem.client + offsets::client::dwViewAngles);

            Vector3 compensated = bestRawAngle;
            compensated.x -= punchAngle.x * 2.0f;
            compensated.y -= punchAngle.y * 2.0f;
            NormalizeAngles(compensated);

            Vector3 delta = compensated - viewAngles;
            NormalizeAngles(delta);
            float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);

            Vector3 finalAngle;
            if (config.aimSmooth <= 1.0f || dist < 0.15f) {
                finalAngle = compensated;
            }
            else {
                finalAngle = SmoothAngle(viewAngles, compensated, config.aimSmooth);
            }
            NormalizeAngles(finalAngle);
            mem.Write<Vector3>(mem.client + offsets::client::dwViewAngles, finalAngle);
        }
    }
}