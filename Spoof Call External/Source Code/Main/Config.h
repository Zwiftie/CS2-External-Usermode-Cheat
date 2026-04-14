#pragma once
#include "SDK.h"

struct Config {
    // ESP
    bool bEsp = false;
    bool bEspBox = true;
    bool bEspSkeleton = true;
    bool bEspHealth = true;
    bool bEspName = true;
    bool bEspSnaplines = false;
    bool bEspArmor = false;
    bool bEspDistance = false;
    bool bEspTeamCheck = true;
    float espBoxColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float espSkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float espSnaplineColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    float espHealthColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    float espNameColor[4] = { 1.0f, 0.7f, 0.3f, 1.0f };
    float espArmorColor[4] = { 0.2f, 0.6f, 1.0f, 1.0f };
    float espDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool bEspBoxFill = false;
    float espBoxFillColor[4] = { 0.0f, 0.78f, 1.0f, 0.25f };
    float espBoxFillSize = 8.0f;

    // Aimbot
    bool bAimbot = false;
    int aimKey = 0x01;                     // Left mouse button (0x01)
    float aimFov = 5.0f;
    float aimSmooth = 5.0f;
    int aimBone = 6;
    bool bFovCircle = false;
    float aimFovColor[4] = { 0.0f, 0.78f, 1.0f, 0.63f };
    bool bVisibleCheck = true;
    bool bRcs = false;
    bool aimBones[BoneIndex::BONE_COUNT] = {};

    // NEW aimbot features
    int aimMode = 0;                       // 0 = Always, 1 = Hold, 2 = Toggle
    bool aimModeActive = false;            // current state for toggle mode
    bool bAimJitter = false;
    float aimJitterMin = 0.0f;
    float aimJitterMax = 2.0f;
    bool bRandomizedSmooth = false;
    float randomSmoothMin = 3.0f;
    float randomSmoothMax = 12.0f;
    bool bImperfectAngles = false;
    float imperfectAngleMin = 0.0f;
    float imperfectAngleMax = 5.0f;

    // Triggerbot
    bool bTriggerbot = false;
    int triggerKey = 0x12;                 // VK_MENU (ALT)
    int triggerDelay = 10;

    // Misc
    bool bBhop = false;
    bool bNoFlash = false;

    // Glow
    bool bGlow = false;
    bool bGlowShowTeam = false;
    bool bGlowHealthBased = false;
    bool bGlowTeamBased = true;
    float glowColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
    float glowTeamColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    float glowEnemyColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
};

extern Config config;