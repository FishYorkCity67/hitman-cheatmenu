#pragma once

#include <array>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "IPluginInterface.h"
#include "ItemImages.h"

#include <Glacier/ZEntity.h>
#include <Glacier/ZInput.h>
#include <Glacier/ZPrimitives.h>
#include <Glacier/ZResource.h>

class ZGlobalOutfitKit;
class ZHitman5;
class ZActor;
class ZHM5ItemWeapon;

class CheatMenu : public IPluginInterface {
public:
    CheatMenu();
    ~CheatMenu() override;

    void Init() override;
    void OnEngineInitialized() override;
    void OnDrawMenu() override;
    void OnDrawUI(bool p_HasFocus) override;

private:
    enum class ETab : int {
        Home,
        Player,
        Weapons,
        Inventory,
        Powers,
        Movement,
        World,
        Outfits,
        Settings,
        Count
    };

    // Grobe Art eines Gegenstands – bestimmt Bild, Farbe und Filter im Inventar.
    enum class EItemKind : int {
        Pistol,
        Smg,
        Rifle,
        Sniper,
        Shotgun,
        Melee,
        Explosive,
        Poison,
        Distraction,
        Tool,
        Key,
        Container,
        Wire,
        Quest,
        Other,
        Count
    };

    // Was die erfundene Waffe (und der Phantom-Modus) mit getroffenen NPCs macht.
    enum class EPhantomEffect : int {
        Kill,
        KnockOut,
        Random,
        Count
    };

    // Kraefte, die auf dem Spiel-Thread ausgefuehrt werden (Knoepfe im Menue laufen auf dem Render-Thread).
    enum class EPower : int {
        KillNearby,
        KnockOutNearby,
        ForcePush,
        Launch,
        DisarmNearby,
        SummonActors,
        Heal,
    };

    struct SPositionSlot {
        bool m_IsSet = false;
        SMatrix m_Transform {};
    };

    struct SOutfitEntry {
        std::string m_Name;
        std::string m_NameLower;
        TEntityRef<ZGlobalOutfitKit> m_Kit;
    };

    struct SItemEntry {
        ZRepositoryID m_Id;
        std::string m_Name;
        std::string m_NameLower;
        std::string m_IdText;
        EItemKind m_Kind = EItemKind::Other;
        bool m_IsWeapon = false;
    };

    struct SKnockedOut {
        ZActor* m_Actor = nullptr;
        double m_Until = 0.0;      // Echtzeit; < 0 = fuer immer
        double m_NextCheck = 0.0;
        SVector3 m_DownPos {};
    };

    struct SImpulse {
        ZActor* m_Actor = nullptr;
        SVector3 m_Impulse {};
        double m_At = 0.0; // erst wenn der Ragdoll aktiv ist
    };

    struct SPendingHit {
        ZActor* m_Actor = nullptr;
        EPhantomEffect m_Effect = EPhantomEffect::Kill;
        float m_RestoreHp = -1.f; // Lebenspunkte vor dem Treffer (Bewusstlos: die Kugel soll nicht toeten)
    };

    // Game loop.
    void OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent);
    void UpdateNoclip(const SGameUpdateEvent& p_UpdateEvent);
    void UpdatePowers();
    void UpdateKnockedOut();
    void UpdateFrozen();
    void UpdateAutoHeal();

    // UI.
    void PushTheme();
    void PopTheme();
    void DrawHeader();
    void DrawSidebar();
    void DrawFooter();
    void DrawHud();

    void DrawHomeTab();
    void DrawPlayerTab();
    void DrawWeaponsTab();
    void DrawInventoryTab();
    void DrawPowersTab();
    void DrawMovementTab();
    void DrawWorldTab();
    void DrawOutfitsTab();
    void DrawSettingsTab();

    void PageTitle(const char* p_Icon, const char* p_Title, const char* p_Subtitle);
    bool BeginCard(const char* p_Id, const char* p_Caption);
    void EndCard();
    bool ToggleRow(const char* p_Icon, const char* p_Label, const char* p_Description, bool* p_Value, const char* p_Hotkey = nullptr);
    bool SliderRow(const char* p_Label, const char* p_Format, float* p_Value, float p_Min, float p_Max, ImGuiSliderFlags p_Flags = 0);
    bool ActionButton(const char* p_Label, float p_Width = 0.f, bool p_Primary = false);
    void InfoBox(const char* p_Icon, const char* p_Text, bool p_Warning);
    bool Tile(const char* p_Icon, const char* p_Label, const char* p_Sub, bool p_Active, const ImVec2& p_Size);
    bool SegmentedControl(const char* p_Id, const char* const* p_Labels, int p_Count, int* p_Value);
    bool TabHasActiveCheat(ETab p_Tab) const;

    // Inventar.
    void LoadItemCatalog();
    void GiveItem(const SItemEntry& p_Item, int p_Count);
    void SpawnItemInWorld(const ZRepositoryID& p_Id);
    void GivePhantom();
    const SItemEntry* FindPhantomBase() const;
    static EItemKind ClassifyItem(const std::string& p_Category, const std::string& p_NameLower, bool p_IsWeapon);
    static const char* KindName(EItemKind p_Kind);
    static ImVec4 KindColor(EItemKind p_Kind);
    static void DrawItemPicture(ImDrawList* p_DrawList, const ImVec2& p_Min, const ImVec2& p_Max, EItemKind p_Kind, const ImVec4& p_Color);
    bool ItemCard(const SItemEntry& p_Item, const ImVec2& p_Size);

    // Cheats.
    void SetGodMode(bool p_Enabled);
    void SetInvisible(bool p_Enabled);
    void SetNoclip(bool p_Enabled);
    void RefreshCrippleBox();
    void ApplyTimeScale();
    void DisableAll();
    int ActiveCheatCount() const;
    std::vector<const char*> ActiveCheatNames() const;
    void RequestPower(EPower p_Power);
    void RunPower(EPower p_Power);
    void HealPlayer();
    void KnockOut(ZActor* p_Actor);
    void KillActor(ZActor* p_Actor);
    bool IsLiveActor(ZActor* p_Actor) const;
    bool IsPhantomWeapon(ZHM5ItemWeapon* p_Weapon) const;
    void SetStatus(const std::string& p_Text, float p_Seconds = 2.5f);

    bool EnsureAICrippleEntity();
    bool EnsureCrippleBoxEntity();
    void DestroySpawnedEntities();

    void SavePosition(size_t p_Slot);
    void LoadPosition(size_t p_Slot);
    void TeleportForward(float p_Distance);
    void TeleportAllActorsToPlayer();
    void TeleportAllItemsToPlayer();
    void TakeNearestActorOutfit();
    void EquipOutfit(const TEntityRef<ZGlobalOutfitKit>& p_Kit, uint8_t p_Charset, uint8_t p_Variation);
    void RebuildOutfitList();

    void SaveSettings();

    DECLARE_PLUGIN_DETOUR(CheatMenu, void, OnClearScene, ZEntitySceneContext* th, bool p_FullyUnloadScene);
    DECLARE_PLUGIN_DETOUR(CheatMenu, void, ZSecuritySystemCameraManager_OnFrameUpdate, ZSecuritySystemCameraManager* th, const SGameUpdateEvent& updateEvent);
    DECLARE_PLUGIN_DETOUR(CheatMenu, void, ZSecuritySystemCamera_FrameUpdate, ZSecuritySystemCamera* th, const SGameUpdateEvent& updateEvent);
    DECLARE_PLUGIN_DETOUR(CheatMenu, void, ZHM5ItemWeapon_SetBulletsInMagazine, IFirearm* th, int32_t nBullets);
    DECLARE_PLUGIN_DETOUR(
        CheatMenu,
        void,
        ZHitmanMorphemePostProcessor_UpdateWeaponRecoil,
        ZHitmanMorphemePostProcessor* th,
        float fDeltaTime,
        const THashMap<int32_t, int32_t, TDefaultHashMapPolicy<int32_t>>& charboneMap,
        TArrayRef<int32_t> hierarchy
    );
    DECLARE_PLUGIN_DETOUR(CheatMenu, void, ZHM5WeaponRecoilController_RecoilWeapon, ZHM5WeaponRecoilController* th, const TEntityRef<ZHM5ItemWeapon>& rWeapon);
    DECLARE_PLUGIN_DETOUR(CheatMenu, bool, ZHM5ItemWeapon_FireProjectiles, ZHM5ItemWeapon* th, bool bMayStartSound);
    DECLARE_PLUGIN_DETOUR(CheatMenu, bool, ZHM5ItemWeapon_IsFiring, IFirearm* th);
    DECLARE_PLUGIN_DETOUR(CheatMenu, bool, ZActor_YouGotHit, IBaseCharacter* th, const SHitInfo& hitInfo);

    // UI state.
    bool m_ShowWindow = false;
    bool m_ShowHud = true;
    ETab m_CurrentTab = ETab::Home;
    int m_AccentIndex = 0;
    ImVec4 m_Accent {};
    char m_OutfitSearch[128] {};
    std::vector<SOutfitEntry> m_Outfits;
    std::string m_StatusMessage;
    float m_StatusTimer = 0.f;

    // Inventar.
    TResourcePtr<ZTemplateEntityFactory> m_RepositoryResource;
    std::vector<SItemEntry> m_Items;
    std::array<int, static_cast<size_t>(EItemKind::Count)> m_KindCounts {};
    char m_ItemSearch[128] {};
    int m_ItemFilter = -1; // -1 = alle
    int m_GiveCount = 1;
    bool m_GiveToWorld = false;
    std::unordered_map<std::string, double> m_GiveFlash; // Item-ID -> Zeitpunkt (Animation)
    cm::ItemImages m_Images;
    bool m_OnlineImages = true;

    // Player.
    bool m_GodMode = false;
    bool m_Invisible = false;
    bool m_CamerasDisabled = false;
    bool m_AutoHeal = false;

    // Weapons.
    bool m_InfiniteAmmo = false;
    bool m_NoReload = false;
    bool m_NoRecoil = false;
    bool m_PerfectAim = false;
    bool m_RapidFire = false;
    bool m_OneHitKill = false;
    std::unordered_map<void*, int> m_MagazineSize; // groesste gesehene Magazinfuellung je Waffe

    // Erfundene Waffe "Phantom X-47".
    ZRepositoryID m_PhantomBaseId {};
    bool m_PhantomGiven = false;
    bool m_PhantomAllWeapons = false;
    int m_PhantomEffect = static_cast<int>(EPhantomEffect::Kill);
    double m_LastPhantomShot = -1e9;
    int m_PhantomHits = 0;

    // Kraefte.
    float m_PowerRadius = 15.f;
    float m_PushStrength = 70.f;
    float m_KnockOutSeconds = 60.f; // 300 = fuer immer
    bool m_FreezeNpcs = false;
    std::unordered_map<ZActor*, SMatrix> m_FrozenTransforms;
    std::vector<SKnockedOut> m_KnockedOut;
    std::vector<SImpulse> m_Impulses;
    std::mutex m_PendingMutex;
    std::vector<EPower> m_PendingPowers;
    std::vector<SPendingHit> m_PendingHits;

    // Movement.
    bool m_Noclip = false;
    float m_NoclipSpeed = 6.f;
    float m_NoclipBoost = 4.f;
    SMatrix m_NoclipTransform {};
    std::array<SPositionSlot, 4> m_PositionSlots {};

    // World.
    bool m_TimeScaleEnabled = false;
    float m_TimeScale = 0.5f;
    bool m_TimeScaleWasApplied = false;

    // Seconds until persistent cheats are re-applied after a level load.
    float m_ReapplyTimer = -1.f;

    // Hotkeys.
    ZInputAction m_GodModeAction;
    ZInputAction m_InvisibleAction;
    ZInputAction m_NoclipAction;
    ZInputAction m_TimeScaleAction;
    ZInputAction m_PushAction;
    ZInputAction m_KnockOutAction;
    ZInputAction m_HealAction;
    ZInputAction m_ForwardAction;
    ZInputAction m_BackwardAction;
    ZInputAction m_LeftAction;
    ZInputAction m_RightAction;
    ZInputAction m_FastAction;

    // Spawned helper entities.
    ZEntityRef m_AICrippleEntity;
    ZEntityRef m_CrippleBoxEntity;
};

DECLARE_ZHM_PLUGIN(CheatMenu)
