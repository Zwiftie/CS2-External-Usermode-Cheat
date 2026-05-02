#pragma once
#include <cstddef>
#include <cstdint>

namespace offsets {

    // Module: client.dll
    // Updated from offsets.hpp (Build No: 14154, Timestamp: 2026-04-22)
    namespace client {
        constexpr std::ptrdiff_t dwCSGOInput = 0x233DD80;
        constexpr std::ptrdiff_t dwEntityList = 0x24CED50;
        constexpr std::ptrdiff_t dwGameEntitySystem = 0x24CED50;
        constexpr std::ptrdiff_t dwGameRules = 0x19EE9E8;
        constexpr std::ptrdiff_t dwGlobalVars = 0x20496A0;
        constexpr std::ptrdiff_t dwGlowManager = 0x2325D30;
        constexpr std::ptrdiff_t dwLocalPlayerController = 0x2308520;
        constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x20547A0;
        constexpr std::ptrdiff_t dwPlantedC4 = 0x2336A48;
        constexpr std::ptrdiff_t dwPrediction = 0x20546B0;
        constexpr std::ptrdiff_t dwSensitivity = 0x2326848;
        constexpr std::ptrdiff_t dwSensitivity_sensitivity = 0x58;
        constexpr std::ptrdiff_t dwViewAngles = 0x233E408;
        constexpr std::ptrdiff_t dwViewMatrix = 0x232EAC0;
        constexpr std::ptrdiff_t dwViewRender = 0x232DCB8;
        constexpr std::ptrdiff_t dwWeaponC4 = 0x22A6CB8;
    }

    // Module: engine2.dll
    // Updated from offsets.hpp (Build No: 14154, Timestamp: 2026-04-22)
    namespace engine {
        constexpr std::ptrdiff_t dwBuildNumber = 0x60CC74;
        constexpr std::ptrdiff_t dwNetworkGameClient = 0x90A0C0;
        constexpr std::ptrdiff_t dwNetworkGameClient_signOnState = 0x230;
        constexpr std::ptrdiff_t dwWindowWidth = 0x90E4E8;
        constexpr std::ptrdiff_t dwWindowHeight = 0x90E4EC;
    }

    // Common entity offsets (from client_dll.hpp)
    namespace entity {
        constexpr std::ptrdiff_t m_CBodyComponent = 0x30;
        constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
        constexpr std::ptrdiff_t m_iMaxHealth = 0x348;
        constexpr std::ptrdiff_t m_iHealth = 0x34C;
        constexpr std::ptrdiff_t m_lifeState = 0x354;
        constexpr std::ptrdiff_t m_iTeamNum = 0x3EB;
        constexpr std::ptrdiff_t m_fFlags = 0x3F8;
        constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3FC;
        constexpr std::ptrdiff_t m_vecVelocity = 0x430;
        constexpr std::ptrdiff_t m_hOwnerEntity = 0x520;
    }

    // CGameSceneNode offsets (from client_dll.hpp)
    namespace sceneNode {
        constexpr std::ptrdiff_t m_nodeToWorld = 0x10;
        constexpr std::ptrdiff_t m_vecOrigin = 0x80;
        constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
        constexpr std::ptrdiff_t m_angAbsRotation = 0xD4;
        constexpr std::ptrdiff_t m_bDormant = 0x103;
    }

    // CSkeletonInstance offsets (from client_dll.hpp)
    namespace skeleton {
        constexpr std::ptrdiff_t m_modelState = 0x150;
        // Bone access offsets (common pattern, not explicitly in schema)
        constexpr std::ptrdiff_t m_boneArraySubOffset = 0x80;
        constexpr std::ptrdiff_t m_pBoneArray = 0x1E0;
    }

    // C_BaseModelEntity offsets (from client_dll.hpp)
    namespace model {
        constexpr std::ptrdiff_t m_vecViewOffset = 0xE70;
        constexpr std::ptrdiff_t m_Glow = 0xDD8;
        constexpr std::ptrdiff_t m_Collision = 0xD28;
    }

    // C_BasePlayerPawn offsets (from client_dll.hpp)
    namespace playerPawn {
        constexpr std::ptrdiff_t m_pWeaponServices = 0x11E0;
        constexpr std::ptrdiff_t m_pItemServices = 0x11E8;
        constexpr std::ptrdiff_t m_pCameraServices = 0x1218;
        constexpr std::ptrdiff_t m_pMovementServices = 0x1220;
        constexpr std::ptrdiff_t v_angle = 0x12A4;          // Updated from schema
        constexpr std::ptrdiff_t m_hController = 0x13A8;
    }

    // C_CSPlayerPawnBase offsets (from client_dll.hpp)
    namespace csPawnBase {
        constexpr std::ptrdiff_t m_flFlashBangTime = 0x13EC;
        constexpr std::ptrdiff_t m_flFlashScreenshotAlpha = 0x13F0;
        constexpr std::ptrdiff_t m_flFlashMaxAlpha = 0x13FC;
        constexpr std::ptrdiff_t m_flFlashDuration = 0x1400;
        constexpr std::ptrdiff_t m_hOriginalController = 0x1450;
    }

    // C_CSPlayerPawn offsets (from client_dll.hpp)
    namespace csPawn
    {
        // Recoil is now stored in CCSPlayer_AimPunchServices
        constexpr std::ptrdiff_t m_pAimPunchServices = 0x1490;

        constexpr std::ptrdiff_t m_bIsScoped = 0x1C48;
        constexpr std::ptrdiff_t m_bIsDefusing = 0x1C4A;
        constexpr std::ptrdiff_t m_bIsGrabbingHostage = 0x1C4B;
        constexpr std::ptrdiff_t m_entitySpottedState = 0x1C30;
        constexpr std::ptrdiff_t m_iShotsFired = 0x1C5C;
        constexpr std::ptrdiff_t m_flVelocityModifier = 0x1C64;
        constexpr std::ptrdiff_t m_ArmorValue = 0x1C74;
        constexpr std::ptrdiff_t m_angEyeAngles = 0x3300;
        constexpr std::ptrdiff_t m_iIDEntIndex = 0x33DC;

        // EntitySpottedState_t
        constexpr std::ptrdiff_t m_bSpottedByMask = 0xC;

        // C_PlantedC4
        constexpr std::ptrdiff_t m_flC4Blow = 0x1190;
    }

    namespace CCSPlayer_AimPunchServices
    {
        constexpr std::ptrdiff_t m_aimPunchAngle = 0x70;
        constexpr std::ptrdiff_t m_aimPunchAngleVel = 0x7C;
        constexpr std::ptrdiff_t m_aimPunchTickBase = 0x88;
    }
    // CCSPlayerController offsets (from client_dll.hpp)
    namespace controller {
        constexpr std::ptrdiff_t m_iPing = 0x820;
        constexpr std::ptrdiff_t m_iCompTeammateColor = 0x840;
        constexpr std::ptrdiff_t m_szClan = 0x850;
        constexpr std::ptrdiff_t m_sSanitizedPlayerName = 0x858;
        constexpr std::ptrdiff_t m_hPlayerPawn = 0x904;
        constexpr std::ptrdiff_t m_hObserverPawn = 0x908;
        constexpr std::ptrdiff_t m_bPawnIsAlive = 0x90C;
        constexpr std::ptrdiff_t m_iPawnHealth = 0x910;
        constexpr std::ptrdiff_t m_iPawnArmor = 0x914;
        constexpr std::ptrdiff_t m_bPawnHasDefuser = 0x918;
        constexpr std::ptrdiff_t m_bPawnHasHelmet = 0x919;
        constexpr std::ptrdiff_t m_iMVPs = 0x948;
        constexpr std::ptrdiff_t m_hPawn = 0x6BC;               // from CBasePlayerController
        constexpr std::ptrdiff_t m_iszPlayerName = 0x6F0;
        constexpr std::ptrdiff_t m_steamID = 0x778;
        constexpr std::ptrdiff_t m_bIsLocalPlayerController = 0x780;
    }

    // Input button offsets (from buttons.hpp, module: client.dll)
    namespace buttons {
        constexpr std::ptrdiff_t attack = 0x204DA20;
        constexpr std::ptrdiff_t attack2 = 0x204DAB0;
        constexpr std::ptrdiff_t back = 0x204DCF0;
        constexpr std::ptrdiff_t duck = 0x204DFC0;
        constexpr std::ptrdiff_t forward = 0x204DC60;
        constexpr std::ptrdiff_t jump = 0x204DF30;
        constexpr std::ptrdiff_t left = 0x204DD80;
        constexpr std::ptrdiff_t lookatweapon = 0x233DCA0;
        constexpr std::ptrdiff_t reload = 0x204D990;
        constexpr std::ptrdiff_t right = 0x204DE10;
        constexpr std::ptrdiff_t showscores = 0x233DB80;
        constexpr std::ptrdiff_t sprint = 0x204D900;
        constexpr std::ptrdiff_t turnleft = 0x204DB40;
        constexpr std::ptrdiff_t turnright = 0x204DBD0;
        constexpr std::ptrdiff_t use = 0x204DEA0;
        constexpr std::ptrdiff_t zoom = 0x233DC10;
    }
}

namespace glow {
    constexpr std::ptrdiff_t m_glowColorOverride = 0x40;
    constexpr std::ptrdiff_t m_bGlowing = 0x51;
}