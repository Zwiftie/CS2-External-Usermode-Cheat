#include "Glow.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include <algorithm>

namespace Glow {
    void Run(uintptr_t localController) {
        if (!config.bGlow) return;
        if (!localController) return;

        uint8_t localTeam = mem.Read<uint8_t>(localController + offsets::entity::m_iTeamNum);
        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) return;

        for (int i = 1; i < 64; ++i) {
            uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9)) + 16);
            if (!listEntry) continue;

            uintptr_t player = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!player) continue;

            uint8_t playerTeam = mem.Read<uint8_t>(player + offsets::entity::m_iTeamNum);
            if (playerTeam == localTeam && !config.bGlowShowTeam) continue;

            uint32_t playerPawn = mem.Read<uint32_t>(player + offsets::controller::m_hPlayerPawn);
            if (!playerPawn) continue;

            uintptr_t listEntry2 = mem.Read<uintptr_t>(entityList + 8 * ((playerPawn & 0x7FFF) >> 9) + 16);
            if (!listEntry2) continue;

            uintptr_t playerCsPawn = mem.Read<uintptr_t>(listEntry2 + 112 * (playerPawn & 0x1FF));
            if (!playerCsPawn) continue;

            int health = mem.Read<int>(playerCsPawn + offsets::entity::m_iHealth);
            if (health < 1) continue;

            float r, g, b, a;

            if (config.bGlowHealthBased) {
                float healthPercent = std::clamp(health / 100.0f, 0.0f, 1.0f);
                r = 1.0f - healthPercent;
                g = healthPercent;
                b = 0.0f;
                a = 1.0f;
            }
            else if (config.bGlowTeamBased) {
                if (playerTeam == localTeam) {
                    r = config.glowTeamColor[0];
                    g = config.glowTeamColor[1];
                    b = config.glowTeamColor[2];
                    a = config.glowTeamColor[3];
                }
                else {
                    r = config.glowEnemyColor[0];
                    g = config.glowEnemyColor[1];
                    b = config.glowEnemyColor[2];
                    a = config.glowEnemyColor[3];
                }
            }
            else {
                r = config.glowColor[0];
                g = config.glowColor[1];
                b = config.glowColor[2];
                a = config.glowColor[3];
            }

            DWORD colorArgb = (
                (static_cast<DWORD>(a * 255) << 24) |
                (static_cast<DWORD>(b * 255) << 16) |
                (static_cast<DWORD>(g * 255) << 8) |
                static_cast<DWORD>(r * 255)
                );

            uintptr_t glowOffset = playerCsPawn + offsets::model::m_Glow;
            mem.Write<DWORD>(glowOffset + glow::m_glowColorOverride, colorArgb);
            mem.Write<bool>(glowOffset + glow::m_bGlowing, true);
        }
    }
}