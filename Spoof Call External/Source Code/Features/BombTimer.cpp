#include "BombTimer.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "imgui.h"
#include <cmath>
#include <chrono>

namespace BombTimer {

    static float g_globalTime = 0.0f;
    static uintptr_t g_lastPlanted = 0;
    static float g_cachedBlowTime = 0.0f;
    static float g_localRemaining = 0.0f;
    static std::chrono::steady_clock::time_point g_lastUpdate = std::chrono::steady_clock::now();
    static int g_plantedStableCount = 0;
    static int g_unplantedStableCount = 0;
    static bool g_wasEnabled = false;
    static int g_validRemainingCount = 0;

    // Get global game time (from dwGlobalVars)
    static float GetGlobalTime() {
        // dwGlobalVars is typically a pointer stored at client + dwGlobalVars
        uintptr_t globals = mem.Read<uintptr_t>(mem.client + offsets::client::dwGlobalVars);
        if (!globals) return 0.0f;
        float curtime = mem.Read<float>(globals + 0x8); // curtime offset
        if (!std::isfinite(curtime) || curtime <= 0.0f) return 0.0f;
        return curtime;
    }

    bool IsPlanted() {
        uintptr_t plantedC4 = mem.Read<uintptr_t>(mem.client + offsets::client::dwPlantedC4);
        if (!plantedC4) return false;
        // Optional: check if bomb is alive/active
        int health = mem.Read<int>(plantedC4 + offsets::entity::m_iHealth);
        return health > 0;
    }

    float GetRemainingTime() {
        // Keep this simple: this function tries to compute remaining from game memory.
        // The Render() function contains more robust logic (caching + fallback) so this
        // may return 0 if game reads are invalid.
        uintptr_t plantedC4 = mem.Read<uintptr_t>(mem.client + offsets::client::dwPlantedC4);
        if (!plantedC4) return 0.0f;

        float blowTime = mem.Read<float>(plantedC4 + offsets::csPawn::m_flC4Blow);
        float curTime = GetGlobalTime();

        if (!std::isfinite(blowTime) || !std::isfinite(curTime)) return 0.0f;
        if (blowTime <= 0.0f || curTime <= 0.0f) return 0.0f;

        float remaining = blowTime - curTime;
        if (remaining <= 0.0f) return 0.0f;
        if (remaining > 120.0f) return 0.0f;
        return remaining;
    }

    void Render() {
        if (!config.bBombTimer) {
            // ensure we reset initialization state so toggling the feature on reinitializes timers
            g_wasEnabled = false;
            return;
        }

        static float posX = 100, posY = 100;
        static bool dragging = false;

        // Read planted pointer each frame
        auto now = std::chrono::steady_clock::now();

        // Reset internal state when the feature is first enabled to avoid leftover values
        if (!g_wasEnabled) {
            g_lastPlanted = 0;
            g_cachedBlowTime = 0.0f;
            g_localRemaining = 0.0f;
            g_plantedStableCount = 0;
            g_unplantedStableCount = 0;
            g_lastUpdate = now;
            g_wasEnabled = true;
        }

        uintptr_t plantedC4 = mem.Read<uintptr_t>(mem.client + offsets::client::dwPlantedC4);
        // planted C4 doesn't use the same entity health offsets; detect planting by checking blow time
        float blowTimePeek = 0.0f;
        if (plantedC4) blowTimePeek = mem.Read<float>(plantedC4 + offsets::csPawn::m_flC4Blow);
        bool plantedFlag = (plantedC4 != 0) && std::isfinite(blowTimePeek) && (blowTimePeek > 0.0f);

        // Update stability counters to avoid reacting to flickering reads
        const int STABLE_THRESHOLD = 3; // frames
        if (plantedFlag) {
            g_plantedStableCount++;
            g_unplantedStableCount = 0;
        }
        else {
            g_unplantedStableCount++;
            g_plantedStableCount = 0;
        }

        // We'll maintain a local countdown (g_localRemaining) updated by steady clock every frame.
        float remaining = 0.0f;

        if (plantedFlag && g_plantedStableCount >= STABLE_THRESHOLD) {
            // Transition from no-bomb to planted: initialize localRemaining from game time if possible
            if (g_lastPlanted == 0) {
                // Only initialize if the game reports a stable, sane remaining time
                float gameRemaining = GetRemainingTime();
                if (gameRemaining >= 3.0f && gameRemaining <= 300.0f) {
                    g_validRemainingCount++;
                }
                else {
                    g_validRemainingCount = 0;
                }

                // require the remaining to be stable for a few frames
                if (g_validRemainingCount >= STABLE_THRESHOLD) {
                    g_localRemaining = gameRemaining;
                    float blowTime = mem.Read<float>(plantedC4 + offsets::csPawn::m_flC4Blow);
                    g_cachedBlowTime = std::isfinite(blowTime) ? blowTime : 0.0f;
                    g_lastUpdate = now;
                    g_lastPlanted = plantedC4;
                    g_validRemainingCount = 0;
                }
            }

            // Decrement local countdown using steady clock
            std::chrono::duration<float> dt = now - g_lastUpdate;
                g_lastUpdate = now;
                g_localRemaining -= dt.count();
                if (g_localRemaining < 0.0f) g_localRemaining = 0.0f;
                // If local timer somehow grew huge (bogus data), reset it to avoid stuck display
                if (!std::isfinite(g_localRemaining) || g_localRemaining > 300.0f) {
                    g_localRemaining = 0.0f;
                }
                remaining = g_localRemaining;
        }
        else if (g_unplantedStableCount >= STABLE_THRESHOLD) {
            // bomb removed -> reset
            g_lastPlanted = 0;
            g_cachedBlowTime = 0.0f;
            g_localRemaining = 0.0f;
            remaining = 0.0f;
        }

        // Nothing to draw if timer expired
        if (remaining <= 0.0f) return;

        // Validate remaining before display
        if (!std::isfinite(remaining) || remaining <= 0.0f) return;
        if (remaining > 300.0f) return;
        char buf[32];
        snprintf(buf, sizeof(buf), "Bomb: %.1f s", remaining);

        // Always set the window position from our stored coords so dragging updates it immediately
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
        ImGui::Begin("Bomb Timer", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoBackground);

        // Draw colored background based on urgency onto the foreground draw list so it's visible
        float urgency = 1.0f - (remaining / 45.0f);
        urgency = std::clamp(urgency, 0.0f, 1.0f);
        ImVec4 bgCol = ImVec4(0.9f, 0.2f, 0.6f, 0.3f + urgency * 0.5f);
        ImVec2 wp = ImGui::GetWindowPos();
        // Calculate size from text and add padding so the window has content
        ImVec2 txt = ImGui::CalcTextSize(buf);
        const float padX = 12.f, padY = 6.f;
        ImVec2 rectSize = ImVec2(txt.x + padX * 2.f, txt.y + padY * 2.f);

        // Use the window draw list so the rectangle is rendered with the window's draw order
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Increase alpha so the rectangle is visible
        bgCol.w = 0.6f + urgency * 0.4f;
        dl->AddRectFilled(wp, ImVec2(wp.x + rectSize.x, wp.y + rectSize.y), ImGui::ColorConvertFloat4ToU32(bgCol), 8.0f);

        // Draw text on the window draw list so it appears above the rectangle
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(wp.x + padX, wp.y + padY), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.8f, 0.2f, 1.0f)), buf);

        // Provide window content so ImGui gives the window the expected size and hover state
        ImGui::Dummy(rectSize);

        // Make window draggable
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            posX += delta.x;
            posY += delta.y;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }

        ImGui::End();
    }
}