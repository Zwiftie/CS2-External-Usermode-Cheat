#include "Menu.h"
#include "Config.h"
#include "imgui.h"
#include <Windows.h>
#include <cstdio>
#include <cmath>

// ── UI Global State ───────────────────────────────────────────────────────────
static bool  g_menuVisible = true;
static int   g_activeTab = 0;
static float g_tabAnim[4] = {};
static float g_toggleAnim[64] = {};
static int   g_toggleCounter = 0;
static float g_menuOpenAnim = 0.f;
static int* g_listeningKey = nullptr;

static ImFont* g_fontBold = nullptr;
static ImFont* g_fontBrand = nullptr;

namespace Menu {
    void SetFonts(ImFont* bold, ImFont* brand) {
        g_fontBold = bold;
        g_fontBrand = brand;
    }
}

// ── Math helpers ──────────────────────────────────────────────────────────────
static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float Clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
static inline float EaseInOut(float t) { return t * t * (3.f - 2.f * t); }

// ── Palette — Tactical Amber / Thermal-HUD ────────────────────────────────────
#define C_BG0        IM_COL32(10,  11,  9,  255)
#define C_BG1        IM_COL32(16,  18,  13, 255)
#define C_BG2        IM_COL32(22,  25,  18, 255)
#define C_BG3        IM_COL32(30,  34,  24, 255)
#define C_AMBER      IM_COL32(255, 176,  0,  255)
#define C_AMBER_DIM  IM_COL32(255, 176,  0,  80)
#define C_AMBER_GLOW IM_COL32(255, 176,  0,  18)
#define C_RED        IM_COL32(220,  55,  40, 255)
#define C_RED_DIM    IM_COL32(220,  55,  40,  55)
#define C_GREEN      IM_COL32( 80, 200,  90, 255)
#define C_TEXT       IM_COL32(195, 200, 175, 255)
#define C_TEXT_DIM   IM_COL32(100, 108,  85, 255)
#define C_TEXT_MUTED IM_COL32( 55,  60,  45, 255)
#define C_BORDER     IM_COL32( 38,  44,  30, 255)
#define C_BORDER_LIT IM_COL32(255, 176,   0,  45)
#define C_NOISE      IM_COL32(255, 255, 200,   4)

// ── Draw helpers ──────────────────────────────────────────────────────────────
static void DrawScanlines(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float spacing = 3.f)
{
    dl->PushClipRect(a, b, true);
    for (float y = a.y; y < b.y; y += spacing)
        dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), col, 1.f);
    dl->PopClipRect();
}

static void DrawBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col,
    float len = 12.f, float thick = 1.8f)
{
    float w = b.x - a.x, h = b.y - a.y;
    float lx = (len > w * 0.45f) ? w * 0.45f : len;
    float ly = (len > h * 0.45f) ? h * 0.45f : len;
    dl->AddLine({ a.x,a.y }, { a.x + lx,a.y }, col, thick);
    dl->AddLine({ a.x,a.y }, { a.x,a.y + ly }, col, thick);
    dl->AddLine({ b.x,a.y }, { b.x - lx,a.y }, col, thick);
    dl->AddLine({ b.x,a.y }, { b.x,a.y + ly }, col, thick);
    dl->AddLine({ a.x,b.y }, { a.x + lx,b.y }, col, thick);
    dl->AddLine({ a.x,b.y }, { a.x,b.y - ly }, col, thick);
    dl->AddLine({ b.x,b.y }, { b.x - lx,b.y }, col, thick);
    dl->AddLine({ b.x,b.y }, { b.x,b.y - ly }, col, thick);
}

static void AmberSep(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha = 1.f)
{
    dl->AddLine(a, b, IM_COL32(255, 176, 0, (int)(12 * alpha)), 3.f);
    dl->AddLine(a, b, IM_COL32(255, 176, 0, (int)(90 * alpha)), 1.f);
}

// ── Toggle Switch ─────────────────────────────────────────────────────────────
static bool ToggleSwitch(const char* id, const char* label, bool* v)
{
    int idx = g_toggleCounter++;
    if (idx >= 64) idx = 63;

    float& anim = g_toggleAnim[idx];
    float  target = *v ? 1.f : 0.f;
    anim = Lerp(anim, target, 0.18f);
    float t = EaseInOut(Clamp01(anim));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    const float frameH = 20.f, trackW = 32.f, trackH = 11.f, knobSz = 7.f;
    float trackY = pos.y + (frameH - trackH) * 0.5f;

    float totalW = trackW + 8.f + ImGui::CalcTextSize(label).x + 4.f;
    float avail = ImGui::GetContentRegionAvail().x;
    if (totalW > avail) totalW = avail;

    ImGui::InvisibleButton(id, ImVec2(totalW, frameH));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *v = !*v;

    ImU32 trackBg = *v
        ? IM_COL32((int)(18 + 10 * t), (int)(18 + 10 * t), 13, (int)(220))
        : IM_COL32(18, 20, 14, 200);
    dl->AddRectFilled({ pos.x,trackY }, { pos.x + trackW,trackY + trackH }, trackBg, 2.f);

    if (t > 0.01f)
        dl->AddRectFilled({ pos.x,trackY }, { pos.x + trackW * t,trackY + trackH },
            IM_COL32(255, 176, 0, (int)(35 * t)), 2.f);

    dl->AddRect({ pos.x,trackY }, { pos.x + trackW,trackY + trackH },
        *v ? C_AMBER_DIM : C_BORDER, 2.f, 0, 1.f);

    float knobX = pos.x + 2.f + t * (trackW - knobSz * 2.f - 4.f);
    float knobYc = trackY + trackH * 0.5f;
    if (*v)
        dl->AddRectFilled({ knobX - 1.f, knobYc - knobSz * 0.5f - 2.f },
            { knobX + knobSz * 2.f + 1.f, knobYc + knobSz * 0.5f + 2.f },
            IM_COL32(255, 176, 0, (int)(30 * t)), 1.f);
    dl->AddRectFilled({ knobX,knobYc - knobSz * 0.5f }, { knobX + knobSz * 2.f,knobYc + knobSz * 0.5f },
        *v ? C_AMBER : IM_COL32(50, 55, 40, 255), 1.f);
    dl->AddLine({ knobX + knobSz * 0.5f,knobYc - knobSz * 0.5f + 1.f },
        { knobX + knobSz * 0.5f,knobYc + knobSz * 0.5f - 1.f },
        IM_COL32(255, 255, 200, (int)(50 * t + 10)), 1.f);

    float labelX = pos.x + trackW + 8.f;
    float labelY = pos.y + (frameH - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText({ labelX,labelY },
        *v ? C_TEXT : (hovered ? IM_COL32(135, 140, 115, 255) : C_TEXT_DIM), label);

    return *v;
}

// ── Section Label ─────────────────────────────────────────────────────────────
static void SectionLabel(const char* text)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p = ImGui::GetCursorScreenPos();
    float       avail = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled({ p.x,p.y }, { p.x + 3.f,p.y + ImGui::GetTextLineHeight() }, C_AMBER, 0.f);

    ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
    float   sz = f->LegacySize;
    dl->AddText(f, sz, { p.x + 8.f,p.y }, C_AMBER, text);

    float textW = f->CalcTextSizeA(sz, FLT_MAX, 0.f, text).x;
    float lineY = p.y + ImGui::GetTextLineHeight() * 0.5f;
    float startX = p.x + textW + 14.f;
    for (float x = startX; x < p.x + avail - 4.f; x += 6.f)
        dl->AddLine({ x,lineY }, { x + 3.f,lineY }, C_BORDER, 1.f);

    ImGui::Dummy(ImVec2(avail, ImGui::GetTextLineHeight() + 7.f));
}

// ── Card ──────────────────────────────────────────────────────────────────────
static void CardBegin(const char* id)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f, 0.098f, 0.071f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
}

static void CardEnd()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    DrawBrackets(dl, { p.x + 1,p.y + 1 }, { p.x + s.x - 2,p.y + s.y - 2 }, C_AMBER_DIM, 10.f, 1.2f);
    dl->AddLine({ p.x + 2,p.y + 1 }, { p.x + s.x - 2,p.y + 1 }, IM_COL32(255, 176, 0, 55), 1.f);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ── Nav Tab ───────────────────────────────────────────────────────────────────
static bool NavTab(const char* label, const char* subtext, int idx, float w, float h)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    bool        active = (g_activeTab == idx);

    float& anim = g_tabAnim[idx];
    anim = Lerp(anim, active ? 1.f : 0.f, 0.14f);
    float t = EaseInOut(Clamp01(anim));

    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    float bgA = Lerp(hovered ? 0.06f : 0.f, 0.14f, t);
    if (bgA > 0.005f)
        dl->AddRectFilled(pos, { pos.x + w,pos.y + h },
            IM_COL32(255, 176, 0, (int)(bgA * 255)), 0.f);

    if (t > 0.005f) {
        float barW = w * t;
        float barX = pos.x + (w - barW) * 0.5f;
        dl->AddRectFilled({ barX,pos.y + h - 2.f }, { barX + barW,pos.y + h },
            IM_COL32(255, 176, 0, (int)(220 * t)), 0.f);
        dl->AddRectFilled({ barX,pos.y + h - 5.f }, { barX + barW,pos.y + h - 2.f },
            IM_COL32(255, 176, 0, (int)(25 * t)), 0.f);
    }

    {
        char badge[4]; snprintf(badge, sizeof(badge), "%02d", idx + 1);
        ImVec2 bs = ImGui::CalcTextSize(badge);
        dl->AddText({ pos.x + w - bs.x - 6.f,pos.y + 5.f },
            active ? IM_COL32(255, 176, 0, 160) : C_TEXT_MUTED, badge);
    }

    ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
    ImVec2  ts = f->CalcTextSizeA(f->LegacySize, FLT_MAX, 0, label);
    dl->AddText(f, f->LegacySize,
        { pos.x + 12.f, pos.y + h * 0.5f - ts.y * 0.5f - (subtext[0] ? 4.f : 0.f) },
        active ? C_AMBER : (hovered ? IM_COL32(160, 165, 135, 255) : C_TEXT_DIM), label);

    if (subtext && subtext[0])
        dl->AddText({ pos.x + 12.f,pos.y + h * 0.5f + 2.f },
            active ? IM_COL32(255, 176, 0, 70) : C_TEXT_MUTED, subtext);

    if (clicked) g_activeTab = idx;
    return clicked;
}

// ── Sliders ───────────────────────────────────────────────────────────────────
static bool SliderFloat(const char* id, const char* label,
    float* v, float mn, float mx, const char* fmt)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      labelPos = ImGui::GetCursorScreenPos();
    float       avail = ImGui::GetContentRegionAvail().x;

    dl->AddText(labelPos, C_TEXT_DIM, label);
    char valBuf[32]; snprintf(valBuf, sizeof(valBuf), fmt, *v);
    ImVec2 valSz = ImGui::CalcTextSize(valBuf);
    dl->AddText({ labelPos.x + avail - valSz.x,labelPos.y }, C_AMBER, valBuf);
    ImGui::Dummy(ImVec2(avail, ImGui::GetTextLineHeight() + 2.f));

    ImVec2 trackPos = ImGui::GetCursorScreenPos();
    float  trackH = 3.f, trackW = avail;
    float  fraction = (*v - mn) / (mx - mn);

    dl->AddRectFilled({ trackPos.x,trackPos.y + 5.f }, { trackPos.x + trackW,trackPos.y + 5.f + trackH }, C_BG3, 0.f);
    dl->AddRectFilled({ trackPos.x,trackPos.y + 5.f }, { trackPos.x + trackW * fraction,trackPos.y + 5.f + trackH }, C_AMBER, 0.f);
    for (int i = 1; i < 10; i++) {
        float tx = trackPos.x + trackW * (i * 0.1f);
        dl->AddLine({ tx,trackPos.y + 3.f }, { tx,trackPos.y + 5.f }, C_BG3, 1.f);
    }

    float grabX = trackPos.x + trackW * fraction;
    float grabYc = trackPos.y + 5.f + trackH * 0.5f;
    const float d = 5.f;
    dl->AddQuadFilled({ grabX,grabYc - d - 1.f }, { grabX + d + 1.f,grabYc }, { grabX,grabYc + d + 1.f }, { grabX - d - 1.f,grabYc }, IM_COL32(0, 0, 0, 120));
    dl->AddQuadFilled({ grabX,grabYc - d }, { grabX + d,grabYc }, { grabX,grabYc + d }, { grabX - d,grabYc }, C_AMBER);
    dl->AddLine({ grabX,grabYc - d + 1.f }, { grabX + d - 1.f,grabYc }, IM_COL32(255, 240, 180, 160), 1.f);

    ImGui::SetNextItemWidth(trackW);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    bool changed = ImGui::SliderFloat(id, v, mn, mx, "");
    ImGui::PopStyleColor(5);
    ImGui::Dummy(ImVec2(avail, 5.f));
    return changed;
}

static bool SliderInt(const char* id, const char* label,
    int* v, int mn, int mx, const char* fmt)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      labelPos = ImGui::GetCursorScreenPos();
    float       avail = ImGui::GetContentRegionAvail().x;

    dl->AddText(labelPos, C_TEXT_DIM, label);
    char ffmt[32]; snprintf(ffmt, sizeof(ffmt), fmt, *v);
    ImVec2 valSz = ImGui::CalcTextSize(ffmt);
    dl->AddText({ labelPos.x + avail - valSz.x,labelPos.y }, C_AMBER, ffmt);
    ImGui::Dummy(ImVec2(avail, ImGui::GetTextLineHeight() + 2.f));

    ImVec2 trackPos = ImGui::GetCursorScreenPos();
    float  trackH = 3.f, trackW = avail;
    float  fraction = (float)(*v - mn) / (float)(mx - mn);

    dl->AddRectFilled({ trackPos.x,trackPos.y + 5.f }, { trackPos.x + trackW,trackPos.y + 5.f + trackH }, C_BG3, 0.f);
    dl->AddRectFilled({ trackPos.x,trackPos.y + 5.f }, { trackPos.x + trackW * fraction,trackPos.y + 5.f + trackH }, C_AMBER, 0.f);
    for (int i = 1; i < 10; i++) {
        float tx = trackPos.x + trackW * (i * 0.1f);
        dl->AddLine({ tx,trackPos.y + 3.f }, { tx,trackPos.y + 5.f }, C_BG3, 1.f);
    }

    float grabX = trackPos.x + trackW * fraction;
    float grabYc = trackPos.y + 5.f + trackH * 0.5f;
    const float d = 5.f;
    dl->AddQuadFilled({ grabX,grabYc - d - 1.f }, { grabX + d + 1.f,grabYc }, { grabX,grabYc + d + 1.f }, { grabX - d - 1.f,grabYc }, IM_COL32(0, 0, 0, 120));
    dl->AddQuadFilled({ grabX,grabYc - d }, { grabX + d,grabYc }, { grabX,grabYc + d }, { grabX - d,grabYc }, C_AMBER);
    dl->AddLine({ grabX,grabYc - d + 1.f }, { grabX + d - 1.f,grabYc }, IM_COL32(255, 240, 180, 160), 1.f);

    ImGui::SetNextItemWidth(trackW);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    bool changed = ImGui::SliderInt(id, v, mn, mx, "");
    ImGui::PopStyleColor(5);
    ImGui::Dummy(ImVec2(avail, 5.f));
    return changed;
}

// ── Key name helper ───────────────────────────────────────────────────────────
static const char* GetKeyName(int vk) {
    switch (vk) {
    case 0x01: return "M1";   case 0x02: return "M2";
    case 0x04: return "M3";   case 0x05: return "M4";
    case 0x06: return "M5";   case 0x10: return "SHFT";
    case 0x11: return "CTRL"; case 0x12: return "ALT";
    case 0x14: return "CAPS"; case 0x20: return "SPC";
    case 0x09: return "TAB";
    case VK_F1:  return "F1";  case VK_F2:  return "F2";
    case VK_F3:  return "F3";  case VK_F4:  return "F4";
    case VK_F5:  return "F5";  case VK_F6:  return "F6";
    case VK_F7:  return "F7";  case VK_F8:  return "F8";
    case VK_F9:  return "F9";  case VK_F10: return "F10";
    case VK_F11: return "F11"; case VK_F12: return "F12";
    default:
        if (vk >= 0x30 && vk <= 0x39) { static char b[2];b[0] = (char)vk;b[1] = 0;return b; }
        if (vk >= 0x41 && vk <= 0x5A) { static char b[2];b[0] = (char)vk;b[1] = 0;return b; }
        static char hex[8]; snprintf(hex, sizeof(hex), "0x%02X", vk); return hex;
    }
}

// ── KeyBind ───────────────────────────────────────────────────────────────────
static void KeyBind(const char* id, const char* label, int* key)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool        listening = (g_listeningKey == key);
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       avail = ImGui::GetContentRegionAvail().x;

    dl->AddText(pos, C_TEXT_DIM, label);

    char btnLabel[64];
    if (listening) snprintf(btnLabel, sizeof(btnLabel), "... ##%s", id);
    else           snprintf(btnLabel, sizeof(btnLabel), "[%s]##%s", GetKeyName(*key), id);

    float btnW = 68.f;
    ImGui::SetCursorScreenPos({ pos.x + avail - btnW, pos.y - 1.f });

    ImGui::PushStyleColor(ImGuiCol_Button,
        listening ? ImVec4(0.18f, 0.14f, 0.01f, 1.f) : ImVec4(0.09f, 0.10f, 0.07f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.16f, 0.02f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.22f, 0.02f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        listening ? ImVec4(1.f, 0.69f, 0.f, 1.f) : ImVec4(0.7f, 0.72f, 0.6f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);

    if (ImGui::Button(btnLabel, ImVec2(btnW, 0)))
        g_listeningKey = listening ? nullptr : key;

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(4);

    DrawBrackets(ImGui::GetWindowDrawList(),
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
        listening ? C_AMBER : C_BORDER, 5.f, 1.f);

    ImGui::SetCursorScreenPos({ pos.x, pos.y + ImGui::GetFrameHeight() + 2.f });
    ImGui::Dummy(ImVec2(avail, 0.f));

    if (g_listeningKey == key) {
        for (int m = 2; m <= 6; m++)
            if (GetAsyncKeyState(m) & 1) { *key = m;g_listeningKey = nullptr;return; }
        for (int k = 0x08; k <= 0xFE; k++) {
            if (k == VK_INSERT || k == VK_END) continue;
            if (k == VK_ESCAPE) { if (GetAsyncKeyState(k) & 1) { g_listeningKey = nullptr;return; }continue; }
            if (GetAsyncKeyState(k) & 1) { *key = k;g_listeningKey = nullptr;return; }
        }
    }
}

// ── Main render ───────────────────────────────────────────────────────────────
static void RenderMenu()
{
    const float W = 620.f;
    const float H = 490.f;
    const float sideW = 148.f;
    const float dt = 0.016f;

    g_menuOpenAnim = Lerp(g_menuOpenAnim, 1.f, 0.12f);
    float openT = EaseInOut(Clamp01(g_menuOpenAnim));
    g_toggleCounter = 0;

    ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2.f);

    // Empty window title (stealth)
    ImGui::Begin("##", &g_menuVisible,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();

    // Base fill + scanlines
    dl->AddRectFilled(wp, { wp.x + W,wp.y + H }, C_BG0, 2.f);
    DrawScanlines(dl, wp, { wp.x + W,wp.y + H }, C_NOISE, 3.f);

    // Double border
    dl->AddRect(wp, { wp.x + W,wp.y + H }, IM_COL32(255, 176, 0, (int)(50 * openT)), 2.f, 0, 1.5f);
    dl->AddRect({ wp.x + 3,wp.y + 3 }, { wp.x + W - 3,wp.y + H - 3 }, IM_COL32(255, 176, 0, (int)(15 * openT)), 1.f, 0, 1.f);
    DrawBrackets(dl, { wp.x + 1,wp.y + 1 }, { wp.x + W - 1,wp.y + H - 1 },
        IM_COL32(255, 176, 0, (int)(130 * openT)), 18.f, 2.f);

    // ── Sidebar ───────────────────────────────────────────────────────────────
    {
        dl->AddRectFilled(wp, { wp.x + sideW,wp.y + H }, C_BG1, 0.f);
        float sepX = wp.x + sideW;
        dl->AddLine({ sepX,wp.y + 8 }, { sepX,wp.y + H - 8 }, C_BORDER, 1.f);
        AmberSep(dl, { sepX,wp.y + 50 }, { sepX,wp.y + H - 50 }, 0.3f);

        // Brand
        float brandY = wp.y + 16.f;
        dl->AddRectFilled({ wp.x + 10,wp.y + 8 }, { wp.x + sideW - 10,wp.y + 9 }, IM_COL32(255, 176, 0, 80), 0.f);

        ImFont* bf = g_fontBrand ? g_fontBrand : ImGui::GetFont();
        const char* b1 = "ANGEL WINGS";
        ImVec2 ts1 = bf->CalcTextSizeA(bf->LegacySize, FLT_MAX, 0, b1);
        dl->AddText(bf, bf->LegacySize, { wp.x + (sideW - ts1.x) * 0.5f,brandY }, C_AMBER, b1);

        ImFont* mf = g_fontBold ? g_fontBold : ImGui::GetFont();
        const char* b2 = "E X T E R N A L";
        ImVec2 ts2 = mf->CalcTextSizeA(11.f, FLT_MAX, 0, b2);
        dl->AddText(mf, 11.f, { wp.x + (sideW - ts2.x) * 0.5f, brandY + ts1.y + 2.f }, C_TEXT_MUTED, b2);

        AmberSep(dl, { wp.x + 12,brandY + ts1.y + 16.f }, { wp.x + sideW - 12,brandY + ts1.y + 16.f }, 0.6f);

        // Nav tabs
        float navStartY = 76.f;
        ImGui::SetCursorPos({ 4.f, navStartY });
        ImGui::BeginChild("##nav", ImVec2(sideW - 8, H - navStartY - 44.f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        ImGui::Spacing();
        NavTab("VIS", "ESP / Colors", 0, sideW - 8, 44);
        ImGui::Spacing();
        NavTab("AIM", "FOV / Smooth", 1, sideW - 8, 44);
        ImGui::Spacing();
        NavTab("MISC", "Trigger / Util", 2, sideW - 8, 44);
        ImGui::EndChild();

        // Footer
        const char* ver = "V 2.0";
        ImVec2 vs = ImGui::CalcTextSize(ver);
        dl->AddText({ wp.x + (sideW - vs.x) * 0.5f, wp.y + H - 22.f }, C_TEXT_MUTED, ver);

        static float blinkT = 0.f; blinkT += dt * 2.2f;
        float blink = sinf(blinkT) * 0.5f + 0.5f;
        dl->AddRectFilled({ wp.x + sideW * 0.5f - 22.f,wp.y + H - 19.f },
            { wp.x + sideW * 0.5f - 16.f,wp.y + H - 13.f },
            IM_COL32(255, 176, 0, (int)(blink * 190 + 30)), 1.f);
    }

    // ── Content ───────────────────────────────────────────────────────────────
    ImGui::SetCursorPos({ sideW + 16.f, 14.f });
    float contentW = W - sideW - 30.f;
    float contentH = H - 28.f;
    ImGui::BeginChild("##content", ImVec2(contentW, contentH), false, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 7));

    // ── TAB 0 — VISUALS ───────────────────────────────────────────────────────
    if (g_activeTab == 0) {
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled({ p.x,p.y + 2.f }, { p.x + 4.f,p.y + 13.f }, C_AMBER, 0.f);
            ImGui::GetWindowDrawList()->AddText(
                g_fontBold ? g_fontBold : ImGui::GetFont(), 15.f, { p.x + 10.f,p.y }, C_TEXT, "VISUALS");
            ImGui::Dummy(ImVec2(contentW, 17.f));
        }

        {
            float half = contentW * 0.5f;
            ImGui::BeginGroup(); ToggleSwitch("##esp_en", "Enable ESP", &config.bEsp); ImGui::EndGroup();
            ImGui::SameLine(half);
            ImGui::BeginGroup(); ToggleSwitch("##tc_en", "Team Check", &config.bEspTeamCheck); ImGui::EndGroup();
        }

        ImGui::Spacing();
        SectionLabel("ELEMENTS");
        CardBegin("##esp_elem");
        {
            float half = (contentW - 24.f) * 0.5f;
            ImGui::BeginGroup();
            ToggleSwitch("##box", "Box", &config.bEspBox);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##box_c", config.espBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth(); ImGui::Dummy({ 0,6 });

            ToggleSwitch("##skel", "Skeleton", &config.bEspSkeleton);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##skel_c", config.espSkeletonColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth(); ImGui::Dummy({ 0,6 });

            ToggleSwitch("##hp", "Health Bar", &config.bEspHealth);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##hp_c", config.espHealthColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth(); ImGui::Dummy({ 0,6 });

            ToggleSwitch("##name", "Name", &config.bEspName);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##name_c", config.espNameColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth();
            ImGui::EndGroup();

            ImGui::SameLine(half + 12.f);

            ImGui::BeginGroup();
            ToggleSwitch("##armor", "Armor Bar", &config.bEspArmor);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##armor_c", config.espArmorColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth(); ImGui::Dummy({ 0,6 });

            ToggleSwitch("##snap", "Snaplines", &config.bEspSnaplines);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##snap_c", config.espSnaplineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth(); ImGui::Dummy({ 0,6 });

            ToggleSwitch("##dist", "Distance", &config.bEspDistance);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##dist_c", config.espDistanceColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth();
            ImGui::EndGroup();
        }
        CardEnd();

        SectionLabel("GLOW");
        CardBegin("##glow_card");
        {
            float half = (contentW - 24.f) * 0.5f;
            ImGui::BeginGroup(); ToggleSwitch("##glow_en", "Enable Glow", &config.bGlow); ImGui::EndGroup();
            ImGui::SameLine(half + 12.f);
            ImGui::BeginGroup(); ToggleSwitch("##glow_team", "Show Team", &config.bGlowShowTeam); ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::BeginGroup(); ToggleSwitch("##glow_health", "Health Based", &config.bGlowHealthBased); ImGui::EndGroup();
            ImGui::SameLine(half + 12.f);
            ImGui::BeginGroup(); ToggleSwitch("##glow_teamcol", "Team Based", &config.bGlowTeamBased); ImGui::EndGroup();

            ImGui::Spacing();
            if (!config.bGlowHealthBased) {
                if (config.bGlowTeamBased) {
                    ImGui::BeginGroup();
                    ImGui::Text("Team Color"); ImGui::SameLine(); ImGui::PushItemWidth(80);
                    ImGui::ColorEdit4("##glow_team_c", config.glowTeamColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
                    ImGui::PopItemWidth(); ImGui::EndGroup();
                    ImGui::SameLine(half + 12.f);
                    ImGui::BeginGroup();
                    ImGui::Text("Enemy Color"); ImGui::SameLine(); ImGui::PushItemWidth(80);
                    ImGui::ColorEdit4("##glow_enemy_c", config.glowEnemyColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
                    ImGui::PopItemWidth(); ImGui::EndGroup();
                }
                else {
                    ImGui::BeginGroup();
                    ImGui::Text("Glow Color"); ImGui::SameLine(); ImGui::PushItemWidth(80);
                    ImGui::ColorEdit4("##glow_single_c", config.glowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
                    ImGui::PopItemWidth(); ImGui::EndGroup();
                }
            }
        }
        CardEnd();
    }
    // ── TAB 1 — AIMBOT ────────────────────────────────────────────────────────
    else if (g_activeTab == 1) {
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled({ p.x,p.y + 2.f }, { p.x + 4.f,p.y + 13.f }, C_AMBER, 0.f);
            ImGui::GetWindowDrawList()->AddText(
                g_fontBold ? g_fontBold : ImGui::GetFont(), 15.f, { p.x + 10.f,p.y }, C_TEXT, "AIMBOT");
            ImGui::Dummy(ImVec2(contentW, 17.f));
        }

        {
            float half = contentW * 0.5f;
            ImGui::BeginGroup(); ToggleSwitch("##aim_en", "Enable Aimbot", &config.bAimbot); ImGui::EndGroup();
            ImGui::SameLine(half);
            ImGui::BeginGroup(); ToggleSwitch("##rcs", "RCS", &config.bRcs); ImGui::EndGroup();
        }

        // Aim Mode selector
        ImGui::Spacing();
        {
            ImDrawList* dl2 = ImGui::GetWindowDrawList();
            ImVec2 mp = ImGui::GetCursorScreenPos();
            dl2->AddText(mp, C_TEXT_DIM, "Aim Mode");
            ImGui::Dummy(ImVec2(contentW, ImGui::GetTextLineHeight() + 2.f));

            const char* modeLabels[] = { "Always On", "Hold Key", "Toggle" };
            float btnW = (contentW - 16.f) / 3.f;
            for (int m = 0; m < 3; m++) {
                bool active = (config.aimMode == m);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(0.3f, 0.22f, 0.0f, 1.f) : ImVec4(0.09f, 0.10f, 0.07f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.17f, 0.01f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.28f, 0.0f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    active ? ImVec4(1.f, 0.69f, 0.f, 1.f) : ImVec4(0.6f, 0.65f, 0.5f, 1.f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
                char btnId[32]; snprintf(btnId, sizeof(btnId), "%s##mode%d", modeLabels[m], m);
                if (ImGui::Button(btnId, ImVec2(btnW, 0)))
                    config.aimMode = m;
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
                // Bracket highlight on active
                if (active) {
                    ImVec2 bMin = ImGui::GetItemRectMin();
                    ImVec2 bMax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(bMin, bMax, C_AMBER_DIM, 2.f, 0, 1.f);
                }
                if (m < 2) ImGui::SameLine();
            }
        }
        ImGui::Spacing();
        SectionLabel("TARGETING");
        CardBegin("##aim_target");
        SliderFloat("##fov", "Field of View", &config.aimFov, 1.f, 30.f, "%.1f deg");
        SliderFloat("##smooth", "Smoothing", &config.aimSmooth, 1.f, 20.f, "%.1f");
        ImGui::Spacing();
        {
            dl->AddText(ImGui::GetCursorScreenPos(), C_TEXT_DIM, "Target Bones");
            ImGui::Dummy(ImVec2(contentW - 24.f, ImGui::GetTextLineHeight() + 2.f));
            float half = (contentW - 24.f) * 0.5f;
            ImGui::BeginGroup();
            ToggleSwitch("##aim_b_head", "Head", &config.aimBones[BoneIndex::HEAD]);  ImGui::Dummy({ 0,2 });
            ToggleSwitch("##aim_b_neck", "Neck", &config.aimBones[BoneIndex::NECK]);
            ImGui::EndGroup();
            ImGui::SameLine(half + 12.f);
            ImGui::BeginGroup();
            ToggleSwitch("##aim_b_chest", "Chest", &config.aimBones[BoneIndex::SPINE_2]); ImGui::Dummy({ 0,2 });
            ToggleSwitch("##aim_b_pelvis", "Pelvis", &config.aimBones[BoneIndex::PELVIS]);
            ImGui::EndGroup();
        }
        CardEnd();

        SectionLabel("OPTIONS");
        CardBegin("##aim_opts");
        {
            ToggleSwitch("##fovcircle", "FOV Circle Overlay", &config.bFovCircle);
            ImGui::SameLine(); ImGui::PushItemWidth(80);
            ImGui::ColorEdit4("##fov_c", config.aimFovColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::PopItemWidth();
            ImGui::Spacing();
            KeyBind("aimkey", "Aim Key", &config.aimKey);
            ImGui::Spacing();
            ToggleSwitch("##vischeck", "Visible Check", &config.bVisibleCheck);
            ImGui::Spacing();
            // New advanced options
            SectionLabel("ADVANCED");

            ToggleSwitch("##jitter", "Aim Jitter", &config.bAimJitter);
            if (config.bAimJitter) {
                SliderFloat("##jitmin", "Jitter Min (deg)", &config.aimJitterMin, 0.0f, 1.0f, "%.2f");
                SliderFloat("##jitmax", "Jitter Max (deg)", &config.aimJitterMax, 0.0f, 1.5f, "%.2f");
            }

            ToggleSwitch("##randsmooth", "Randomized Smooth", &config.bRandomizedSmooth);
            if (config.bRandomizedSmooth) {
                SliderFloat("##smin", "Smooth Min", &config.randomSmoothMin, 1.0f, 20.0f, "%.1f");
                SliderFloat("##smax", "Smooth Max", &config.randomSmoothMax, 1.0f, 20.0f, "%.1f");
            }

            ToggleSwitch("##imperfect", "Imperfect Angles (misses)", &config.bImperfectAngles);
            if (config.bImperfectAngles) {
                SliderFloat("##immin", "Miss Angle Min (deg)", &config.imperfectAngleMin, 0.0f, 2.0f, "%.2f");
                SliderFloat("##immax", "Miss Angle Max (deg)", &config.imperfectAngleMax, 0.0f, 3.0f, "%.2f");
            }
            SectionLabel("TRIGGERBOT");
            ToggleSwitch("##trig_en", "Enable Triggerbot", &config.bTriggerbot);
            ImGui::Spacing();
            SliderInt("##trigdelay", "Delay (ms)", &config.triggerDelay, 0, 100, "%d ms");
            ImGui::Spacing();
            KeyBind("trigkey", "Trigger Key", &config.triggerKey);
        }
        CardEnd();
    }
    // ── TAB 2 — MISC ──────────────────────────────────────────────────────────
    else if (g_activeTab == 2) {
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled({ p.x,p.y + 2.f }, { p.x + 4.f,p.y + 13.f }, C_AMBER, 0.f);
            ImGui::GetWindowDrawList()->AddText(
                g_fontBold ? g_fontBold : ImGui::GetFont(), 15.f, { p.x + 10.f,p.y }, C_TEXT, "MISCELLANEOUS");
            ImGui::Dummy(ImVec2(contentW, 17.f));
        }

        SectionLabel("MOVEMENT & VISUAL");
        CardBegin("##misc_card");
        {
            float half = (contentW - 24.f) * 0.5f;
            ImGui::BeginGroup(); ToggleSwitch("##bhop", "Bunny Hop", &config.bBhop); ImGui::EndGroup();
            ImGui::SameLine(half + 12.f);
            ImGui::BeginGroup(); ToggleSwitch("##noflash", "No Flash", &config.bNoFlash); ImGui::EndGroup();
        }
        CardEnd();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

// ── Exported namespace ────────────────────────────────────────────────────────
namespace Menu {
    void Render() {
        if (g_menuVisible) RenderMenu();
    }
    bool IsVisible() {
        return g_menuVisible;
    }
    void Toggle() {
        g_menuVisible = !g_menuVisible;
        if (g_menuVisible) g_menuOpenAnim = 0.f;
    }
}