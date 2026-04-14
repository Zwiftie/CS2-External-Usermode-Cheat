#include "Skeleton.h"
#include "imgui.h"

namespace ESP {
    void DrawSkeleton(const Vector3* bones, const Matrix4x4& vm, int w, int h, const float* color) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));

        static const int connections[][2] = {
            { BoneIndex::PELVIS, BoneIndex::SPINE_1 },
            { BoneIndex::SPINE_1, BoneIndex::SPINE_2 },
            { BoneIndex::SPINE_2, BoneIndex::NECK },
            { BoneIndex::NECK, BoneIndex::HEAD },
            { BoneIndex::NECK, BoneIndex::LEFT_SHOULDER },
            { BoneIndex::LEFT_SHOULDER, BoneIndex::LEFT_ELBOW },
            { BoneIndex::LEFT_ELBOW, BoneIndex::LEFT_HAND },
            { BoneIndex::NECK, BoneIndex::RIGHT_SHOULDER },
            { BoneIndex::RIGHT_SHOULDER, BoneIndex::RIGHT_ELBOW },
            { BoneIndex::RIGHT_ELBOW, BoneIndex::RIGHT_HAND },
            { BoneIndex::PELVIS, BoneIndex::LEFT_HIP },
            { BoneIndex::LEFT_HIP, BoneIndex::LEFT_KNEE },
            { BoneIndex::LEFT_KNEE, BoneIndex::LEFT_FOOT },
            { BoneIndex::PELVIS, BoneIndex::RIGHT_HIP },
            { BoneIndex::RIGHT_HIP, BoneIndex::RIGHT_KNEE },
            { BoneIndex::RIGHT_KNEE, BoneIndex::RIGHT_FOOT },
        };

        for (const auto& c : connections) {
            Vector2 from, to;
            if (WorldToScreen(bones[c[0]], from, vm, w, h) &&
                WorldToScreen(bones[c[1]], to, vm, w, h)) {
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), col, 1.5f);
            }
        }
    }
}