#include "ESP.h"
#include "Box.h"
#include "Skeleton.h"
#include "HealthBar.h"
#include "Name.h"
#include "Snapline.h"
#include "ArmorBar.h"
#include "Distance.h"
#include "Config.h"
#include "imgui.h"

namespace ESP {

    void Render(const EntityData* entities, int count, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
        if (!config.bEsp) return;

        for (int i = 0; i < count; i++) {
            const EntityData& ent = entities[i];
            if (!ent.valid) continue;

            Vector3 headTop = ent.headPos + Vector3{ 0, 0, 8.0f };
            Vector2 screenHead, screenFeet;

            if (!WorldToScreen(headTop, screenHead, viewMatrix, screenWidth, screenHeight)) continue;
            if (!WorldToScreen(ent.origin, screenFeet, viewMatrix, screenWidth, screenHeight)) continue;

            float boxHeight = screenFeet.y - screenHead.y;
            if (boxHeight < 4.0f) continue;

            // Box fill (if enabled)
            if (config.bEspBoxFill) {
                ImDrawList* draw = ImGui::GetBackgroundDrawList();
                ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(config.espBoxFillColor[0], config.espBoxFillColor[1], config.espBoxFillColor[2], config.espBoxFillColor[3]));
                float pad = config.espBoxFillSize;
                float left = screenHead.x - (screenFeet.y - screenHead.y) / 2.4f * 0.5f;
                float right = screenHead.x + (screenFeet.y - screenHead.y) / 2.4f * 0.5f;
                ImVec2 a(left - pad, screenHead.y - pad);
                ImVec2 b(right + pad, screenFeet.y + pad);
                draw->AddRectFilled(a, b, fillCol, 6.0f);
            }

            if (config.bEspBox)
                DrawBox(screenHead, screenFeet, config.espBoxColor);

            if (config.bEspHealth)
                DrawHealthBar(screenHead, screenFeet, ent.health, config.espHealthColor);

            if (config.bEspName && ent.name[0])
                DrawName(screenHead, ent.name, config.espNameColor);

            if (config.bEspSkeleton && ent.bonesValid)
                DrawSkeleton(ent.bones, viewMatrix, screenWidth, screenHeight, config.espSkeletonColor);

            if (config.bEspSnaplines)
                DrawSnapline(screenFeet, screenWidth, screenHeight, config.espSnaplineColor);

            if (config.bEspArmor && ent.armor > 0)
                DrawArmorBar(screenHead, screenFeet, ent.armor, config.espArmorColor);

            if (config.bEspDistance)
                DrawDistance(screenFeet, ent.distance, config.espDistanceColor);
        }
    }
}