#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <cstdio>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "Memory.h"
#include "SDK.h"
#include "Offsets.h"
#include "Config.h"
#include "ESP.h"
#include "Aimbot.h"
#include "Menu.h"
#include "Bunnyhop.h"
#include "NoFlash.h"
#include "Triggerbot.h"
#include "Glow.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ── Globals ─────────────────────────────────────────────────────────────────
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_pRTV = nullptr;

static HWND     g_overlay = nullptr;
static HWND     g_gameWnd = nullptr;
static bool     g_running = true;
static int      g_width = 1920;
static int      g_height = 1080;

static EntityData   g_entities[64];
static int          g_entityCount = 0;
static int          g_localEntityIndex = -1;

// ── Debug State (keep your full DebugInfo struct) ──────────────────────────
static struct DebugInfo {
    bool enabled = true;
    uintptr_t clientBase = 0;
    uintptr_t engineBase = 0;
    DWORD pid = 0;
    HWND gameWnd = nullptr;
    uintptr_t localController = 0;
    uintptr_t localPawn = 0;
    uintptr_t entityList = 0;
    uint8_t localTeam = 0;
    int localHealth = 0;
    Vector3 localOrigin = {};
    Vector3 viewAngles = {};
    float viewMatrix00 = 0;
    int entityCount = 0;
    int controllersFound = 0;
    int pawnsResolved = 0;
    int aliveEnemies = 0;
    int dormantSkipped = 0;
    int bonesValid = 0;
    uintptr_t lastPawn = 0;
    int lastHealth = 0;
    uint8_t lastTeam = 0;
    Vector3 lastOrigin = {};
    uintptr_t lastSceneNode = 0;
    uintptr_t lastBoneArray = 0;
    char lastName[128] = {};
    struct EntityTrace {
        int index = 0;
        uintptr_t controller = 0;
        uint32_t pawnHandle = 0;
        uintptr_t pawn = 0;
        int health = 0;
        uint8_t team = 0;
        bool isDormant = false;
        const char* failReason = "";
    };
    EntityTrace traces[24];
    int traceCount = 0;
    int localCtrlFoundIdx = -1;
    uint32_t localCtrlPawnHandle = 0;
    int noPawnHandle = 0;
    int pawnNull = 0;
    int pawnEntryNull = 0;
} g_debug;

// ── DX11 Helpers (unchanged) ───────────────────────────────────────────────
static void CreateRenderTarget() {
    ID3D11Texture2D* pBack = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    if (pBack) {
        g_pd3dDevice->CreateRenderTargetView(pBack, nullptr, &g_pRTV);
        pBack->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_pRTV) { g_pRTV->Release(); g_pRTV = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 0, 1 };
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc = { 1, 0 };
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL flArr[] = { D3D_FEATURE_LEVEL_11_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        flArr, 1, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dContext)))
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();  g_pd3dDevice = nullptr; }
}

// ── WndProc (will be used for both hijacked and custom overlay) ────────────
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── Overlay Hijacking ──────────────────────────────────────────────────────
static HWND FindOverlayWindow() {
    // Try NVIDIA GeForce Overlay first
    HWND overlay = FindWindowW(L"CEF-OSC-WIDGET", L"NVIDIA GeForce Overlay");
    if (overlay) return overlay;

    // Fallback: Discord overlay
    overlay = FindWindowW(L"Chrome_WidgetWin_0", L"Discord");
    if (overlay) return overlay;

    // Fallback: custom creation
    return nullptr;
}

static HWND SetupCustomOverlay(HINSTANCE hInst, int x, int y, int w, int h) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CS2Overlay";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName, L"", WS_POPUP,
        x, y, w, h,
        nullptr, nullptr, hInst, nullptr
    );
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    return hwnd;
}

// ── Entity Cache (full original implementation) ────────────────────────────
static void UpdateEntityData(uintptr_t localPawn, uint8_t localTeam) {
    g_entityCount = 0;
    g_debug.controllersFound = 0;
    g_debug.pawnsResolved = 0;
    g_debug.aliveEnemies = 0;
    g_debug.dormantSkipped = 0;
    g_debug.bonesValid = 0;
    g_debug.localCtrlFoundIdx = -1;
    g_debug.localCtrlPawnHandle = 0;
    g_debug.noPawnHandle = 0;
    g_debug.pawnNull = 0;
    g_debug.pawnEntryNull = 0;

    uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
    g_debug.entityList = entityList;
    if (!entityList) return;

    uintptr_t localSN = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
    Vector3 localOrigin{};
    if (localSN)
        localOrigin = mem.Read<Vector3>(localSN + offsets::sceneNode::m_vecAbsOrigin);
    g_debug.localOrigin = localOrigin;

    g_debug.traceCount = 0;

    for (int i = 1; i <= 64 && g_entityCount < 64; i++) {
        uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9) + 16));
        if (!listEntry) continue;

        uintptr_t controller = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
        if (!controller || controller < 0x10000 || controller > 0x7FFFFFFFFFFF) continue;
        g_debug.controllersFound++;

        uint32_t pawnHandle = mem.Read<uint32_t>(controller + offsets::controller::m_hPlayerPawn);

        auto addTrace = [&](const char* reason, uintptr_t pawn = 0, int hp = 0, uint8_t tm = 0, bool dorm = false) {
            if (g_debug.traceCount < 24) {
                auto& t = g_debug.traces[g_debug.traceCount++];
                t.index = i;
                t.controller = controller;
                t.pawnHandle = pawnHandle;
                t.pawn = pawn;
                t.health = hp;
                t.team = tm;
                t.isDormant = dorm;
                t.failReason = reason;
            }
            };

        if (controller == g_debug.localController) {
            g_debug.localCtrlFoundIdx = i;
            g_debug.localCtrlPawnHandle = pawnHandle;
            g_localEntityIndex = i;
        }

        if (!pawnHandle || pawnHandle == 0xFFFFFFFF) { g_debug.noPawnHandle++; addTrace("no pawn handle"); continue; }

        uintptr_t pawnEntry = mem.Read<uintptr_t>(entityList + (8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
        if (!pawnEntry) { g_debug.pawnEntryNull++; addTrace("pawnEntry null"); continue; }

        uintptr_t pawn = mem.Read<uintptr_t>(pawnEntry + 112 * ((pawnHandle & 0x7FFF) & 0x1FF));
        if (!pawn) { g_debug.pawnNull++; addTrace("pawn null"); continue; }
        if (pawn == localPawn) { addTrace("is local", pawn); continue; }
        g_debug.pawnsResolved++;

        int health = mem.Read<int>(pawn + offsets::entity::m_iHealth);
        uint8_t team = mem.Read<uint8_t>(pawn + offsets::entity::m_iTeamNum);

        if (health <= 0) { addTrace("dead (hp<=0)", pawn, health, team); continue; }
        if (health > 100) { addTrace("bad hp (>100)", pawn, health, team); continue; }
        g_debug.aliveEnemies++;

        if (config.bEspTeamCheck && team == localTeam) { addTrace("teammate", pawn, health, team); continue; }

        uintptr_t sceneNode = mem.Read<uintptr_t>(pawn + offsets::entity::m_pGameSceneNode);
        if (!sceneNode) { addTrace("no sceneNode", pawn, health, team); continue; }

        bool dormant = mem.Read<bool>(sceneNode + offsets::sceneNode::m_bDormant);
        if (dormant) { g_debug.dormantSkipped++; addTrace("dormant", pawn, health, team, true); continue; }

        addTrace("OK", pawn, health, team);

        EntityData& ent = g_entities[g_entityCount];
        ent.controller = controller;
        ent.pawn = pawn;
        ent.health = health;
        ent.team = team;
        ent.dormant = false;
        ent.alive = true;
        ent.valid = true;
        ent.origin = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
        ent.armor = mem.Read<int>(pawn + offsets::csPawn::m_ArmorValue);
        ent.hasHelmet = mem.Read<bool>(controller + offsets::controller::m_bPawnHasHelmet);
        ent.isScoped = mem.Read<bool>(pawn + offsets::csPawn::m_bIsScoped);
        ent.distance = localOrigin.Distance(ent.origin);

        mem.ReadRaw(controller + offsets::controller::m_iszPlayerName, ent.name, sizeof(ent.name));
        ent.name[127] = '\0';

        struct RawBone { float data[8]; };
        RawBone rawBones[BoneIndex::BONE_COUNT]{};

        uintptr_t boneArray = mem.Read<uintptr_t>(sceneNode + offsets::skeleton::m_pBoneArray);
        if (boneArray && mem.ReadRaw(boneArray, rawBones, sizeof(rawBones))) {
            ent.bonesValid = true;
            g_debug.bonesValid++;
            for (int b = 0; b < BoneIndex::BONE_COUNT; b++)
                ent.bones[b] = { rawBones[b].data[0], rawBones[b].data[1], rawBones[b].data[2] };
            ent.headPos = ent.bones[BoneIndex::HEAD];
        }
        else {
            ent.bonesValid = false;
            ent.headPos = ent.origin + Vector3{ 0, 0, 72.0f };
        }

        g_debug.lastPawn = pawn;
        g_debug.lastHealth = health;
        g_debug.lastTeam = team;
        g_debug.lastOrigin = ent.origin;
        g_debug.lastSceneNode = sceneNode;
        g_debug.lastBoneArray = boneArray;
        memcpy(g_debug.lastName, ent.name, sizeof(g_debug.lastName));

        g_entityCount++;
    }
}

// ── Entry Point ─────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    SetProcessDPIAware();
    // No console – remove AllocConsole for stealth. Errors will be written to a log file if needed.
    // For debugging, you can keep it, but for final release, comment out AllocConsole.

    // Wait for CS2
    printf("[*] Waiting for Counter-Strike 2...\n");
    while (!mem.Init(L"cs2.exe")) {
        Sleep(1000);
    }
    printf("[+] CS2 found  PID: %u\n", mem.pid);
    printf("[+] client.dll  0x%llX\n", static_cast<unsigned long long>(mem.client));
    printf("[+] engine2.dll 0x%llX\n", static_cast<unsigned long long>(mem.engine));

    g_debug.pid = mem.pid;
    g_debug.clientBase = mem.client;
    g_debug.engineBase = mem.engine;

    if (!mem.client) { printf("[-] FATAL: client.dll base is NULL!\n"); }
    if (!mem.engine) { printf("[-] FATAL: engine2.dll base is NULL!\n"); }

    g_gameWnd = FindWindowA(nullptr, "Counter-Strike 2");
    if (!g_gameWnd) {
        printf("[-] CS2 window not found\n");
        return 1;
    }

    RECT gr;
    GetWindowRect(g_gameWnd, &gr);
    g_width = gr.right - gr.left;
    g_height = gr.bottom - gr.top;

    // ── Overlay Hijacking ───────────────────────────────────────────────────
    g_overlay = FindOverlayWindow();
    if (g_overlay) {
        printf("[+] Hijacked existing overlay (HWND: 0x%p)\n", g_overlay);
        // Modify its style to be layered and click-through when menu is hidden
        LONG_PTR exStyle = GetWindowLongPtrW(g_overlay, GWL_EXSTYLE);
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST;
        SetWindowLongPtrW(g_overlay, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(g_overlay, RGB(0, 0, 0), 255, LWA_ALPHA);
        // Force its position and size
        SetWindowPos(g_overlay, HWND_TOPMOST, gr.left, gr.top, g_width, g_height, SWP_NOACTIVATE);
        // Subclass its window procedure so ImGui can receive input
        SetWindowLongPtrW(g_overlay, GWLP_WNDPROC, (LONG_PTR)WndProc);
        ShowWindow(g_overlay, SW_SHOW);
    }
    else {
        printf("[-] No overlay found. Creating custom overlay.\n");
        g_overlay = SetupCustomOverlay(hInst, gr.left, gr.top, g_width, g_height);
    }

    // DX11 + ImGui
    if (!CreateDeviceD3D(g_overlay)) {
        printf("[-] DX11 init failed\n");
        CleanupDeviceD3D();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(g_overlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    // Font loading (original code)
    auto TryLoadFont = [&](const char* paths[], int count, float size, const ImFontConfig* cfg) -> ImFont* {
        for (int i = 0; i < count; i++) {
            DWORD attr = GetFileAttributesA(paths[i]);
            if (attr != INVALID_FILE_ATTRIBUTES)
                return io.Fonts->AddFontFromFileTTF(paths[i], size, cfg);
        }
        return nullptr;
        };

    ImFontConfig fontCfg{};
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = false;

    const char* regularPaths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
    const char* boldPaths[] = {
        "C:\\Windows\\Fonts\\seguisb.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\calibrib.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf"
    };

    ImFont* mainFont = TryLoadFont(regularPaths, 3, 14.0f, &fontCfg);
    if (!mainFont) mainFont = io.Fonts->AddFontDefault();

    ImFont* boldFont = TryLoadFont(boldPaths, 4, 14.0f, &fontCfg);
    ImFont* brandFont = TryLoadFont(boldPaths, 4, 22.0f, &fontCfg);

    Menu::SetFonts(boldFont ? boldFont : mainFont, brandFont ? brandFont : mainFont);

    printf("[+] Overlay ready\n");
    printf("[*] INSERT  toggle menu\n");
    printf("[*] END     exit\n");

    // Main loop
    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        if (GetAsyncKeyState(VK_END) & 1) { g_running = false; break; }
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            Menu::Toggle();
            // Update click‑through state
            LONG_PTR exStyle = GetWindowLongPtr(g_overlay, GWL_EXSTYLE);
            if (Menu::IsVisible())
                exStyle &= ~WS_EX_TRANSPARENT;
            else
                exStyle |= WS_EX_TRANSPARENT;
            SetWindowLongPtrW(g_overlay, GWL_EXSTYLE, exStyle);
        }

        // Update overlay position (if game window moved/resized)
        if (!IsWindow(g_gameWnd)) { g_running = false; break; }
        RECT r;
        GetWindowRect(g_gameWnd, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        if (w != g_width || h != g_height || r.left != gr.left || r.top != gr.top) {
            MoveWindow(g_overlay, r.left, r.top, w, h, TRUE);
            g_width = w; g_height = h;
            gr = r;
        }

        // Read game data
        uintptr_t localController = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerController);
        uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);

        g_debug.localController = localController;
        g_debug.localPawn = localPawn;

        Matrix4x4 viewMatrix{};
        uint8_t localTeam = 0;

        if (localController && localPawn) {
            localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);
            g_debug.localTeam = localTeam;
            g_debug.localHealth = mem.Read<int>(localPawn + offsets::entity::m_iHealth);
            g_debug.viewAngles = mem.Read<Vector3>(mem.client + offsets::client::dwViewAngles);

            UpdateEntityData(localPawn, localTeam);
            viewMatrix = mem.Read<Matrix4x4>(mem.client + offsets::client::dwViewMatrix);
            g_debug.viewMatrix00 = viewMatrix.m[0][0];
            g_debug.entityCount = g_entityCount;

            Bunnyhop::Run(localPawn);
            NoFlash::Run(localPawn);
            Triggerbot::Run(localPawn, localTeam);
            Glow::Run(localController);
        }
        else {
            g_entityCount = 0;
            g_debug.entityCount = 0;
        }

        // Render
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (localController && localPawn) {
            ESP::Render(g_entities, g_entityCount, viewMatrix, g_width, g_height);
            Aimbot::Run(g_entities, g_entityCount, localPawn, localController, g_localEntityIndex, g_width, g_height);

            if (config.bAimbot && config.bFovCircle) {
                float radPerDeg = 3.14159265f / 180.0f;
                float screenRadius = tanf(config.aimFov * radPerDeg) / tanf(45.0f * radPerDeg) * (g_width * 0.5f);
                ImGui::GetBackgroundDrawList()->AddCircle(
                    ImVec2(g_width * 0.5f, g_height * 0.5f),
                    screenRadius, IM_COL32(0, 200, 255, 160), 64, 1.2f);
            }
        }

        Menu::Render();

        ImGui::Render();
        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dContext->OMSetRenderTargets(1, &g_pRTV, nullptr);
        g_pd3dContext->ClearRenderTargetView(g_pRTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    if (g_overlay && !FindOverlayWindow()) // if we created it ourselves, destroy it
        DestroyWindow(g_overlay);
    mem.Cleanup();

    return 0;
}