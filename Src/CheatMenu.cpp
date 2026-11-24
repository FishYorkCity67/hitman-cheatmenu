#include "CheatMenu.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>

#include <IconsMaterialDesign.h>

#include <Glacier/ZAction.h>
#include <Glacier/ZActor.h>
#include <Glacier/ZCameraEntity.h>
#include <Glacier/ZContentKitManager.h>
#include <Glacier/ZGameLoopManager.h>
#include <Glacier/ZGameTime.h>
#include <Glacier/ZHM5CrippleBox.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZInputActionManager.h>
#include <Glacier/ZItem.h>
#include <Glacier/ZModule.h>
#include <Glacier/ZOutfit.h>
#include <Glacier/ZSpatialEntity.h>
#include <Glacier/SExternalReferences.h>
#include <Glacier/SGameUpdateEvent.h>

#include "Logging.h"
#include "Theme.h"

#undef min
#undef max

// ---------------------------------------------------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------------------------------------------------

using namespace cm;

namespace {
    int g_RowInCard = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------------------------------

CheatMenu::CheatMenu() :
    m_GodModeAction("CM_GodMode"),
    m_InvisibleAction("CM_Invisible"),
    m_NoclipAction("CM_Noclip"),
    m_TimeScaleAction("CM_TimeScale"),
    m_PushAction("CM_Push"),
    m_KnockOutAction("CM_KnockOut"),
    m_HealAction("CM_Heal"),
    m_ForwardAction("CM_Forward"),
    m_BackwardAction("CM_Backward"),
    m_LeftAction("CM_Left"),
    m_RightAction("CM_Right"),
    m_FastAction("CM_Fast") {
    m_Accent = c_Accents[0].m_Color;
}

CheatMenu::~CheatMenu() {
    const ZMemberDelegate<CheatMenu, void(const SGameUpdateEvent&)> s_Delegate(this, &CheatMenu::OnFrameUpdate);
    Globals::GameLoopManager->UnregisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    if (m_TimeScaleWasApplied) {
        Globals::GameTimeManager->m_fGameTimeMultiplier = 1.f;
    }

    m_Images.Shutdown();
}

void CheatMenu::Init() {
    Hooks::ZEntitySceneContext_ClearScene->AddDetour(this, &CheatMenu::OnClearScene);
    Hooks::ZSecuritySystemCameraManager_OnFrameUpdate->AddDetour(this, &CheatMenu::ZSecuritySystemCameraManager_OnFrameUpdate);
    Hooks::ZSecuritySystemCamera_FrameUpdate->AddDetour(this, &CheatMenu::ZSecuritySystemCamera_FrameUpdate);
    Hooks::ZHM5ItemWeapon_SetBulletsInMagazine->AddDetour(this, &CheatMenu::ZHM5ItemWeapon_SetBulletsInMagazine);
    Hooks::ZHitmanMorphemePostProcessor_UpdateWeaponRecoil->AddDetour(this, &CheatMenu::ZHitmanMorphemePostProcessor_UpdateWeaponRecoil);
    Hooks::ZHM5WeaponRecoilController_RecoilWeapon->AddDetour(this, &CheatMenu::ZHM5WeaponRecoilController_RecoilWeapon);
    Hooks::ZHM5ItemWeapon_FireProjectiles->AddDetour(this, &CheatMenu::ZHM5ItemWeapon_FireProjectiles);
    Hooks::ZHM5ItemWeapon_IsFiring->AddDetour(this, &CheatMenu::ZHM5ItemWeapon_IsFiring);
    Hooks::ZActor_YouGotHit->AddDetour(this, &CheatMenu::ZActor_YouGotHit);

    m_AccentIndex = std::clamp(static_cast<int>(GetSettingInt("ui", "accent", 0)), 0, c_AccentCount - 1);
    m_Accent = c_Accents[m_AccentIndex].m_Color;
    m_ShowHud = GetSettingBool("ui", "hud", true);
    m_OnlineImages = GetSettingBool("ui", "online_images", true);

    // Echte Gegenstandsbilder: Zwischenspeicher neben der DLL (mods\CheatMenu\images).
    {
        HMODULE s_Module = nullptr;
        wchar_t s_Path[MAX_PATH] = {};
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&g_RowInCard), &s_Module
        );
        GetModuleFileNameW(s_Module, s_Path, MAX_PATH);
        std::wstring s_Dir = s_Path;
        s_Dir = s_Dir.substr(0, s_Dir.find_last_of(L"\\/"));
        m_Images.SetOnline(m_OnlineImages);
        m_Images.Start(s_Dir + L"\\CheatMenu\\images");
    }

    m_NoclipSpeed = static_cast<float>(GetSettingDouble("movement", "noclip_speed", 6.0));
    m_NoclipBoost = static_cast<float>(GetSettingDouble("movement", "noclip_boost", 4.0));
    m_TimeScale = static_cast<float>(GetSettingDouble("world", "time_scale", 0.5));
    m_PhantomEffect = std::clamp(static_cast<int>(GetSettingInt("phantom", "effect", 0)), 0, static_cast<int>(EPhantomEffect::Count) - 1);
    m_PowerRadius = static_cast<float>(GetSettingDouble("powers", "radius", 15.0));
    m_PushStrength = static_cast<float>(GetSettingDouble("powers", "push_strength", 70.0));
    m_KnockOutSeconds = static_cast<float>(GetSettingDouble("powers", "knockout_seconds", 60.0));
}

void CheatMenu::OnEngineInitialized() {
    const ZMemberDelegate<CheatMenu, void(const SGameUpdateEvent&)> s_Delegate(this, &CheatMenu::OnFrameUpdate);
    Globals::GameLoopManager->RegisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    const char* s_Bindings =
        "CheatMenuInput={"
        "CM_GodMode=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,g);"
        "CM_Invisible=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,i);"
        "CM_Noclip=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,b);"
        "CM_TimeScale=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,t);"
        "CM_Push=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,f);"
        "CM_KnockOut=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,k);"
        "CM_Heal=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,h);"
        "CM_Forward=hold(kb,w);"
        "CM_Backward=hold(kb,s);"
        "CM_Left=hold(kb,a);"
        "CM_Right=hold(kb,d);"
        "CM_Fast=hold(kb,lshift) | hold(kb,rshift);};";

    if (!ZInputActionManager::AddBindings(s_Bindings)) {
        Logger::Warn("[CheatMenu] Failed to register hotkeys.");
    }
}

void CheatMenu::SaveSettings() {
    SetSettingInt("ui", "accent", m_AccentIndex);
    SetSettingBool("ui", "hud", m_ShowHud);
    SetSettingBool("ui", "online_images", m_OnlineImages);
    SetSettingDouble("movement", "noclip_speed", m_NoclipSpeed);
    SetSettingDouble("movement", "noclip_boost", m_NoclipBoost);
    SetSettingDouble("world", "time_scale", m_TimeScale);
    SetSettingInt("phantom", "effect", m_PhantomEffect);
    SetSettingDouble("powers", "radius", m_PowerRadius);
    SetSettingDouble("powers", "push_strength", m_PushStrength);
    SetSettingDouble("powers", "knockout_seconds", m_KnockOutSeconds);
}

void CheatMenu::SetStatus(const std::string& p_Text, float p_Seconds) {
    m_StatusMessage = p_Text;
    m_StatusTimer = p_Seconds;
}

// ---------------------------------------------------------------------------------------------------------------------
// Game loop
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent) {
    if (Functions::ZInputAction_Digital->Call(&m_GodModeAction, -1)) {
        SetGodMode(!m_GodMode);
    }

    if (Functions::ZInputAction_Digital->Call(&m_InvisibleAction, -1)) {
        SetInvisible(!m_Invisible);
    }

    if (Functions::ZInputAction_Digital->Call(&m_NoclipAction, -1)) {
        SetNoclip(!m_Noclip);
    }

    if (Functions::ZInputAction_Digital->Call(&m_TimeScaleAction, -1)) {
        m_TimeScaleEnabled = !m_TimeScaleEnabled;
    }

    if (Functions::ZInputAction_Digital->Call(&m_PushAction, -1)) {
        RequestPower(EPower::ForcePush);
    }

    if (Functions::ZInputAction_Digital->Call(&m_KnockOutAction, -1)) {
        RequestPower(EPower::KnockOutNearby);
    }

    if (Functions::ZInputAction_Digital->Call(&m_HealAction, -1)) {
        RequestPower(EPower::Heal);
    }

    ApplyTimeScale();
    UpdatePowers();
    UpdateKnockedOut();
    UpdateFrozen();
    UpdateAutoHeal();

    // Spawned helper entities die with the scene, so bring persistent cheats back once the next level is playable.
    if (m_ReapplyTimer > 0.f) {
        m_ReapplyTimer -= p_UpdateEvent.m_RealTimeDelta.ToSeconds();

        if (m_ReapplyTimer <= 0.f && SDK()->GetLocalPlayer()) {
            if (m_GodMode) SetGodMode(true);
            if (m_Invisible) SetInvisible(true);
            if (m_InfiniteAmmo) RefreshCrippleBox();
        }
    }

    UpdateNoclip(p_UpdateEvent);

    if (m_StatusTimer > 0.f) {
        m_StatusTimer -= p_UpdateEvent.m_RealTimeDelta.ToSeconds();
    }
}

void CheatMenu::UpdateNoclip(const SGameUpdateEvent& p_UpdateEvent) {
    if (!m_Noclip) {
        return;
    }

    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    const auto s_HitmanSpatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();
    const auto s_Camera = Functions::GetCurrentCamera->Call();

    if (!s_HitmanSpatial || !s_Camera) {
        return;
    }

    const SMatrix s_CameraTransform = s_Camera->GetObjectToWorldMatrix();
    const float s_Delta = p_UpdateEvent.m_RealTimeDelta.ToSeconds();

    float s_Speed = m_NoclipSpeed;

    if (Functions::ZInputAction_Digital->Call(&m_FastAction, -1)) {
        s_Speed *= m_NoclipBoost;
    }

    const float s_Step = s_Speed * s_Delta;

    // The camera looks down its negative Up axis.
    if (Functions::ZInputAction_Digital->Call(&m_ForwardAction, -1))
        m_NoclipTransform.Trans += s_CameraTransform.Up * -s_Step;

    if (Functions::ZInputAction_Digital->Call(&m_BackwardAction, -1))
        m_NoclipTransform.Trans += s_CameraTransform.Up * s_Step;

    if (Functions::ZInputAction_Digital->Call(&m_LeftAction, -1))
        m_NoclipTransform.Trans += s_CameraTransform.Right * -s_Step;

    if (Functions::ZInputAction_Digital->Call(&m_RightAction, -1))
        m_NoclipTransform.Trans += s_CameraTransform.Right * s_Step;

    s_HitmanSpatial->SetObjectToWorldMatrixFromEditor(m_NoclipTransform);
}

void CheatMenu::ApplyTimeScale() {
    if (m_TimeScaleEnabled) {
        if (Globals::GameTimeManager->m_fGameTimeMultiplier != m_TimeScale) {
            Globals::GameTimeManager->m_fGameTimeMultiplier = m_TimeScale;
        }

        m_TimeScaleWasApplied = true;
    }
    else if (m_TimeScaleWasApplied) {
        Globals::GameTimeManager->m_fGameTimeMultiplier = 1.f;
        m_TimeScaleWasApplied = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Cheats
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::SetGodMode(bool p_Enabled) {
    m_GodMode = p_Enabled;

    if (EnsureAICrippleEntity()) {
        const std::string s_PinName = p_Enabled ? "SetHeroInvincible" : "SetHeroVulnerable";
        m_AICrippleEntity.SignalInputPin(s_PinName);
    }
}

void CheatMenu::SetInvisible(bool p_Enabled) {
    m_Invisible = p_Enabled;

    if (EnsureAICrippleEntity()) {
        const std::string s_PinName = p_Enabled ? "SetHeroHidden" : "SetHeroVisible";
        m_AICrippleEntity.SignalInputPin(s_PinName);
    }
}

void CheatMenu::SetNoclip(bool p_Enabled) {
    if (p_Enabled) {
        const auto s_LocalHitman = SDK()->GetLocalPlayer();

        if (!s_LocalHitman) {
            m_Noclip = false;
            return;
        }

        if (const auto s_Spatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>()) {
            m_NoclipTransform = s_Spatial->GetObjectToWorldMatrix();
        }
    }

    m_Noclip = p_Enabled;
    RefreshCrippleBox();
}

void CheatMenu::RefreshCrippleBox() {
    const bool s_IsNeeded = m_InfiniteAmmo || m_Noclip;

    if (!s_IsNeeded) {
        if (m_CrippleBoxEntity) {
            // Hand control back to the player before removing the box.
            if (auto s_Box = m_CrippleBoxEntity.QueryInterface<ZHM5CrippleBox>()) {
                s_Box->m_bLimitedAmmo = true;
                s_Box->m_bMovementAllowed = true;
                Functions::ZHM5CrippleBox_UpdateFlags->Call(s_Box);
                Functions::ZHM5CrippleBox_SetDataOnHitman->Call(s_Box->m_rHitmanCharacter, false);
            }

            Functions::ZEntityManager_DeleteEntity->Call(Globals::EntityManager, m_CrippleBoxEntity, {});
            m_CrippleBoxEntity = {};
        }

        return;
    }

    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    if (!m_CrippleBoxEntity) {
        if (!EnsureCrippleBoxEntity()) {
            return;
        }

        auto s_Box = m_CrippleBoxEntity.QueryInterface<ZHM5CrippleBox>();
        s_Box->m_bActivateOnStart = true;
        s_Box->m_rHitmanCharacter = s_LocalHitman;
        s_Box->m_bLimitedAmmo = !m_InfiniteAmmo;
        s_Box->m_bMovementAllowed = !m_Noclip;
        s_Box->Activate(0);
        return;
    }

    auto s_Box = m_CrippleBoxEntity.QueryInterface<ZHM5CrippleBox>();
    s_Box->m_bLimitedAmmo = !m_InfiniteAmmo;
    s_Box->m_bMovementAllowed = !m_Noclip;
    Functions::ZHM5CrippleBox_UpdateFlags->Call(s_Box);
    Functions::ZHM5CrippleBox_SetDataOnHitman->Call(s_Box->m_rHitmanCharacter, false);
}

bool CheatMenu::EnsureAICrippleEntity() {
    if (m_AICrippleEntity) {
        return true;
    }

    const auto s_Scene = Globals::Hitman5Module->m_pEntitySceneContext->m_pScene;

    if (!s_Scene) {
        return false;
    }

    constexpr auto s_FactoryId = ResId<"[modules:/zaicrippleentity.class].pc_entitytype">;

    TResourcePtr<ZTemplateEntityFactory> s_Factory;
    Globals::ResourceManager->LoadResource(s_Factory, s_FactoryId);

    if (!s_Factory) {
        return false;
    }

    SExternalReferences s_ExternalRefs;
    Functions::ZEntityManager_NewEntity->Call(
        Globals::EntityManager, m_AICrippleEntity, "", s_Factory, s_Scene.m_entityRef, s_ExternalRefs, -1
    );

    return static_cast<bool>(m_AICrippleEntity);
}

bool CheatMenu::EnsureCrippleBoxEntity() {
    if (m_CrippleBoxEntity) {
        return true;
    }

    const auto s_Scene = Globals::Hitman5Module->m_pEntitySceneContext->m_pScene;

    if (!s_Scene) {
        return false;
    }

    constexpr auto s_FactoryId = ResId<"[modules:/zhm5cripplebox.class].pc_entitytype">;

    TResourcePtr<ZTemplateEntityFactory> s_Factory;
    Globals::ResourceManager->LoadResource(s_Factory, s_FactoryId);

    if (!s_Factory) {
        return false;
    }

    SExternalReferences s_ExternalRefs;
    Functions::ZEntityManager_NewEntity->Call(
        Globals::EntityManager, m_CrippleBoxEntity, "", s_Factory, s_Scene.m_entityRef, s_ExternalRefs, -1
    );

    return static_cast<bool>(m_CrippleBoxEntity);
}

void CheatMenu::DestroySpawnedEntities() {
    if (m_AICrippleEntity) {
        Functions::ZEntityManager_DeleteEntity->Call(Globals::EntityManager, m_AICrippleEntity, {});
        m_AICrippleEntity = {};
    }

    if (m_CrippleBoxEntity) {
        Functions::ZEntityManager_DeleteEntity->Call(Globals::EntityManager, m_CrippleBoxEntity, {});
        m_CrippleBoxEntity = {};
    }
}

void CheatMenu::DisableAll() {
    if (m_GodMode) SetGodMode(false);
    if (m_Invisible) SetInvisible(false);

    m_CamerasDisabled = false;
    m_InfiniteAmmo = false;
    m_NoReload = false;
    m_NoRecoil = false;
    m_PerfectAim = false;
    m_RapidFire = false;
    m_OneHitKill = false;
    m_TimeScaleEnabled = false;
    m_AutoHeal = false;
    m_PhantomAllWeapons = false;
    m_FreezeNpcs = false;

    SetNoclip(false);
    RefreshCrippleBox();

    SetStatus("Alle Cheats deaktiviert");
}

int CheatMenu::ActiveCheatCount() const {
    return static_cast<int>(ActiveCheatNames().size());
}

std::vector<const char*> CheatMenu::ActiveCheatNames() const {
    std::vector<const char*> s_Names;

    if (m_GodMode) s_Names.push_back("Unverwundbar");
    if (m_Invisible) s_Names.push_back("Unsichtbar");
    if (m_CamerasDisabled) s_Names.push_back("Kameras aus");
    if (m_InfiniteAmmo) s_Names.push_back("Unendlich Munition");
    if (m_NoReload) s_Names.push_back("Kein Nachladen");
    if (m_NoRecoil) s_Names.push_back("Kein Rückstoß");
    if (m_PerfectAim) s_Names.push_back("Perfekte Präzision");
    if (m_RapidFire) s_Names.push_back("Schnellfeuer");
    if (m_OneHitKill) s_Names.push_back("One-Hit-Kill");
    if (m_Noclip) s_Names.push_back("Noclip");
    if (m_TimeScaleEnabled) s_Names.push_back("Zeitmanipulation");
    if (m_AutoHeal) s_Names.push_back("Auto-Heilung");
    if (m_PhantomAllWeapons) s_Names.push_back("Phantom-Munition");
    if (m_FreezeNpcs) s_Names.push_back("NPCs eingefroren");

    return s_Names;
}

bool CheatMenu::TabHasActiveCheat(ETab p_Tab) const {
    switch (p_Tab) {
        case ETab::Player: return m_GodMode || m_Invisible || m_CamerasDisabled || m_AutoHeal;
        case ETab::Weapons: return m_InfiniteAmmo || m_NoReload || m_NoRecoil || m_PerfectAim || m_RapidFire || m_OneHitKill || m_PhantomAllWeapons;
        case ETab::Inventory: return m_PhantomGiven;
        case ETab::Powers: return m_FreezeNpcs || !m_KnockedOut.empty();
        case ETab::Movement: return m_Noclip;
        case ETab::World: return m_TimeScaleEnabled;
        default: return false;
    }
}

void CheatMenu::SavePosition(size_t p_Slot) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    if (const auto s_Spatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>()) {
        m_PositionSlots[p_Slot].m_Transform = m_Noclip ? m_NoclipTransform : s_Spatial->GetObjectToWorldMatrix();
        m_PositionSlots[p_Slot].m_IsSet = true;
        m_StatusMessage = "Position " + std::to_string(p_Slot + 1) + " gespeichert";
        m_StatusTimer = 2.5f;
    }
}

void CheatMenu::LoadPosition(size_t p_Slot) {
    if (!m_PositionSlots[p_Slot].m_IsSet) {
        return;
    }

    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    if (const auto s_Spatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>()) {
        s_Spatial->SetObjectToWorldMatrixFromEditor(m_PositionSlots[p_Slot].m_Transform);
        m_NoclipTransform = m_PositionSlots[p_Slot].m_Transform;
        m_StatusMessage = "Zu Position " + std::to_string(p_Slot + 1) + " teleportiert";
        m_StatusTimer = 2.5f;
    }
}

void CheatMenu::TeleportForward(float p_Distance) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();
    const auto s_Camera = Functions::GetCurrentCamera->Call();

    if (!s_LocalHitman || !s_Camera) {
        return;
    }

    const auto s_Spatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();

    if (!s_Spatial) {
        return;
    }

    // Use the horizontal part of the view direction so 47 doesn't end up inside the floor.
    const SMatrix s_CameraTransform = s_Camera->GetObjectToWorldMatrix();
    float s_DirX = -s_CameraTransform.Up.x;
    float s_DirY = -s_CameraTransform.Up.y;
    const float s_Length = std::sqrt(s_DirX * s_DirX + s_DirY * s_DirY);

    if (s_Length < 0.001f) {
        return;
    }

    s_DirX /= s_Length;
    s_DirY /= s_Length;

    SMatrix s_Transform = m_Noclip ? m_NoclipTransform : s_Spatial->GetObjectToWorldMatrix();
    s_Transform.Trans.x += s_DirX * p_Distance;
    s_Transform.Trans.y += s_DirY * p_Distance;

    s_Spatial->SetObjectToWorldMatrixFromEditor(s_Transform);
    m_NoclipTransform = s_Transform;
}

void CheatMenu::TeleportAllActorsToPlayer() {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    const auto s_HitmanSpatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();

    if (!s_HitmanSpatial) {
        return;
    }

    int s_Count = 0;

    for (int i = 0; i < *Globals::NextActorId; ++i) {
        ZActor* s_Actor = Globals::ActorManager->m_activatedActors[i].m_pInterfaceRef;

        if (!s_Actor) {
            continue;
        }

        ZEntityRef s_Ref;
        s_Actor->GetID(s_Ref);

        if (auto s_ActorSpatial = s_Ref.QueryInterface<ZSpatialEntity>()) {
            s_ActorSpatial->SetObjectToWorldMatrixFromEditor(s_HitmanSpatial->GetObjectToWorldMatrix());
            ++s_Count;
        }
    }

    m_StatusMessage = std::to_string(s_Count) + " NPCs herbeigerufen";
    m_StatusTimer = 2.5f;
}

void CheatMenu::TeleportAllItemsToPlayer() {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    const auto s_HitmanSpatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();

    if (!s_HitmanSpatial) {
        return;
    }

    int s_Count = 0;
    const ZHM5ActionManager* s_ActionManager = Globals::HM5ActionManager;

    for (size_t i = 0; i < s_ActionManager->m_Actions.size(); ++i) {
        const ZHM5Action* s_Action = s_ActionManager->m_Actions[i];

        if (!s_Action || s_Action->m_eActionType != EActionType::AT_PICKUP) {
            continue;
        }

        const ZHM5Item* s_Item = s_Action->m_Object.QueryInterface<ZHM5Item>();

        if (!s_Item || !s_Item->m_rGeomentity.m_pInterfaceRef) {
            continue;
        }

        s_Item->m_rGeomentity.m_pInterfaceRef->SetObjectToWorldMatrixFromEditor(s_HitmanSpatial->GetObjectToWorldMatrix());
        ++s_Count;
    }

    m_StatusMessage = std::to_string(s_Count) + " Gegenstände herbeigerufen";
    m_StatusTimer = 2.5f;
}

void CheatMenu::TakeNearestActorOutfit() {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return;
    }

    const auto s_HitmanSpatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();

    if (!s_HitmanSpatial) {
        return;
    }

    const SVector3 s_HitmanPos = s_HitmanSpatial->GetObjectToWorldMatrix().Trans;
    ZActor* s_Nearest = nullptr;
    float s_NearestDistance = 6.f;

    for (int i = 0; i < *Globals::NextActorId; ++i) {
        ZActor* s_Actor = Globals::ActorManager->m_activatedActors[i].m_pInterfaceRef;

        if (!s_Actor) {
            continue;
        }

        ZEntityRef s_Ref;
        s_Actor->GetID(s_Ref);

        const auto s_ActorSpatial = s_Ref.QueryInterface<ZSpatialEntity>();

        if (!s_ActorSpatial) {
            continue;
        }

        const SVector3 s_Offset = SVector3(s_ActorSpatial->GetObjectToWorldMatrix().Trans) - s_HitmanPos;
        const float s_Distance = std::sqrt(s_Offset.x * s_Offset.x + s_Offset.y * s_Offset.y + s_Offset.z * s_Offset.z);

        if (s_Distance < s_NearestDistance) {
            s_Nearest = s_Actor;
            s_NearestDistance = s_Distance;
        }
    }

    if (!s_Nearest) {
        m_StatusMessage = "Kein NPC in der Nähe (max. 6 m)";
        m_StatusTimer = 2.5f;
        return;
    }

    EquipOutfit(
        s_Nearest->m_rOutfit,
        static_cast<uint8_t>(s_Nearest->m_nOutfitCharset),
        static_cast<uint8_t>(s_Nearest->m_nOutfitVariation)
    );
}

void CheatMenu::EquipOutfit(const TEntityRef<ZGlobalOutfitKit>& p_Kit, uint8_t p_Charset, uint8_t p_Variation) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman || !p_Kit.m_pInterfaceRef) {
        return;
    }

    if (p_Charset >= p_Kit.m_pInterfaceRef->m_aCharSets.size()) {
        p_Charset = 0;
    }

    Functions::ZHitman5_SetOutfit->Call(s_LocalHitman.m_pInterfaceRef, p_Kit, p_Charset, p_Variation, false, false);

    m_StatusMessage = std::string("Outfit: ") + p_Kit.m_pInterfaceRef->m_sCommonName.c_str();
    m_StatusTimer = 2.5f;
}

void CheatMenu::RebuildOutfitList() {
    m_Outfits.clear();

    ZContentKitManager* s_ContentKitManager = Globals::ContentKitManager;

    if (!s_ContentKitManager) {
        return;
    }

    for (const auto& s_Pair : s_ContentKitManager->m_repositoryGlobalOutfitKits) {
        const ZGlobalOutfitKit* s_Kit = s_Pair.second.m_pInterfaceRef;

        if (!s_Kit || s_Kit->m_sCommonName.size() == 0) {
            continue;
        }

        std::string s_Name(s_Kit->m_sCommonName.c_str(), s_Kit->m_sCommonName.size());
        m_Outfits.push_back({ s_Name, ToLower(s_Name), s_Pair.second });
    }

    std::sort(m_Outfits.begin(), m_Outfits.end(), [](const auto& a, const auto& b) { return a.m_NameLower < b.m_NameLower; });
}

// ---------------------------------------------------------------------------------------------------------------------
// UI: entry points
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::OnDrawMenu() {
    if (ImGui::Button(ICON_MD_TUNE " CHEAT MENU")) {
        m_ShowWindow = !m_ShowWindow;
    }
}

void CheatMenu::OnDrawUI(bool p_HasFocus) {
    if (!p_HasFocus) {
        DrawHud();
        return;
    }

    if (!m_ShowWindow) {
        return;
    }

    // Fertig geladene Gegenstandsbilder in Texturen umwandeln (ein paar pro Bild, damit nichts ruckelt).
    m_Images.UploadPending(4);

    PushTheme();

    ImGui::SetNextWindowSize({ 1020.f, 680.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 820.f, 520.f }, { FLT_MAX, FLT_MAX });

    constexpr ImGuiWindowFlags s_Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##CheatMenuWindow", &m_ShowWindow, s_Flags)) {
        DrawHeader();

        const float s_BodyHeight = ImGui::GetContentRegionAvail().y - c_FooterHeight;

        DrawSidebar();

        ImGui::SameLine(0.f, 0.f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 26.f, 22.f });
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });

        if (ImGui::BeginChild("##Content", { 0.f, s_BodyHeight }, ImGuiChildFlags_AlwaysUseWindowPadding)) {
            switch (m_CurrentTab) {
                case ETab::Home: DrawHomeTab(); break;
                case ETab::Player: DrawPlayerTab(); break;
                case ETab::Weapons: DrawWeaponsTab(); break;
                case ETab::Inventory: DrawInventoryTab(); break;
                case ETab::Powers: DrawPowersTab(); break;
                case ETab::Movement: DrawMovementTab(); break;
                case ETab::World: DrawWorldTab(); break;
                case ETab::Outfits: DrawOutfitsTab(); break;
                case ETab::Settings: DrawSettingsTab(); break;
                default: break;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        DrawFooter();
    }

    ImGui::End();

    PopTheme();
}

// ---------------------------------------------------------------------------------------------------------------------
// UI: theme
// ---------------------------------------------------------------------------------------------------------------------

namespace {
    constexpr int c_PushedColors = 22;
    constexpr int c_PushedVars = 13;
}

void CheatMenu::PushTheme() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, c_WindowBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_PopupBg, c_CardBg);
    ImGui::PushStyleColor(ImGuiCol_Border, c_Border);
    ImGui::PushStyleColor(ImGuiCol_Text, c_Text);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, c_Muted);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, c_Raised);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Mix(c_Raised, c_Text, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Mix(c_Raised, c_Text, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, m_Accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Mix(m_Accent, c_Text, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Button, c_Raised);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Mix(c_Raised, m_Accent, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Mix(c_Raised, m_Accent, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Header, Alpha(m_Accent, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Alpha(m_Accent, 0.24f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, Alpha(m_Accent, 0.32f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, c_Raised);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, Mix(c_Raised, c_Text, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, m_Accent);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Alpha(m_Accent, 0.35f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 12.f, 8.f });
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 10.f, 10.f });
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 16.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 8.f);
}

void CheatMenu::PopTheme() {
    ImGui::PopStyleVar(c_PushedVars);
    ImGui::PopStyleColor(c_PushedColors);
}

// ---------------------------------------------------------------------------------------------------------------------
// UI: frame
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::DrawHeader() {
    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const ImVec2 s_Origin = ImGui::GetCursorScreenPos();
    const float s_Width = ImGui::GetWindowWidth();
    const float s_Base = ImGui::GetFontSize();

    // Logo tile.
    const ImVec2 s_LogoMin = Offset(s_Origin, 22.f, 16.f);
    const ImVec2 s_LogoMax = Offset(s_LogoMin, 36.f, 36.f);
    s_DrawList->AddRectFilled(s_LogoMin, s_LogoMax, Col(m_Accent), 10.f);

    ImFont* s_Black = SDK()->GetImGuiBlackFont();
    const ImVec2 s_LogoText = Measure(s_Black, s_Base * 1.05f, "47");
    Text(
        s_DrawList, s_Black, s_Base * 1.05f,
        Offset(s_LogoMin, (36.f - s_LogoText.x) * 0.5f, (36.f - s_LogoText.y) * 0.5f),
        { 1.f, 1.f, 1.f, 1.f }, "47"
    );

    // Title.
    Text(s_DrawList, SDK()->GetImGuiBoldFont(), s_Base * 1.2f, Offset(s_Origin, 70.f, 15.f), c_Text, "Cheat Menu");
    Text(
        s_DrawList, SDK()->GetImGuiRegularFont(), s_Base * 0.88f, Offset(s_Origin, 70.f, 37.f), c_Muted,
        "HITMAN World of Assassination  \xC2\xB7  Einzelspieler"
    );

    // Close button.
    const ImVec2 s_ClosePos = Offset(s_Origin, s_Width - 54.f, 18.f);
    ImGui::SetCursorScreenPos(s_ClosePos);

    if (ImGui::InvisibleButton("##Close", { 32.f, 32.f })) {
        m_ShowWindow = false;
    }

    const bool s_CloseHovered = ImGui::IsItemHovered();

    if (s_CloseHovered) {
        s_DrawList->AddRectFilled(s_ClosePos, Offset(s_ClosePos, 32.f, 32.f), Col(Alpha(c_Text, 0.07f)), 8.f);
    }

    const ImVec2 s_CloseSize = Measure(nullptr, s_Base * 1.1f, ICON_MD_CLOSE);
    Text(
        s_DrawList, nullptr, s_Base * 1.1f,
        Offset(s_ClosePos, (32.f - s_CloseSize.x) * 0.5f, (32.f - s_CloseSize.y) * 0.5f),
        s_CloseHovered ? c_Text : c_Muted, ICON_MD_CLOSE
    );

    // Divider.
    s_DrawList->AddLine(
        Offset(s_Origin, 0.f, c_HeaderHeight - 1.f), Offset(s_Origin, s_Width, c_HeaderHeight - 1.f), Col(c_Border)
    );

    ImGui::SetCursorScreenPos(Offset(s_Origin, 0.f, c_HeaderHeight));
}

void CheatMenu::DrawSidebar() {
    struct STabInfo {
        ETab m_Tab;
        const char* m_Icon;
        const char* m_Label;
    };

    static constexpr STabInfo s_Tabs[] = {
        { ETab::Home, ICON_MD_DASHBOARD, "Start" },
        { ETab::Player, ICON_MD_PERSON, "Spieler" },
        { ETab::Weapons, ICON_MD_GPS_FIXED, "Waffen" },
        { ETab::Inventory, ICON_MD_BACKPACK, "Inventar" },
        { ETab::Powers, ICON_MD_BOLT, "Kr\xC3\xA4" "fte & NPCs" },
        { ETab::Movement, ICON_MD_EXPLORE, "Bewegung" },
        { ETab::World, ICON_MD_PUBLIC, "Welt" },
        { ETab::Outfits, ICON_MD_CHECKROOM, "Outfits" },
        { ETab::Settings, ICON_MD_SETTINGS, "Einstellungen" },
    };

    const float s_Height = ImGui::GetContentRegionAvail().y - c_FooterHeight;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, c_SidebarBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 12.f, 16.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.f, 4.f });

    if (ImGui::BeginChild("##Sidebar", { c_SidebarWidth, s_Height }, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
        const float s_Base = ImGui::GetFontSize();

        Text(
            s_DrawList, SDK()->GetImGuiBoldFont(), s_Base * 0.74f, Offset(ImGui::GetCursorScreenPos(), 10.f, 0.f),
            c_Muted, "MEN\xC3\x9C"
        );
        ImGui::Dummy({ 0.f, s_Base * 0.9f });

        for (const auto& s_Info : s_Tabs) {
            const bool s_Selected = m_CurrentTab == s_Info.m_Tab;
            const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
            const float s_ItemWidth = ImGui::GetContentRegionAvail().x;
            constexpr float s_ItemHeight = 40.f;

            ImGui::PushID(s_Info.m_Label);

            if (ImGui::InvisibleButton("##Tab", { s_ItemWidth, s_ItemHeight })) {
                m_CurrentTab = s_Info.m_Tab;

                if (m_CurrentTab == ETab::Outfits) {
                    RebuildOutfitList();
                }

                if (m_CurrentTab == ETab::Inventory && m_Items.empty()) {
                    LoadItemCatalog();
                }
            }

            const bool s_Hovered = ImGui::IsItemHovered();
            const float s_T = Animate(ImGui::GetItemID(), s_Selected ? 1.f : (s_Hovered ? 0.35f : 0.f));

            if (s_T > 0.f) {
                s_DrawList->AddRectFilled(
                    s_Pos, Offset(s_Pos, s_ItemWidth, s_ItemHeight),
                    Col(s_Selected ? Alpha(m_Accent, 0.14f * s_T) : Alpha(c_Text, 0.1f * s_T)), 9.f
                );
            }

            if (s_Selected) {
                s_DrawList->AddRectFilled(
                    Offset(s_Pos, 0.f, 10.f), Offset(s_Pos, 3.f, s_ItemHeight - 10.f), Col(m_Accent), 2.f
                );
            }

            const ImVec4 s_IconColor = s_Selected ? m_Accent : c_Muted;
            const ImVec4 s_LabelColor = s_Selected ? c_Text : Mix(c_Muted, c_Text, s_Hovered ? 0.7f : 0.25f);
            const float s_IconSize = s_Base * 1.15f;

            Text(
                s_DrawList, nullptr, s_IconSize,
                Offset(s_Pos, 14.f, (s_ItemHeight - s_IconSize) * 0.5f), s_IconColor, s_Info.m_Icon
            );
            Text(
                s_DrawList, SDK()->GetImGuiMediumFont(), s_Base,
                Offset(s_Pos, 44.f, (s_ItemHeight - s_Base) * 0.5f), s_LabelColor, s_Info.m_Label
            );

            if (TabHasActiveCheat(s_Info.m_Tab)) {
                s_DrawList->AddCircleFilled(Offset(s_Pos, s_ItemWidth - 16.f, s_ItemHeight * 0.5f), 3.5f, Col(m_Accent));
            }

            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void CheatMenu::DrawFooter() {
    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const ImVec2 s_Origin = ImGui::GetCursorScreenPos();
    const float s_Width = ImGui::GetWindowWidth();
    const float s_Base = ImGui::GetFontSize();
    const float s_Small = s_Base * 0.86f;
    const float s_TextY = (c_FooterHeight - s_Small) * 0.5f;

    s_DrawList->AddLine(s_Origin, Offset(s_Origin, s_Width, 0.f), Col(c_Border));

    // Status on the left.
    const bool s_InLevel = SDK()->GetLocalPlayer() && Globals::Hitman5Module->m_pEntitySceneContext->m_pScene;
    s_DrawList->AddCircleFilled(Offset(s_Origin, 26.f, c_FooterHeight * 0.5f), 4.f, Col(s_InLevel ? c_Success : c_Muted));

    const char* s_Status = s_InLevel ? "Im Einsatz" : "Kein Level geladen";

    if (m_StatusTimer > 0.f && !m_StatusMessage.empty()) {
        s_Status = m_StatusMessage.c_str();
    }

    Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Small, Offset(s_Origin, 38.f, s_TextY), c_Text, s_Status);

    // Active count and kill switch on the right.
    const int s_Active = ActiveCheatCount();
    const std::string s_Count = std::to_string(s_Active) + (s_Active == 1 ? " Cheat aktiv" : " Cheats aktiv");

    constexpr float s_ButtonWidth = 104.f;
    constexpr float s_ButtonHeight = 30.f;
    const ImVec2 s_ButtonPos = Offset(s_Origin, s_Width - s_ButtonWidth - 16.f, (c_FooterHeight - s_ButtonHeight) * 0.5f);

    const ImVec2 s_CountSize = Measure(SDK()->GetImGuiRegularFont(), s_Small, s_Count.c_str());
    Text(
        s_DrawList, SDK()->GetImGuiRegularFont(), s_Small,
        { s_ButtonPos.x - s_CountSize.x - 14.f, s_Origin.y + s_TextY },
        s_Active > 0 ? m_Accent : c_Muted, s_Count.c_str()
    );

    ImGui::SetCursorScreenPos(s_ButtonPos);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.f, 6.f });
    ImGui::BeginDisabled(s_Active == 0);

    if (ImGui::Button(ICON_MD_POWER_SETTINGS_NEW " Alles aus", { s_ButtonWidth, s_ButtonHeight })) {
        DisableAll();
    }

    ImGui::EndDisabled();
    ImGui::PopStyleVar();
}

void CheatMenu::DrawHud() {
    if (!m_ShowHud) {
        return;
    }

    const auto s_Names = ActiveCheatNames();

    if (s_Names.empty()) {
        return;
    }

    const ImGuiViewport* s_Viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        { s_Viewport->WorkPos.x + s_Viewport->WorkSize.x - 20.f, s_Viewport->WorkPos.y + 20.f }, ImGuiCond_Always,
        { 1.f, 0.f }
    );

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Alpha(c_WindowBg, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_Border, Alpha(c_Border, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, c_Text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14.f, 10.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8.f, 5.f });

    constexpr ImGuiWindowFlags s_Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##CheatMenuHud", nullptr, s_Flags)) {
        ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
        const float s_Base = ImGui::GetFontSize();

        Text(
            s_DrawList, SDK()->GetImGuiBoldFont(), s_Base * 0.74f, ImGui::GetCursorScreenPos(), c_Muted,
            "AKTIVE CHEATS"
        );
        ImGui::Dummy({ 120.f, s_Base * 0.8f });

        for (const char* s_Name : s_Names) {
            const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
            s_DrawList->AddCircleFilled(Offset(s_Pos, 4.f, s_Base * 0.5f), 3.f, Col(m_Accent));
            ImGui::SetCursorScreenPos(Offset(s_Pos, 14.f, 0.f));
            ImGui::TextUnformatted(s_Name);
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}

// ---------------------------------------------------------------------------------------------------------------------
// UI: widgets
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::PageTitle(const char* p_Icon, const char* p_Title, const char* p_Subtitle) {
    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const float s_Base = ImGui::GetFontSize();
    const float s_TitleSize = s_Base * 1.55f;

    Text(s_DrawList, nullptr, s_TitleSize, Offset(s_Pos, 0.f, 2.f), m_Accent, p_Icon);
    Text(s_DrawList, SDK()->GetImGuiBoldFont(), s_TitleSize, Offset(s_Pos, s_TitleSize + 10.f, 0.f), c_Text, p_Title);
    Text(s_DrawList, SDK()->GetImGuiRegularFont(), s_Base * 0.92f, Offset(s_Pos, 0.f, s_TitleSize + 8.f), c_Muted, p_Subtitle);

    ImGui::Dummy({ 0.f, s_TitleSize + s_Base * 0.92f + 14.f });
}

bool CheatMenu::BeginCard(const char* p_Id, const char* p_Caption) {
    if (p_Caption) {
        const float s_Base = ImGui::GetFontSize();
        Text(
            ImGui::GetWindowDrawList(), SDK()->GetImGuiBoldFont(), s_Base * 0.74f,
            Offset(ImGui::GetCursorScreenPos(), 4.f, 0.f), c_Muted, p_Caption
        );
        ImGui::Dummy({ 0.f, s_Base * 0.74f });
    }

    g_RowInCard = 0;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, c_CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16.f, 10.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 10.f, 8.f });

    ImGui::BeginChild(
        p_Id, { 0.f, 0.f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    return true;
}

void CheatMenu::EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Dummy({ 0.f, 8.f });
}

bool CheatMenu::ToggleRow(const char* p_Icon, const char* p_Label, const char* p_Description, bool* p_Value, const char* p_Hotkey) {
    ImGui::PushID(p_Label);

    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const float s_Width = ImGui::GetContentRegionAvail().x;
    constexpr float s_Height = 58.f;

    if (g_RowInCard++ > 0) {
        s_DrawList->AddLine(Offset(s_Pos, 0.f, -4.f), Offset(s_Pos, s_Width, -4.f), Col(Alpha(c_Border, 0.7f)));
    }

    const bool s_Clicked = ImGui::InvisibleButton("##Row", { s_Width, s_Height });
    const bool s_Hovered = ImGui::IsItemHovered();

    if (s_Clicked) {
        *p_Value = !*p_Value;
    }

    const float s_T = Animate(ImGui::GetItemID(), *p_Value ? 1.f : 0.f);

    if (s_Hovered) {
        s_DrawList->AddRectFilled(
            Offset(s_Pos, -8.f, 2.f), Offset(s_Pos, s_Width + 8.f, s_Height - 2.f), Col(Alpha(c_Text, 0.03f)), 9.f
        );
    }

    // Icon badge.
    constexpr float s_Badge = 36.f;
    const ImVec2 s_BadgePos = Offset(s_Pos, 0.f, (s_Height - s_Badge) * 0.5f);
    s_DrawList->AddRectFilled(
        s_BadgePos, Offset(s_BadgePos, s_Badge, s_Badge),
        Col(Mix(Alpha(c_Text, 0.05f), Alpha(m_Accent, 0.18f), s_T)), 10.f
    );

    const float s_IconSize = s_Base * 1.1f;
    const ImVec2 s_IconDim = Measure(nullptr, s_IconSize, p_Icon);
    Text(
        s_DrawList, nullptr, s_IconSize,
        Offset(s_BadgePos, (s_Badge - s_IconDim.x) * 0.5f, (s_Badge - s_IconDim.y) * 0.5f),
        Mix(c_Muted, m_Accent, s_T), p_Icon
    );

    // Label and description.
    const float s_DescSize = s_Base * 0.86f;
    const float s_TextTop = s_Pos.y + (s_Height - (s_Base + s_DescSize + 4.f)) * 0.5f;
    Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Base, { s_Pos.x + 50.f, s_TextTop }, c_Text, p_Label);
    Text(
        s_DrawList, SDK()->GetImGuiRegularFont(), s_DescSize, { s_Pos.x + 50.f, s_TextTop + s_Base + 4.f }, c_Muted,
        p_Description
    );

    // Switch and optional hotkey chip.
    constexpr float s_SwitchWidth = 42.f;
    constexpr float s_SwitchHeight = 24.f;
    const ImVec2 s_SwitchPos = Offset(s_Pos, s_Width - s_SwitchWidth, (s_Height - s_SwitchHeight) * 0.5f);
    DrawSwitch(s_DrawList, s_SwitchPos, s_T, s_Hovered, m_Accent);

    if (p_Hotkey) {
        const float s_ChipText = s_Base * 0.78f;
        const ImVec2 s_ChipDim = Measure(SDK()->GetImGuiMediumFont(), s_ChipText, p_Hotkey);
        const ImVec2 s_ChipMax = { s_SwitchPos.x - 14.f, s_Pos.y + s_Height * 0.5f + s_ChipDim.y * 0.5f + 4.f };
        const ImVec2 s_ChipMin = { s_ChipMax.x - s_ChipDim.x - 14.f, s_ChipMax.y - s_ChipDim.y - 8.f };

        s_DrawList->AddRect(s_ChipMin, s_ChipMax, Col(c_Border), 6.f);
        Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_ChipText, Offset(s_ChipMin, 7.f, 4.f), c_Muted, p_Hotkey);
    }

    ImGui::PopID();
    return s_Clicked;
}

bool CheatMenu::SliderRow(const char* p_Label, const char* p_Format, float* p_Value, float p_Min, float p_Max, ImGuiSliderFlags p_Flags) {
    ImGui::PushID(p_Label);

    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const float s_Width = ImGui::GetContentRegionAvail().x;

    char s_ValueText[32];
    snprintf(s_ValueText, sizeof(s_ValueText), p_Format, *p_Value);

    Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Base, Offset(s_Pos, 0.f, 4.f), c_Text, p_Label);

    const ImVec2 s_ValueDim = Measure(SDK()->GetImGuiBoldFont(), s_Base, s_ValueText);
    Text(s_DrawList, SDK()->GetImGuiBoldFont(), s_Base, Offset(s_Pos, s_Width - s_ValueDim.x, 4.f), m_Accent, s_ValueText);

    ImGui::Dummy({ 0.f, s_Base + 4.f });

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8.f, 4.f });
    ImGui::SetNextItemWidth(s_Width);
    const bool s_Changed = ImGui::SliderFloat("##Slider", p_Value, p_Min, p_Max, "", ImGuiSliderFlags_AlwaysClamp | p_Flags);
    ImGui::PopStyleVar();

    ImGui::PopID();
    return s_Changed;
}

bool CheatMenu::ActionButton(const char* p_Label, float p_Width, bool p_Primary) {
    if (p_Primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, m_Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Mix(m_Accent, c_Text, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Mix(m_Accent, { 0.f, 0.f, 0.f, 1.f }, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_Text, { 1.f, 1.f, 1.f, 1.f });
    }

    ImGui::PushFont(SDK()->GetImGuiMediumFont(), 0.f);
    const bool s_Pressed = ImGui::Button(p_Label, { p_Width, 38.f });
    ImGui::PopFont();

    if (p_Primary) {
        ImGui::PopStyleColor(4);
    }

    return s_Pressed;
}

void CheatMenu::InfoBox(const char* p_Icon, const char* p_Text, bool p_Warning) {
    const ImVec4 s_Tint = p_Warning ? c_Warning : m_Accent;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Alpha(s_Tint, 0.08f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14.f, 12.f });

    ImGui::PushID(p_Text);
    ImGui::BeginChild(
        "##Info", { 0.f, 0.f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar
    );

    ImGui::PushStyleColor(ImGuiCol_Text, s_Tint);
    ImGui::TextUnformatted(p_Icon);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 10.f);
    ImGui::PushStyleColor(ImGuiCol_Text, Mix(c_Text, c_Muted, 0.25f));
    ImGui::TextWrapped("%s", p_Text);
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopID();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Dummy({ 0.f, 4.f });
}

// ---------------------------------------------------------------------------------------------------------------------
// UI: pages
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::DrawPlayerTab() {
    PageTitle(ICON_MD_PERSON, "Spieler", "Schutz und Tarnung f\xC3\xBCr Agent 47.");

    BeginCard("##Protection", "SCHUTZ");

    bool s_GodMode = m_GodMode;
    if (ToggleRow(ICON_MD_SECURITY, "Unverwundbar", "Kein Schaden durch Sch\xC3\xBCsse, Explosionen oder St\xC3\xBCrze.", &s_GodMode, "Strg+G")) {
        SetGodMode(s_GodMode);
    }

    bool s_Invisible = m_Invisible;
    if (ToggleRow(ICON_MD_VISIBILITY_OFF, "Unsichtbar", "NPCs und Kameras bemerken dich nicht mehr.", &s_Invisible, "Strg+I")) {
        SetInvisible(s_Invisible);
    }

    ToggleRow(ICON_MD_VIDEOCAM_OFF, "\xC3\x9C" "berwachungskameras aus", "Kameras zeichnen nichts mehr auf.", &m_CamerasDisabled);

    EndCard();

    BeginCard("##Health", "GESUNDHEIT");
    ToggleRow(ICON_MD_FAVORITE, "Auto-Heilung", "Deine Gesundheit f\xC3\xBCllt sich sofort wieder auf.", &m_AutoHeal);
    ImGui::Dummy({ 0.f, 2.f });

    if (ActionButton(ICON_MD_LOCAL_HOSPITAL " Sofort voll heilen", -FLT_MIN, true)) {
        RequestPower(EPower::Heal);
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    InfoBox(
        ICON_MD_INFO_OUTLINE,
        "Unverwundbar, Unsichtbar und Unendlich Munition werden nach einem Ladebildschirm automatisch wieder aktiviert.",
        false
    );
}

void CheatMenu::DrawWeaponsTab() {
    PageTitle(ICON_MD_GPS_FIXED, "Waffen", "Munition, Pr\xC3\xA4zision und Feuerkraft.");

    BeginCard("##Ammo", "MUNITION");

    bool s_InfiniteAmmo = m_InfiniteAmmo;
    if (ToggleRow(ICON_MD_ALL_INCLUSIVE, "Unendlich Munition", "Deine Reservemunition geht nie aus.", &s_InfiniteAmmo)) {
        m_InfiniteAmmo = s_InfiniteAmmo;
        RefreshCrippleBox();
    }

    ToggleRow(ICON_MD_REPLAY, "Kein Nachladen", "Das Magazin wird vor jedem Schuss wieder voll gemacht.", &m_NoReload);

    EndCard();

    BeginCard("##Handling", "HANDHABUNG");
    ToggleRow(ICON_MD_CENTER_FOCUS_STRONG, "Perfekte Pr\xC3\xA4zision", "Jeder Schuss trifft genau die Mitte des Fadenkreuzes.", &m_PerfectAim);
    ToggleRow(ICON_MD_GAVEL, "Kein R\xC3\xBC" "cksto\xC3\x9F", "Die Waffe bleibt beim Schie\xC3\x9F" "en ruhig.", &m_NoRecoil);
    ToggleRow(ICON_MD_FAST_FORWARD, "Schnellfeuer", "Keine Wartezeit zwischen den Sch\xC3\xBCssen.", &m_RapidFire);
    EndCard();

    BeginCard("##Damage", "SCHADEN");
    ToggleRow(ICON_MD_WHATSHOT, "One-Hit-Kill", "Jeder Treffer schaltet einen NPC sofort aus.", &m_OneHitKill);
    ToggleRow(
        ICON_MD_AUTO_AWESOME, "Phantom-Munition f\xC3\xBCr alle Waffen",
        "Jede deiner Waffen wirkt wie die Phantom X-47 (Wirkung unten).", &m_PhantomAllWeapons
    );
    ImGui::Dummy({ 0.f, 2.f });

    static const char* const s_Effects[] = { ICON_MD_WHATSHOT " T\xC3\xB6" "dlich", ICON_MD_BEDTIME " Bewusstlos", ICON_MD_CASINO " Zufall" };

    if (SegmentedControl("##PhantomEffect", s_Effects, 3, &m_PhantomEffect)) {
        SaveSettings();
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    InfoBox(
        ICON_MD_BACKPACK,
        "Die erfundene Waffe \"Phantom X-47\" bekommst du im Inventar. Getroffene NPCs sterben sofort oder fallen bewusstlos um.",
        false
    );
}

void CheatMenu::DrawMovementTab() {
    PageTitle(ICON_MD_EXPLORE, "Bewegung", "Freie Bewegung und Teleport.");

    BeginCard("##Noclip", "NOCLIP");

    bool s_Noclip = m_Noclip;
    if (ToggleRow(ICON_MD_FLIGHT, "Noclip", "Mit WASD durch W\xC3\xA4nde fliegen, Shift f\xC3\xBCr Turbo.", &s_Noclip, "Strg+B")) {
        SetNoclip(s_Noclip);
    }

    ImGui::Dummy({ 0.f, 2.f });

    if (SliderRow("Fluggeschwindigkeit", "%.1f m/s", &m_NoclipSpeed, 1.f, 30.f)) {
        SaveSettings();
    }

    if (SliderRow("Turbo-Faktor (Shift)", "%.1fx", &m_NoclipBoost, 1.f, 10.f)) {
        SaveSettings();
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    BeginCard("##Teleport", "TELEPORT");
    ImGui::Dummy({ 0.f, 2.f });

    const float s_Half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ActionButton(ICON_MD_NEAR_ME " 5 m nach vorne", s_Half, true)) {
        TeleportForward(5.f);
    }

    ImGui::SameLine();

    if (ActionButton(ICON_MD_ARROW_UPWARD " 20 m nach vorne", s_Half)) {
        TeleportForward(20.f);
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    BeginCard("##Slots", "GESPEICHERTE POSITIONEN");
    ImGui::Dummy({ 0.f, 2.f });

    for (size_t i = 0; i < m_PositionSlots.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        const auto& s_Slot = m_PositionSlots[i];
        const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
        const float s_Base = ImGui::GetFontSize();
        ImDrawList* s_DrawList = ImGui::GetWindowDrawList();

        char s_Label[32];
        snprintf(s_Label, sizeof(s_Label), "Slot %zu", i + 1);
        Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Base, Offset(s_Pos, 0.f, 10.f), c_Text, s_Label);

        char s_Coords[96];

        if (s_Slot.m_IsSet) {
            snprintf(
                s_Coords, sizeof(s_Coords), "%.1f / %.1f / %.1f",
                s_Slot.m_Transform.Trans.x, s_Slot.m_Transform.Trans.y, s_Slot.m_Transform.Trans.z
            );
        }
        else {
            snprintf(s_Coords, sizeof(s_Coords), "leer");
        }

        Text(s_DrawList, SDK()->GetImGuiRegularFont(), s_Base * 0.86f, Offset(s_Pos, 72.f, 11.f), c_Muted, s_Coords);

        const float s_ButtonWidth = 112.f;
        ImGui::SetCursorScreenPos(Offset(s_Pos, ImGui::GetContentRegionAvail().x - s_ButtonWidth * 2.f - 10.f, 0.f));

        if (ActionButton(ICON_MD_SAVE " Speichern", s_ButtonWidth)) {
            SavePosition(i);
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(!s_Slot.m_IsSet);

        if (ActionButton(ICON_MD_MY_LOCATION " Laden", s_ButtonWidth, true)) {
            LoadPosition(i);
        }

        ImGui::EndDisabled();
        ImGui::PopID();
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();
}

void CheatMenu::DrawWorldTab() {
    PageTitle(ICON_MD_PUBLIC, "Welt", "Steuere den Lauf der Zeit.");

    BeginCard("##Time", "ZEIT");
    ToggleRow(ICON_MD_SLOW_MOTION_VIDEO, "Zeitmanipulation", "Spielgeschwindigkeit mit dem Regler unten \xC3\xA4ndern.", &m_TimeScaleEnabled, "Strg+T");

    ImGui::Dummy({ 0.f, 2.f });

    // Logarithmisch: 0,05x bis 100x lassen sich auf einem Regler gleich fein einstellen.
    if (SliderRow("Spielgeschwindigkeit", m_TimeScale < 10.f ? "%.2fx" : "%.0fx", &m_TimeScale, 0.05f, 100.f, ImGuiSliderFlags_Logarithmic)) {
        SaveSettings();
    }

    ImGui::Dummy({ 0.f, 2.f });

    struct SPreset {
        const char* m_Label;
        float m_Value;
    };

    static constexpr SPreset s_Presets[] = {
        { "0.1x", 0.1f }, { "0.5x", 0.5f }, { "1x", 1.f }, { "2x", 2.f }, { "5x", 5.f }, { "10x", 10.f }, { "50x", 50.f }, { "100x", 100.f },
    };

    const float s_PresetWidth =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (std::size(s_Presets) - 1)) /
        std::size(s_Presets);

    for (size_t i = 0; i < std::size(s_Presets); ++i) {
        if (i > 0) {
            ImGui::SameLine();
        }

        const bool s_Active = m_TimeScaleEnabled && std::fabs(m_TimeScale - s_Presets[i].m_Value) < 0.001f;

        if (ActionButton(s_Presets[i].m_Label, s_PresetWidth, s_Active)) {
            m_TimeScale = s_Presets[i].m_Value;
            m_TimeScaleEnabled = true;
            SaveSettings();
        }
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    if (m_TimeScale > 10.f) {
        InfoBox(
            ICON_MD_WARNING,
            "Sehr hohe Geschwindigkeiten (\xC3\xBC" "ber 10x) k\xC3\xB6nnen Physik und NPC-Wege durcheinanderbringen \xE2\x80\x93 "
            "Strg+T schaltet jederzeit zur\xC3\xBC" "ck auf normal.",
            true
        );
    }
}

void CheatMenu::DrawOutfitsTab() {
    PageTitle(ICON_MD_CHECKROOM, "Outfits", "Wechsle sofort in jede Verkleidung.");

    BeginCard("##Quick", "SCHNELLWAHL");
    ImGui::Dummy({ 0.f, 2.f });

    if (ActionButton(ICON_MD_FACE " Outfit vom n\xC3\xA4" "chsten NPC \xC3\xBC" "bernehmen", -FLT_MIN, true)) {
        TakeNearestActorOutfit();
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    BeginCard("##Catalog", "ALLE OUTFITS");
    ImGui::Dummy({ 0.f, 2.f });

    if (m_Outfits.empty()) {
        RebuildOutfitList();
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##Search", ICON_MD_SEARCH " Outfit suchen ...", m_OutfitSearch, sizeof(m_OutfitSearch));

    const std::string s_Filter = ToLower(m_OutfitSearch);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.f, 0.f, 0.f, 0.15f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.f, 6.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.f, 2.f });

    if (ImGui::BeginChild("##OutfitList", { 0.f, 240.f }, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        int s_Shown = 0;

        for (const auto& s_Outfit : m_Outfits) {
            if (!s_Filter.empty() && s_Outfit.m_NameLower.find(s_Filter) == std::string::npos) {
                continue;
            }

            ++s_Shown;
            ImGui::PushID(&s_Outfit);

            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, { 0.f, 0.5f });

            if (ImGui::Selectable(s_Outfit.m_Name.c_str(), false, 0, { 0.f, 30.f })) {
                EquipOutfit(s_Outfit.m_Kit, 0, 0);
            }

            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        if (s_Shown == 0) {
            ImGui::TextDisabled(m_Outfits.empty() ? "Lade zuerst ein Level." : "Keine Treffer.");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();
}

void CheatMenu::DrawSettingsTab() {
    PageTitle(ICON_MD_SETTINGS, "Einstellungen", "Passe das Men\xC3\xBC an.");

    BeginCard("##Appearance", "DARSTELLUNG");

    ImDrawList* s_DrawList = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    const ImVec2 s_LabelPos = ImGui::GetCursorScreenPos();
    Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Base, Offset(s_LabelPos, 0.f, 8.f), c_Text, "Akzentfarbe");
    Text(
        s_DrawList, SDK()->GetImGuiRegularFont(), s_Base * 0.86f, Offset(s_LabelPos, 0.f, 8.f + s_Base + 4.f), c_Muted,
        c_Accents[m_AccentIndex].m_Name
    );

    constexpr float s_Swatch = 26.f;
    constexpr float s_Gap = 10.f;
    const float s_Width = ImGui::GetContentRegionAvail().x;
    ImVec2 s_SwatchPos = Offset(s_LabelPos, s_Width - (s_Swatch + s_Gap) * c_AccentCount + s_Gap, 12.f);

    for (int i = 0; i < c_AccentCount; ++i) {
        ImGui::SetCursorScreenPos(s_SwatchPos);
        ImGui::PushID(i);

        if (ImGui::InvisibleButton("##Swatch", { s_Swatch, s_Swatch })) {
            m_AccentIndex = i;
            m_Accent = c_Accents[i].m_Color;
            SaveSettings();
        }

        const ImVec2 s_Center = Offset(s_SwatchPos, s_Swatch * 0.5f, s_Swatch * 0.5f);

        if (i == m_AccentIndex) {
            s_DrawList->AddCircle(s_Center, s_Swatch * 0.5f + 1.f, Col(c_Text), 0, 2.f);
        }

        s_DrawList->AddCircleFilled(
            s_Center, s_Swatch * 0.5f - (ImGui::IsItemHovered() ? 2.f : 4.f), Col(c_Accents[i].m_Color)
        );

        ImGui::PopID();
        s_SwatchPos.x += s_Swatch + s_Gap;
    }

    ImGui::SetCursorScreenPos(Offset(s_LabelPos, 0.f, 58.f));
    g_RowInCard = 1;

    if (ToggleRow(ICON_MD_LAYERS, "HUD-Anzeige", "Aktive Cheats oben rechts einblenden, wenn das Men\xC3\xBC zu ist.", &m_ShowHud)) {
        SaveSettings();
    }

    if (ToggleRow(
        ICON_MD_IMAGE, "Echte Bilder im Inventar",
        "Renderbilder der Gegenst\xC3\xA4nde aus dem Bildarchiv von HITMAPS laden (werden danach lokal gespeichert).",
        &m_OnlineImages
    )) {
        m_Images.SetOnline(m_OnlineImages);
        SaveSettings();
    }

    EndCard();

    BeginCard("##Keys", "TASTENK\xC3\x9CRZEL");

    static constexpr std::pair<const char*, const char*> s_Keys[] = {
        { "Men\xC3\xBC \xC3\xB6" "ffnen", "^ / ~" },
        { "Unverwundbar", "Strg + G" },
        { "Unsichtbar", "Strg + I" },
        { "Noclip", "Strg + B" },
        { "Zeitmanipulation", "Strg + T" },
        { "Kraftsto\xC3\x9F", "Strg + F" },
        { "Alle in der N\xC3\xA4he k.o.", "Strg + K" },
        { "Sofort heilen", "Strg + H" },
        { "Noclip-Steuerung", "WASD + Shift" },
    };

    for (const auto& [s_Action, s_Key] : s_Keys) {
        const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
        const float s_RowWidth = ImGui::GetContentRegionAvail().x;
        Text(s_DrawList, SDK()->GetImGuiRegularFont(), s_Base, Offset(s_Pos, 0.f, 6.f), c_Text, s_Action);

        const ImVec2 s_KeyDim = Measure(SDK()->GetImGuiMediumFont(), s_Base * 0.86f, s_Key);
        const ImVec2 s_ChipMin = Offset(s_Pos, s_RowWidth - s_KeyDim.x - 16.f, 2.f);
        s_DrawList->AddRectFilled(s_ChipMin, Offset(s_ChipMin, s_KeyDim.x + 16.f, s_KeyDim.y + 10.f), Col(c_Raised), 6.f);
        Text(s_DrawList, SDK()->GetImGuiMediumFont(), s_Base * 0.86f, Offset(s_ChipMin, 8.f, 5.f), c_Text, s_Key);

        ImGui::Dummy({ 0.f, s_Base + 14.f });
    }

    EndCard();

    InfoBox(
        ICON_MD_WARNING,
        "Nutze Cheats nur zum Spa\xC3\x9F im Einzelspieler. In Elusive Targets, bei Bestenlisten oder im Multiplayer "
        "k\xC3\xB6nnen sie dir unfaire Vorteile geben und den Spielstand verf\xC3\xA4lschen.",
        true
    );
}

// ---------------------------------------------------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------------------------------------------------

DEFINE_PLUGIN_DETOUR(CheatMenu, void, OnClearScene, ZEntitySceneContext* th, bool p_FullyUnloadScene) {
    DestroySpawnedEntities();

    m_Noclip = false;
    m_Outfits.clear();
    m_ReapplyTimer = 3.f;

    for (auto& s_Slot : m_PositionSlots) {
        s_Slot.m_IsSet = false;
    }

    // Alles, was auf Objekte dieser Szene zeigt, ist gleich ungueltig.
    m_Items.clear();
    m_RepositoryResource = {};
    m_MagazineSize.clear();
    m_KnockedOut.clear();
    m_Impulses.clear();
    m_FrozenTransforms.clear();
    m_PhantomGiven = false;

    {
        std::scoped_lock s_Lock(m_PendingMutex);
        m_PendingPowers.clear();
        m_PendingHits.clear();
    }

    return { HookAction::Continue() };
}

DEFINE_PLUGIN_DETOUR(
    CheatMenu, void, ZSecuritySystemCameraManager_OnFrameUpdate, ZSecuritySystemCameraManager* th,
    const SGameUpdateEvent& updateEvent
) {
    if (m_Invisible || m_CamerasDisabled) {
        return { HookAction::Return() };
    }

    return { HookAction::Continue() };
}

DEFINE_PLUGIN_DETOUR(
    CheatMenu, void, ZSecuritySystemCamera_FrameUpdate, ZSecuritySystemCamera* th, const SGameUpdateEvent& updateEvent
) {
    if (m_Invisible || m_CamerasDisabled) {
        return { HookAction::Return() };
    }

    return { HookAction::Continue() };
}

DEFINE_PLUGIN_DETOUR(CheatMenu, void, ZHM5ItemWeapon_SetBulletsInMagazine, IFirearm* th, int32_t nBullets) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!th || !s_LocalHitman) {
        return { HookAction::Continue() };
    }

    ZHM5ItemWeapon* s_Weapon = static_cast<ZHM5ItemWeapon*>(th);

    if (s_Weapon->m_pOwner != s_LocalHitman.m_entityRef) {
        return { HookAction::Continue() };
    }

    // Merken, wie voll das Magazin dieser Waffe sein kann (Nachladen/Aufheben setzt es auf voll).
    if (nBullets > 0 && nBullets <= 500) {
        int& s_Size = m_MagazineSize[s_Weapon];
        s_Size = std::max(s_Size, static_cast<int>(nBullets));
    }

    if (!m_NoReload) {
        return { HookAction::Continue() };
    }

    if (s_Weapon->m_nBulletsFired == s_Weapon->m_nBulletsToFire) {
        s_Weapon->m_nBulletsFired = 0;
    }

    if (nBullets != 0) {
        return { HookAction::Continue() };
    }

    if (!s_LocalHitman.m_pInterfaceRef->IsInfiniteAmmoEnabled()) {
        auto s_Character = s_LocalHitman.m_pInterfaceRef->m_pCharacter.m_pInterfaceRef;
        auto s_Controllers = &s_Character->m_rSubcontrollerContainer.m_pInterfaceRef->m_aReferencedControllers;
        auto s_Inventory = static_cast<ZCharacterSubcontrollerInventory*>((*s_Controllers)[6].m_pInterfaceRef);

        const eAmmoType s_AmmoType = th->GetAmmoType();
        uint32 s_AmmoInPocket = Functions::ZCharacterSubcontrollerInventory_GetAmmoInPocketForType->Call(s_Inventory, s_AmmoType);

        if (s_AmmoInPocket > 0) {
            s_AmmoInPocket -= s_Weapon->GetMagazineCapacity();
            s_Inventory->m_nAmmoInPocket[static_cast<size_t>(s_AmmoType)] = s_AmmoInPocket;
            nBullets = s_Weapon->GetMagazineCapacity();
        }
    }
    else {
        nBullets = s_Weapon->GetMagazineCapacity();
    }

    p_Hook->CallOriginal(th, nBullets);

    return { HookAction::Return() };
}

DEFINE_PLUGIN_DETOUR(
    CheatMenu,
    void,
    ZHitmanMorphemePostProcessor_UpdateWeaponRecoil,
    ZHitmanMorphemePostProcessor* th,
    float fDeltaTime,
    const THashMap<int32_t, int32_t, TDefaultHashMapPolicy<int32_t>>& charboneMap,
    TArrayRef<int32_t> hierarchy
) {
    if (m_NoRecoil) {
        return { HookAction::Return() };
    }

    return { HookAction::Continue() };
}

DEFINE_PLUGIN_DETOUR(
    CheatMenu, void, ZHM5WeaponRecoilController_RecoilWeapon, ZHM5WeaponRecoilController* th,
    const TEntityRef<ZHM5ItemWeapon>& rWeapon
) {
    if (!m_NoRecoil) {
        return { HookAction::Continue() };
    }

    p_Hook->CallOriginal(th, rWeapon);
    th->m_vRecoil = SVector2(0.f, 0.f);

    return { HookAction::Return() };
}

DEFINE_PLUGIN_DETOUR(CheatMenu, bool, ZHM5ItemWeapon_FireProjectiles, ZHM5ItemWeapon* th, bool bMayStartSound) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!th || !s_LocalHitman || th->m_pOwner != s_LocalHitman.m_entityRef) {
        return { HookAction::Continue() };
    }

    // Kein Nachladen: das Magazin VOR jedem Schuss auffuellen. So wird es nie leer und das Spiel
    // startet nie die Nachlade-Animation. (Frueher wurde erst bei 0 Schuss reagiert – das ruft das Spiel
    // aber nicht mehr zuverlaessig ueber SetBulletsInMagazine auf.)
    if (m_NoReload) {
        int s_Size = 0;

        if (const auto s_It = m_MagazineSize.find(th); s_It != m_MagazineSize.end()) {
            s_Size = s_It->second;
        }

        if (s_Size <= 0) {
            const int s_Capacity = th->GetMagazineCapacity();
            s_Size = s_Capacity > 0 && s_Capacity <= 500 ? s_Capacity : 0;
        }

        if (s_Size > 0) {
            Hooks::ZHM5ItemWeapon_SetBulletsInMagazine->CallOriginal(static_cast<IFirearm*>(th), s_Size);
        }

        if (th->m_nBulletsFired == th->m_nBulletsToFire) {
            th->m_nBulletsFired = 0;
        }
    }

    // Phantom: Treffer in den naechsten Augenblicken gehoeren zu diesem Schuss.
    if (m_PhantomAllWeapons || IsPhantomWeapon(th)) {
        m_LastPhantomShot = NowSeconds();
    }

    if (!m_PerfectAim) {
        return { HookAction::Continue() };
    }

    const bool s_Result = p_Hook->CallOriginal(th, bMayStartSound);
    th->m_fPrecisionFactor = 0.f;

    return { HookAction::Return(), s_Result };
}

DEFINE_PLUGIN_DETOUR(CheatMenu, bool, ZHM5ItemWeapon_IsFiring, IFirearm* th) {
    if (!m_RapidFire) {
        return { HookAction::Continue() };
    }

    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        return { HookAction::Continue() };
    }

    ZHM5ItemWeapon* s_Weapon = static_cast<ZHM5ItemWeapon*>(th);

    if (s_Weapon->m_pOwner == s_LocalHitman.m_entityRef) {
        s_Weapon->m_tLastShootTime = ZGameTime {};
    }

    return { HookAction::Continue() };
}

DEFINE_PLUGIN_DETOUR(CheatMenu, bool, ZActor_YouGotHit, IBaseCharacter* th, const SHitInfo& hitInfo) {
    if (!th) {
        return { HookAction::Continue() };
    }

    ZActor* s_Actor = static_cast<ZActor*>(th);

    // Treffer der Phantom X-47 (oder mit Phantom-Munition): sofort tot oder bewusstlos.
    if (NowSeconds() - m_LastPhantomShot < 0.35 && s_Actor->IsAlive()) {
        EPhantomEffect s_Effect = static_cast<EPhantomEffect>(m_PhantomEffect);

        if (s_Effect == EPhantomEffect::Random) {
            s_Effect = (GetTickCount64() / 7) % 2 == 0 ? EPhantomEffect::Kill : EPhantomEffect::KnockOut;
        }

        {
            std::scoped_lock s_Lock(m_PendingMutex);
            m_PendingHits.push_back({ s_Actor, s_Effect, s_Actor->m_fCurrentHitPoints });
        }

        ++m_PhantomHits;

        if (s_Effect == EPhantomEffect::Kill) {
            s_Actor->m_fCurrentHitPoints = 0.f;
        }
        else {
            // Die Kugel soll nicht toeten – umfallen laesst ihn UpdatePowers im naechsten Bild.
            s_Actor->m_fCurrentHitPoints = std::max(s_Actor->m_fCurrentHitPoints, 0.f) + 100000.f;
        }

        return { HookAction::Continue() };
    }

    if (m_OneHitKill) {
        s_Actor->m_fCurrentHitPoints = 0.f;
    }

    return { HookAction::Continue() };
}

DEFINE_ZHM_PLUGIN(CheatMenu);
