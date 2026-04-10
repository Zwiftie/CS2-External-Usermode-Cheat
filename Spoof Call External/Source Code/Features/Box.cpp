#include "Box.h"
#include "Config.h"
#include "imgui.h"

namespace ESP {
    void DrawBox(const Vector2& top, const Vector2& bottom, const float* color) {
        float height = bottom.y - top.y;
        float width = height / 2.4f;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
        ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1));

        float left = top.x - width * 0.5f;
        float right = top.x + width * 0.5f;

        draw->AddRect(ImVec2(left - 1, top.y - 1), ImVec2(right + 1, bottom.y + 1), outline, 0, 0, 1.0f);
        draw->AddRect(ImVec2(left + 1, top.y + 1), ImVec2(right - 1, bottom.y - 1), outline, 0, 0, 1.0f);
        draw->AddRect(ImVec2(left, top.y), ImVec2(right, bottom.y), col, 0, 0, 1.0f);
    }
}