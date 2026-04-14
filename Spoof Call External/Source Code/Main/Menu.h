#pragma once
#include "imgui.h"   // for ImFont

namespace Menu {
    void Render();
    bool IsVisible();
    void Toggle();
    void SetFonts(ImFont* bold, ImFont* brand);
}