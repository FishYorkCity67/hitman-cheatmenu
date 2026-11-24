// Inventar: alle Gegenstaende des Spiels als Karten mit Bild und Namen, per Klick ins Inventar (oder vor dich).
// Dazu die erfundene Waffe "Phantom X-47".

#include "CheatMenu.h"

#include <IconsMaterialDesign.h>

#include <Glacier/SExternalReferences.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZInventory.h>
#include <Glacier/ZItem.h>
#include <Glacier/ZModule.h>
#include <Glacier/ZSpatialEntity.h>

#include "Logging.h"
#include "Theme.h"

#include <format>
#include <unordered_set>

#undef min
#undef max

using namespace cm;

namespace {
    bool Contains(const std::string& p_Text, const char* p_Part) {
        return p_Text.find(p_Part) != std::string::npos;
    }

    // Bild so einpassen, dass die Flaeche voll ist (Rest wird abgeschnitten); p_Zoom > 1 = naeher heran.
    void DrawImageCover(ImDrawList* p_DrawList, const ImGuiTexture& p_Image, const ImVec2& p_Min, const ImVec2& p_Max, float p_Zoom, float p_Rounding) {
        const float s_Width = p_Max.x - p_Min.x;
        const float s_Height = p_Max.y - p_Min.y;
        const float s_ImageWidth = static_cast<float>(std::max<UINT>(1, p_Image.width));
        const float s_ImageHeight = static_cast<float>(std::max<UINT>(1, p_Image.height));
        const float s_Scale = std::max(s_Width / s_ImageWidth, s_Height / s_ImageHeight) * std::max(1.f, p_Zoom);
        const float s_U = std::min(1.f, s_Width / (s_ImageWidth * s_Scale));
        const float s_V = std::min(1.f, s_Height / (s_ImageHeight * s_Scale));
        const ImVec2 s_Uv0 = { (1.f - s_U) * 0.5f, (1.f - s_V) * 0.5f };
        const ImVec2 s_Uv1 = { s_Uv0.x + s_U, s_Uv0.y + s_V };

        p_DrawList->AddImageRounded(ImTextureRef(p_Image.id), p_Min, p_Max, s_Uv0, s_Uv1, IM_COL32_WHITE, p_Rounding);
    }

    bool ContainsAny(const std::string& p_Text, std::initializer_list<const char*> p_Parts) {
        for (const char* s_Part : p_Parts) {
            if (Contains(p_Text, s_Part)) {
                return true;
            }
        }

        return false;
    }


}

// ---------------------------------------------------------------------------------------------------------------------
// Katalog laden
// ---------------------------------------------------------------------------------------------------------------------

CheatMenu::EItemKind CheatMenu::ClassifyItem(const std::string& p_Category, const std::string& p_NameLower, bool p_IsWeapon) {
    const std::string& c = p_Category;
    const std::string& n = p_NameLower;

    if (Contains(c, "sniper")) return EItemKind::Sniper;
    if (Contains(c, "shotgun")) return EItemKind::Shotgun;
    if (Contains(c, "smg")) return EItemKind::Smg;
    if (Contains(c, "assault") || Contains(c, "rifle")) return EItemKind::Rifle;
    if (Contains(c, "pistol") || c == "gun") return EItemKind::Pistol;
    if (Contains(c, "melee")) return ContainsAny(n, { "fiber", "wire", "garrot" }) ? EItemKind::Wire : EItemKind::Melee;
    if (Contains(c, "explosive")) return EItemKind::Explosive;
    if (Contains(c, "poison")) return EItemKind::Poison;
    if (Contains(c, "distraction")) return EItemKind::Distraction;
    if (Contains(c, "key")) return EItemKind::Key;
    if (Contains(c, "tool")) return EItemKind::Tool;
    if (Contains(c, "container") || Contains(c, "suitcase")) return EItemKind::Container;
    if (Contains(c, "quest")) return EItemKind::Quest;

    // Ohne brauchbare Kategorie: am Namen erkennen.
    if (ContainsAny(n, { "fiber wire", "garrot", "wire" })) return EItemKind::Wire;
    if (ContainsAny(n, { "knife", "blade", "sword", "katana", "machete", "axe", "cleaver", "scissors", "saber", "dagger" })) return EItemKind::Melee;
    if (ContainsAny(n, { "poison", "emetic", "sedative", "lethal pill", "syringe", "toxin" })) return EItemKind::Poison;
    if (ContainsAny(n, { "explosive", "grenade", "bomb", "c4", "mine", "detonator" })) return EItemKind::Explosive;
    if (ContainsAny(n, { "coin" })) return EItemKind::Distraction;
    if (ContainsAny(n, { "keycard", "key " })) return EItemKind::Key;
    if (ContainsAny(n, { "briefcase", "suitcase" })) return EItemKind::Container;
    if (ContainsAny(n, { "wrench", "hammer", "crowbar", "lockpick", "screwdriver", "kit" })) return EItemKind::Tool;
    if (ContainsAny(n, { "sniper" })) return EItemKind::Sniper;
    if (ContainsAny(n, { "shotgun" })) return EItemKind::Shotgun;
    if (ContainsAny(n, { "rifle", "carbine" })) return EItemKind::Rifle;
    if (ContainsAny(n, { "smg" })) return EItemKind::Smg;

    return p_IsWeapon ? EItemKind::Pistol : EItemKind::Other;
}

const char* CheatMenu::KindName(EItemKind p_Kind) {
    switch (p_Kind) {
        case EItemKind::Pistol: return "Pistolen";
        case EItemKind::Smg: return "Maschinenpistolen";
        case EItemKind::Rifle: return "Sturmgewehre";
        case EItemKind::Sniper: return "Scharfsch\xC3\xBCtzengewehre";
        case EItemKind::Shotgun: return "Schrotflinten";
        case EItemKind::Melee: return "Nahkampf";
        case EItemKind::Explosive: return "Sprengstoff";
        case EItemKind::Poison: return "Gift";
        case EItemKind::Distraction: return "Ablenkung";
        case EItemKind::Tool: return "Werkzeug";
        case EItemKind::Key: return "Schl\xC3\xBCssel";
        case EItemKind::Container: return "Koffer";
        case EItemKind::Wire: return "Drahtschlingen";
        case EItemKind::Quest: return "Missionsgegenst\xC3\xA4nde";
        default: return "Sonstiges";
    }
}

ImVec4 CheatMenu::KindColor(EItemKind p_Kind) {
    switch (p_Kind) {
        case EItemKind::Pistol: return { 0.90f, 0.42f, 0.42f, 1.f };
        case EItemKind::Smg: return { 0.93f, 0.58f, 0.36f, 1.f };
        case EItemKind::Rifle: return { 0.86f, 0.70f, 0.36f, 1.f };
        case EItemKind::Sniper: return { 0.64f, 0.56f, 0.95f, 1.f };
        case EItemKind::Shotgun: return { 0.95f, 0.50f, 0.58f, 1.f };
        case EItemKind::Melee: return { 0.74f, 0.80f, 0.88f, 1.f };
        case EItemKind::Explosive: return { 0.96f, 0.60f, 0.26f, 1.f };
        case EItemKind::Poison: return { 0.40f, 0.85f, 0.56f, 1.f };
        case EItemKind::Distraction: return { 0.95f, 0.80f, 0.36f, 1.f };
        case EItemKind::Tool: return { 0.55f, 0.72f, 0.92f, 1.f };
        case EItemKind::Key: return { 0.96f, 0.86f, 0.48f, 1.f };
        case EItemKind::Container: return { 0.74f, 0.58f, 0.44f, 1.f };
        case EItemKind::Wire: return { 0.78f, 0.78f, 0.80f, 1.f };
        case EItemKind::Quest: return { 0.84f, 0.56f, 0.95f, 1.f };
        default: return { 0.60f, 0.66f, 0.74f, 1.f };
    }
}

void CheatMenu::LoadItemCatalog() {
    m_Items.clear();
    m_KindCounts.fill(0);

    if (!SDK()->GetLocalPlayer()) {
        return;
    }

    if (m_RepositoryResource.m_nResourceIndex.val == -1) {
        const auto s_Id = ResId<"[assembly:/repository/pro.repo].pc_repo">;
        Globals::ResourceManager->LoadResource(m_RepositoryResource, s_Id);
    }

    if (m_RepositoryResource.GetResourceInfo().status != RESOURCE_STATUS_VALID) {
        return;
    }

    const auto s_Repository = static_cast<THashMap<ZRepositoryID, ZDynamicObject, TDefaultHashMapPolicy<ZRepositoryID>>*>(
        m_RepositoryResource.GetResourceData()
    );

    if (!s_Repository) {
        return;
    }

    std::unordered_set<std::string> s_Seen;

    for (const auto& [s_RepositoryId, s_Object] : *s_Repository) {
        const TArray<SDynamicObjectKeyValuePair>* s_Entries = s_Object.As<TArray<SDynamicObjectKeyValuePair>>();

        if (!s_Entries) {
            continue;
        }

        ZString s_Id, s_Title, s_CommonName, s_Name, s_Category;
        bool s_IsItem = false;
        bool s_IsWeapon = false;

        for (const auto& s_Entry : *s_Entries) {
            if (s_Entry.sKey == "ID_") s_Id = *s_Entry.value.As<ZString>();
            else if (s_Entry.sKey == "Title") s_Title = *s_Entry.value.As<ZString>();
            else if (s_Entry.sKey == "CommonName") s_CommonName = *s_Entry.value.As<ZString>();
            else if (s_Entry.sKey == "Name") s_Name = *s_Entry.value.As<ZString>();
            else if (s_Entry.sKey == "InventoryCategoryIcon") s_Category = *s_Entry.value.As<ZString>();
            else if (s_Entry.sKey == "ItemType") s_IsItem = true;
            else if (s_Entry.sKey == "PrimaryConfiguration") s_IsWeapon = true;
        }

        if (s_Id.IsEmpty() || !s_IsItem) {
            continue;
        }

        const std::string s_CategoryText = ToLower(std::string(s_Category.c_str(), s_Category.size()));

        if (s_CategoryText == "evergreen_payout") {
            continue;
        }

        // Lesbarsten Namen nehmen; Lokalisierungs-Schluessel ("UI_...") sind keine Namen.
        std::string s_Display;

        for (const ZString* s_Candidate : { &s_Title, &s_CommonName, &s_Name }) {
            if (s_Candidate->IsEmpty()) {
                continue;
            }

            std::string s_Text(s_Candidate->c_str(), s_Candidate->size());

            if (s_Text.rfind("UI_", 0) == 0 || s_Text.rfind("LOC_", 0) == 0) {
                continue;
            }

            s_Display = std::move(s_Text);
            break;
        }

        if (s_Display.empty()) {
            continue;
        }

        SItemEntry s_Item;
        s_Item.m_Id = s_RepositoryId;
        s_Item.m_Name = s_Display;
        s_Item.m_NameLower = ToLower(s_Display);
        s_Item.m_IdText = std::string(s_Id.c_str(), s_Id.size());
        s_Item.m_IsWeapon = s_IsWeapon;
        s_Item.m_Kind = ClassifyItem(s_CategoryText, s_Item.m_NameLower, s_IsWeapon);

        // Viele Gegenstaende gibt es in mehreren Varianten mit gleichem Namen – einer reicht.
        if (!s_Seen.insert(s_Item.m_NameLower + "#" + std::to_string(static_cast<int>(s_Item.m_Kind))).second) {
            continue;
        }

        ++m_KindCounts[static_cast<size_t>(s_Item.m_Kind)];
        m_Items.push_back(std::move(s_Item));
    }

    std::sort(m_Items.begin(), m_Items.end(), [](const SItemEntry& a, const SItemEntry& b) {
        if (a.m_Kind != b.m_Kind) {
            return a.m_Kind < b.m_Kind;
        }

        return a.m_NameLower < b.m_NameLower;
    });

    Logger::Info("[CheatMenu] {} Gegenstaende im Katalog.", m_Items.size());
}

// ---------------------------------------------------------------------------------------------------------------------
// Geben
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::GiveItem(const SItemEntry& p_Item, int p_Count) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();

    if (!s_LocalHitman) {
        SetStatus("Lade zuerst eine Mission");
        return;
    }

    for (int i = 0; i < p_Count; ++i) {
        if (m_GiveToWorld) {
            SpawnItemInWorld(p_Item.m_Id);
            continue;
        }

        const auto s_Character = s_LocalHitman.m_pInterfaceRef->m_pCharacter.m_pInterfaceRef;

        if (!s_Character || !s_Character->m_rSubcontrollerContainer.m_pInterfaceRef) {
            return;
        }

        const auto s_Controllers = &s_Character->m_rSubcontrollerContainer.m_pInterfaceRef->m_aReferencedControllers;

        if (s_Controllers->size() <= 6) {
            return;
        }

        const auto s_Inventory = static_cast<ZCharacterSubcontrollerInventory*>((*s_Controllers)[6].m_pInterfaceRef);

        Functions::ZCharacterSubcontrollerInventory_CreateItem->Call(
            s_Inventory, p_Item.m_Id, "", {}, ZCharacterSubcontrollerInventory::ECreateItemType::ECIT_ContractItem
        );
    }

    m_GiveFlash[p_Item.m_IdText] = NowSeconds();
    SetStatus(
        (p_Count > 1 ? std::to_string(p_Count) + "x " : std::string()) + p_Item.m_Name +
        (m_GiveToWorld ? " liegt vor dir" : " ist im Inventar")
    );
}

void CheatMenu::SpawnItemInWorld(const ZRepositoryID& p_Id) {
    const auto s_LocalHitman = SDK()->GetLocalPlayer();
    const auto s_Scene = Globals::Hitman5Module->m_pEntitySceneContext->m_pScene;

    if (!s_LocalHitman || !s_Scene) {
        return;
    }

    TResourcePtr<ZTemplateEntityFactory> s_SpawnerFactory, s_KeyFactory;
    Globals::ResourceManager->GetResourcePtr(s_SpawnerFactory, ResId<"[modules:/zitemspawner.class].pc_entitytype">, 0);
    Globals::ResourceManager->GetResourcePtr(s_KeyFactory, ResId<"[modules:/zitemrepositorykeyentity.class].pc_entitytype">, 0);

    if (!s_SpawnerFactory || !s_KeyFactory) {
        SetStatus("Ablegen geht in diesem Level nicht \xE2\x80\x93 nimm \"Ins Inventar\"");
        return;
    }

    ZEntityRef s_Spawner, s_Key;
    SExternalReferences s_ExternalRefs;
    Functions::ZEntityManager_NewEntity->Call(Globals::EntityManager, s_Spawner, "", s_SpawnerFactory, s_Scene.m_entityRef, s_ExternalRefs, -1);
    Functions::ZEntityManager_NewEntity->Call(Globals::EntityManager, s_Key, "", s_KeyFactory, s_Scene.m_entityRef, s_ExternalRefs, -1);

    if (!s_Spawner || !s_Key) {
        return;
    }

    const auto s_HitmanSpatial = s_LocalHitman.m_entityRef.QueryInterface<ZSpatialEntity>();
    const auto s_ItemSpawner = s_Spawner.QueryInterface<ZItemSpawner>();
    const auto s_RepositoryKey = s_Key.QueryInterface<ZItemRepositoryKeyEntity>();

    if (!s_HitmanSpatial || !s_ItemSpawner || !s_RepositoryKey) {
        return;
    }

    s_ItemSpawner->m_ePhysicsMode = ZItemSpawner::EPhysicsMode::EPM_KINEMATIC;
    s_ItemSpawner->m_rMainItemKey.m_entityRef = s_Key;
    s_ItemSpawner->m_rMainItemKey.m_pInterfaceRef = s_RepositoryKey;
    s_RepositoryKey->m_RepositoryId = p_Id;
    s_ItemSpawner->m_bUsePlacementAttach = false;

    s_ItemSpawner->SetObjectToWorldMatrixFromEditor(s_HitmanSpatial->GetObjectToWorldMatrix());

    Functions::ZItemSpawner_RequestContentLoad->Call(s_ItemSpawner);
}

const CheatMenu::SItemEntry* CheatMenu::FindPhantomBase() const {
    // Moeglichst eine Pistole, die 47 sonst nicht dabei hat – sonst wuerde auch seine normale Waffe zur Phantom.
    static const char* const s_Preferred[] = { "bartoli", "krugermeier", "el matador", "striker", "custom", "hackl" };

    for (const char* s_Name : s_Preferred) {
        for (const auto& s_Item : m_Items) {
            if (s_Item.m_Kind == EItemKind::Pistol && Contains(s_Item.m_NameLower, s_Name)) {
                return &s_Item;
            }
        }
    }

    for (const auto& s_Item : m_Items) {
        if (s_Item.m_Kind == EItemKind::Pistol && s_Item.m_IsWeapon && !Contains(s_Item.m_NameLower, "silverballer")) {
            return &s_Item;
        }
    }

    for (const auto& s_Item : m_Items) {
        if (s_Item.m_Kind == EItemKind::Pistol) {
            return &s_Item;
        }
    }

    return nullptr;
}

void CheatMenu::GivePhantom() {
    if (m_Items.empty()) {
        LoadItemCatalog();
    }

    const SItemEntry* s_Base = FindPhantomBase();

    if (!s_Base) {
        SetStatus("Lade zuerst eine Mission");
        return;
    }

    const bool s_ToWorld = m_GiveToWorld;
    m_GiveToWorld = false;
    GiveItem(*s_Base, 1);
    m_GiveToWorld = s_ToWorld;

    m_PhantomBaseId = s_Base->m_Id;
    m_PhantomGiven = true;
    m_GiveFlash["phantom"] = NowSeconds();
    SetStatus("Phantom X-47 ist im Inventar (sieht aus wie: " + s_Base->m_Name + ")", 4.f);
}

bool CheatMenu::IsPhantomWeapon(ZHM5ItemWeapon* p_Weapon) const {
    if (!m_PhantomGiven || !p_Weapon || !p_Weapon->m_pItemConfigDescriptor) {
        return false;
    }

    return p_Weapon->m_pItemConfigDescriptor->m_ItemID == m_PhantomBaseId;
}

// ---------------------------------------------------------------------------------------------------------------------
// Bilder: jede Art von Gegenstand als gezeichnete Silhouette
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::DrawItemPicture(ImDrawList* d, const ImVec2& p_Min, const ImVec2& p_Max, EItemKind p_Kind, const ImVec4& p_Color) {
    const ImVec2 c { (p_Min.x + p_Max.x) * 0.5f, (p_Min.y + p_Max.y) * 0.5f };
    const float s = std::min(p_Max.x - p_Min.x, p_Max.y - p_Min.y) * 0.5f;
    const ImU32 col = Col(p_Color);
    const ImU32 dark = Col(Mix(p_Color, { 0.f, 0.f, 0.f, 1.f }, 0.45f));
    const ImU32 light = Col(Mix(p_Color, { 1.f, 1.f, 1.f, 1.f }, 0.45f));

    const auto P = [&](float x, float y) { return ImVec2 { c.x + x * s, c.y + y * s }; };
    const auto R = [&](float x0, float y0, float x1, float y1, float r, ImU32 cl) { d->AddRectFilled(P(x0, y0), P(x1, y1), cl, r * s); };
    const auto Poly = [&](std::initializer_list<ImVec2> pts, ImU32 cl) {
        ImVec2 s_Points[16];
        int n = 0;

        for (const ImVec2& p : pts) {
            if (n < 16) s_Points[n++] = P(p.x, p.y);
        }

        d->AddConcavePolyFilled(s_Points, n, cl);
    };
    const auto Circle = [&](float x, float y, float r, ImU32 cl) { d->AddCircleFilled(P(x, y), r * s, cl, 32); };
    const auto Ring = [&](float x, float y, float r, float t, ImU32 cl) { d->AddCircle(P(x, y), r * s, cl, 32, t * s); };
    const auto Line = [&](float x0, float y0, float x1, float y1, float t, ImU32 cl) { d->AddLine(P(x0, y0), P(x1, y1), cl, t * s); };

    switch (p_Kind) {
        case EItemKind::Pistol:
            Poly({ { -0.62f, -0.06f }, { -0.24f, -0.06f }, { -0.1f, 0.62f }, { -0.5f, 0.66f } }, col);
            R(-0.6f, -0.14f, 0.45f, 0.02f, 0.03f, col);
            R(-0.8f, -0.44f, 0.62f, -0.12f, 0.05f, col);
            R(0.62f, -0.37f, 0.74f, -0.19f, 0.02f, dark);
            R(-0.74f, -0.52f, -0.62f, -0.44f, 0.02f, col);
            for (int i = 0; i < 4; ++i) Line(-0.68f + i * 0.07f, -0.4f, -0.68f + i * 0.07f, -0.17f, 0.025f, dark);
            Ring(0.02f, 0.1f, 0.14f, 0.06f, col);
            Line(0.0f, 0.02f, -0.03f, 0.16f, 0.05f, dark);
            break;

        case EItemKind::Smg:
            R(-0.6f, -0.32f, 0.55f, 0.04f, 0.04f, col);
            R(0.55f, -0.22f, 0.88f, -0.11f, 0.02f, dark);
            Line(-0.6f, -0.24f, -0.95f, -0.18f, 0.06f, col);
            Line(-0.6f, -0.02f, -0.95f, 0.02f, 0.06f, col);
            Line(-0.95f, -0.2f, -0.95f, 0.04f, 0.07f, col);
            Poly({ { -0.32f, 0.02f }, { -0.12f, 0.02f }, { -0.05f, 0.46f }, { -0.26f, 0.48f } }, col);
            R(0.1f, 0.02f, 0.26f, 0.66f, 0.03f, dark);
            R(-0.2f, -0.42f, 0.2f, -0.32f, 0.02f, col);
            break;

        case EItemKind::Rifle:
            Poly({ { -0.45f, -0.2f }, { -0.98f, -0.12f }, { -0.98f, 0.2f }, { -0.45f, 0.05f } }, col);
            R(-0.46f, -0.24f, 0.36f, 0.06f, 0.03f, col);
            R(0.36f, -0.21f, 0.76f, 0.01f, 0.03f, dark);
            R(0.76f, -0.14f, 0.99f, -0.07f, 0.01f, col);
            R(-0.26f, -0.36f, 0.1f, -0.24f, 0.03f, col);
            Poly({ { -0.3f, 0.06f }, { -0.15f, 0.06f }, { -0.1f, 0.4f }, { -0.26f, 0.42f } }, col);
            Poly({ { 0.04f, 0.06f }, { 0.22f, 0.06f }, { 0.32f, 0.52f }, { 0.13f, 0.56f } }, dark);
            break;

        case EItemKind::Sniper:
            Poly({ { -0.99f, -0.1f }, { -0.5f, -0.13f }, { -0.5f, 0.08f }, { -0.99f, 0.24f } }, col);
            R(-0.51f, -0.14f, 0.26f, 0.07f, 0.03f, col);
            R(0.26f, -0.09f, 0.99f, -0.02f, 0.01f, col);
            R(-0.3f, -0.39f, 0.2f, -0.23f, 0.06f, dark);
            R(-0.36f, -0.43f, -0.26f, -0.19f, 0.03f, col);
            R(0.15f, -0.44f, 0.26f, -0.18f, 0.03f, col);
            R(-0.16f, -0.23f, -0.1f, -0.14f, 0.f, col);
            R(0.04f, -0.23f, 0.1f, -0.14f, 0.f, col);
            Poly({ { -0.42f, 0.07f }, { -0.3f, 0.07f }, { -0.26f, 0.34f }, { -0.38f, 0.36f } }, col);
            Line(0.6f, -0.03f, 0.48f, 0.38f, 0.04f, dark);
            Line(0.6f, -0.03f, 0.72f, 0.38f, 0.04f, dark);
            break;

        case EItemKind::Shotgun:
            Poly({ { -0.99f, -0.08f }, { -0.5f, -0.15f }, { -0.5f, 0.06f }, { -0.99f, 0.26f } }, col);
            R(-0.51f, -0.17f, -0.04f, 0.07f, 0.03f, col);
            R(-0.04f, -0.15f, 0.99f, -0.05f, 0.02f, col);
            R(-0.04f, -0.04f, 0.78f, 0.04f, 0.02f, dark);
            R(0.18f, -0.08f, 0.55f, 0.1f, 0.04f, col);
            for (int i = 0; i < 4; ++i) Line(0.23f + i * 0.08f, -0.05f, 0.23f + i * 0.08f, 0.08f, 0.025f, dark);
            Ring(-0.28f, 0.14f, 0.12f, 0.05f, col);
            break;

        case EItemKind::Melee:
            R(-0.92f, -0.1f, -0.36f, 0.12f, 0.06f, dark);
            for (int i = 0; i < 3; ++i) Line(-0.8f + i * 0.14f, -0.08f, -0.8f + i * 0.14f, 0.1f, 0.03f, Col(Alpha(p_Color, 0.6f)));
            Circle(-0.93f, 0.01f, 0.08f, col);
            R(-0.38f, -0.22f, -0.29f, 0.24f, 0.03f, col);
            Poly({ { -0.29f, -0.12f }, { 0.55f, -0.15f }, { 0.94f, -0.02f }, { 0.5f, 0.1f }, { -0.29f, 0.1f } }, col);
            Line(-0.26f, 0.04f, 0.52f, 0.04f, 0.025f, light);
            break;

        case EItemKind::Explosive:
            Circle(-0.05f, 0.18f, 0.55f, col);
            Circle(-0.26f, -0.02f, 0.11f, Col(Alpha({ 1.f, 1.f, 1.f, 1.f }, 0.35f)));
            R(-0.21f, -0.5f, 0.11f, -0.3f, 0.04f, dark);
            d->AddBezierCubic(P(-0.05f, -0.5f), P(0.0f, -0.72f), P(0.25f, -0.62f), P(0.38f, -0.8f), col, 0.05f * s);
            for (int i = 0; i < 6; ++i) {
                const float a = i * 1.047f;
                Line(0.42f, -0.84f, 0.42f + std::cos(a) * 0.14f, -0.84f + std::sin(a) * 0.14f, 0.035f, Col(c_Warning));
            }
            break;

        case EItemKind::Poison:
            R(-0.42f, -0.36f, 0.42f, 0.8f, 0.24f, Col(Alpha(p_Color, 0.22f)));
            d->AddRectFilled(P(-0.42f, 0.12f), P(0.42f, 0.8f), col, 0.24f * s, ImDrawFlags_RoundCornersBottom);
            d->AddRect(P(-0.42f, -0.36f), P(0.42f, 0.8f), col, 0.24f * s, 0, 0.05f * s);
            R(-0.13f, -0.64f, 0.13f, -0.34f, 0.02f, col);
            R(-0.18f, -0.84f, 0.18f, -0.64f, 0.05f, dark);
            Circle(-0.12f, 0.4f, 0.07f, light);
            Circle(0.14f, 0.58f, 0.05f, light);
            Circle(0.05f, 0.28f, 0.04f, light);
            break;

        case EItemKind::Distraction:
            Circle(0.24f, 0.2f, 0.5f, dark);
            Circle(-0.12f, -0.08f, 0.55f, col);
            Ring(-0.12f, -0.08f, 0.4f, 0.05f, dark);
            Poly({ { -0.12f, -0.3f }, { 0.06f, -0.08f }, { -0.12f, 0.14f }, { -0.3f, -0.08f } }, dark);
            break;

        case EItemKind::Tool: {
            Poly({ { -0.72f, 0.56f }, { -0.56f, 0.72f }, { 0.22f, -0.06f }, { 0.06f, -0.22f } }, col);
            d->PathArcTo(P(0.34f, -0.34f), 0.24f * s, -0.785f + 0.75f, -0.785f + 6.283f - 0.75f, 24);
            d->PathStroke(col, 0, 0.16f * s);
            Ring(-0.62f, 0.62f, 0.09f, 0.04f, dark);
            break;
        }

        case EItemKind::Key:
            Ring(-0.48f, 0.f, 0.27f, 0.12f, col);
            R(-0.24f, -0.07f, 0.86f, 0.07f, 0.03f, col);
            R(0.44f, 0.06f, 0.55f, 0.3f, 0.02f, col);
            R(0.66f, 0.06f, 0.77f, 0.22f, 0.02f, col);
            break;

        case EItemKind::Container:
            d->AddRect(P(-0.25f, -0.58f), P(0.25f, -0.3f), col, 0.08f * s, 0, 0.08f * s);
            R(-0.86f, -0.33f, 0.86f, 0.62f, 0.1f, col);
            Line(-0.86f, -0.04f, 0.86f, -0.04f, 0.04f, dark);
            R(-0.52f, -0.13f, -0.36f, 0.03f, 0.02f, dark);
            R(0.36f, -0.13f, 0.52f, 0.03f, 0.02f, dark);
            break;

        case EItemKind::Wire:
            R(-0.92f, -0.16f, -0.58f, 0.16f, 0.08f, col);
            R(0.58f, -0.16f, 0.92f, 0.16f, 0.08f, col);
            d->AddBezierCubic(P(-0.58f, 0.f), P(-0.2f, 0.75f), P(0.2f, -0.75f), P(0.58f, 0.f), light, 0.035f * s);
            break;

        case EItemKind::Quest: {
            ImVec2 s_Star[10];

            for (int i = 0; i < 10; ++i) {
                const float a = -1.5708f + i * 0.6283f;
                const float r = (i % 2 == 0 ? 0.72f : 0.3f) * s;
                s_Star[i] = { c.x + std::cos(a) * r, c.y + 0.05f * s + std::sin(a) * r };
            }

            d->AddConcavePolyFilled(s_Star, 10, col);
            break;
        }

        default:
            // Paket / Kiste.
            Poly({ { -0.6f, -0.2f }, { 0.f, -0.5f }, { 0.6f, -0.2f }, { 0.f, 0.1f } }, light);
            Poly({ { -0.6f, -0.2f }, { 0.f, 0.1f }, { 0.f, 0.75f }, { -0.6f, 0.45f } }, col);
            Poly({ { 0.f, 0.1f }, { 0.6f, -0.2f }, { 0.6f, 0.45f }, { 0.f, 0.75f } }, dark);
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Oberflaeche
// ---------------------------------------------------------------------------------------------------------------------

bool CheatMenu::ItemCard(const SItemEntry& p_Item, const ImVec2& p_Size) {
    ImGui::PushID(p_Item.m_IdText.c_str());

    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const bool s_Clicked = ImGui::InvisibleButton("##Card", p_Size);
    const bool s_Hovered = ImGui::IsItemHovered();
    const float s_Hover = Animate(ImGui::GetItemID(), s_Hovered ? 1.f : 0.f, 16.f);
    const ImVec4 s_KindColor = KindColor(p_Item.m_Kind);
    const float s_Base = ImGui::GetFontSize();

    // Karte, leicht angehoben beim Ueberfahren.
    const float s_Lift = s_Hover * 3.f;
    const ImVec2 s_Min = Offset(s_Pos, 0.f, -s_Lift);
    const ImVec2 s_Max = Offset(s_Pos, p_Size.x, p_Size.y - s_Lift);
    d->AddRectFilled(Offset(s_Min, 0.f, 4.f), Offset(s_Max, 0.f, 4.f), IM_COL32(0, 0, 0, static_cast<int>(40 + 50 * s_Hover)), 12.f);
    d->AddRectFilled(s_Min, s_Max, Col(Mix(c_CardBg, c_Raised, 0.5f * s_Hover)), 12.f);
    d->AddRect(s_Min, s_Max, Col(Mix(c_Border, s_KindColor, 0.6f * s_Hover)), 12.f, 0, 1.f);

    // Bildflaeche.
    const float s_PicHeight = p_Size.y * 0.58f;
    const ImVec2 s_PicMin = Offset(s_Min, 8.f, 8.f);
    const ImVec2 s_PicMax = { s_Max.x - 8.f, s_Min.y + s_PicHeight };
    // Echtes Bild, sobald es geladen ist – bis dahin (oder wenn es keins gibt) die gezeichnete Silhouette.
    cm::ItemImages::EState s_ImageState = cm::ItemImages::EState::Missing;
    const ImGuiTexture* s_Image = m_Images.Get(p_Item.m_IdText, &s_ImageState); // ohne Internet: nur gespeicherte Bilder

    if (s_Image) {
        DrawImageCover(d, *s_Image, s_PicMin, s_PicMax, 1.12f + 0.08f * s_Hover, 9.f);
        d->AddRect(s_PicMin, s_PicMax, Col(Alpha(s_KindColor, 0.25f + 0.35f * s_Hover)), 9.f, 0, 1.f);
    }
    else {
        d->AddRectFilled(s_PicMin, s_PicMax, Col(Alpha(s_KindColor, 0.10f + 0.06f * s_Hover)), 9.f);
        d->AddRectFilledMultiColor(
            { s_PicMin.x, (s_PicMin.y + s_PicMax.y) * 0.5f }, s_PicMax,
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 40), IM_COL32(0, 0, 0, 40)
        );

        const float s_Pad = 14.f - 3.f * s_Hover;
        const bool s_Loading = s_ImageState == cm::ItemImages::EState::Loading || s_ImageState == cm::ItemImages::EState::Ready;
        DrawItemPicture(
            d, Offset(s_PicMin, s_Pad, s_Pad), { s_PicMax.x - s_Pad, s_PicMax.y - s_Pad }, p_Item.m_Kind,
            Alpha(Mix(s_KindColor, c_Text, 0.15f), s_Loading ? 0.35f : 1.f)
        );
    }

    // Name (bis zu zwei Zeilen) und Art.
    const ImVec2 s_NamePos = { s_Min.x + 10.f, s_PicMax.y + 8.f };
    const float s_NameWidth = p_Size.x - 20.f;
    const ImVec4 s_Clip = { s_NamePos.x, s_NamePos.y, s_NamePos.x + s_NameWidth, s_NamePos.y + s_Base * 2.3f };
    d->AddText(
        SDK()->GetImGuiMediumFont(), s_Base * 0.92f, s_NamePos, Col(c_Text), p_Item.m_Name.c_str(), nullptr, s_NameWidth, &s_Clip
    );

    const ImVec2 s_KindPos = { s_Min.x + 10.f, s_Max.y - s_Base * 0.8f - 10.f };
    d->AddCircleFilled(Offset(s_KindPos, 3.f, s_Base * 0.4f), 3.f, Col(s_KindColor));
    Text(d, SDK()->GetImGuiRegularFont(), s_Base * 0.78f, Offset(s_KindPos, 11.f, 0.f), c_Muted, KindName(p_Item.m_Kind));

    // "+" zum Geben beim Ueberfahren.
    if (s_Hover > 0.01f) {
        const ImVec2 s_Plus = { s_Max.x - 22.f, s_Max.y - 22.f };
        d->AddCircleFilled(s_Plus, 13.f * s_Hover, Col(m_Accent), 24);
        const ImVec2 s_PlusSize = Measure(nullptr, s_Base, ICON_MD_ADD);
        Text(d, nullptr, s_Base, { s_Plus.x - s_PlusSize.x * 0.5f, s_Plus.y - s_PlusSize.y * 0.5f }, Alpha({ 1.f, 1.f, 1.f, 1.f }, s_Hover), ICON_MD_ADD);
    }

    // Kurzes Aufleuchten nach dem Geben.
    if (const auto s_It = m_GiveFlash.find(p_Item.m_IdText); s_It != m_GiveFlash.end()) {
        const float s_Age = static_cast<float>(NowSeconds() - s_It->second);

        if (s_Age < 0.9f) {
            const float s_T = 1.f - s_Age / 0.9f;
            d->AddRect(s_Min, s_Max, Col(Alpha(m_Accent, s_T)), 12.f, 0, 2.5f);
            const char* s_Label = ICON_MD_CHECK " Gegeben";
            const ImVec2 s_LabelSize = Measure(SDK()->GetImGuiBoldFont(), s_Base * 0.86f, s_Label);
            Text(
                d, SDK()->GetImGuiBoldFont(), s_Base * 0.86f,
                { (s_Min.x + s_Max.x - s_LabelSize.x) * 0.5f, s_PicMin.y + 6.f - (1.f - s_T) * 10.f },
                Alpha(c_Text, s_T), s_Label
            );
        }
    }

    if (s_Hovered) {
        ImGui::SetTooltip("%s\n%s\nKlicken: %s", p_Item.m_Name.c_str(), p_Item.m_IdText.c_str(), m_GiveToWorld ? "vor dir ablegen" : "ins Inventar");
    }

    ImGui::PopID();
    return s_Clicked;
}

void CheatMenu::DrawInventoryTab() {
    PageTitle(ICON_MD_BACKPACK, "Inventar", "Alle Gegenst\xC3\xA4nde des Spiels \xE2\x80\x93 anklicken und du hast sie.");

    if (m_Items.empty() && SDK()->GetLocalPlayer()) {
        LoadItemCatalog();
    }

    ImDrawList* d = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    const float s_Width = ImGui::GetContentRegionAvail().x;

    // --- Die erfundene Waffe ----------------------------------------------------------------------------------------
    {
        const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
        constexpr float s_Height = 168.f;
        const ImVec2 s_Max = Offset(s_Pos, s_Width, s_Height);

        d->AddRectFilled(s_Pos, s_Max, Col(c_CardBg), 14.f);
        d->AddRectFilledMultiColor(
            Offset(s_Pos, 1.f, 1.f), Offset(s_Pos, s_Width * 0.42f, s_Height - 1.f),
            Col(Alpha(m_Accent, 0.22f)), Col(Alpha(m_Accent, 0.0f)), Col(Alpha(m_Accent, 0.0f)), Col(Alpha(m_Accent, 0.14f))
        );
        d->AddRect(s_Pos, s_Max, Col(Alpha(m_Accent, 0.45f)), 14.f, 0, 1.f);

        // Dekorative Ringe hinter der Waffe (ruhig, kein Leuchten).
        const ImVec2 s_Center = Offset(s_Pos, s_Width * 0.18f, s_Height * 0.5f);
        const float s_Time = static_cast<float>(ImGui::GetTime());

        for (int i = 0; i < 3; ++i) {
            const float s_Phase = std::fmod(s_Time * 0.25f + i / 3.f, 1.f);
            d->AddCircle(s_Center, 30.f + s_Phase * 60.f, Col(Alpha(m_Accent, 0.22f * (1.f - s_Phase))), 48, 1.5f);
        }

        const SItemEntry* s_BaseItem = FindPhantomBase();
        const ImGuiTexture* s_BaseImage = s_BaseItem ? m_Images.Get(s_BaseItem->m_IdText) : nullptr;

        if (s_BaseImage) {
            const ImVec2 s_ImgMin = Offset(s_Center, -100.f, -70.f);
            const ImVec2 s_ImgMax = Offset(s_Center, 100.f, 70.f);
            DrawImageCover(d, *s_BaseImage, s_ImgMin, s_ImgMax, 1.15f, 10.f);
            d->AddRect(s_ImgMin, s_ImgMax, Col(Alpha(m_Accent, 0.6f)), 10.f, 0, 1.5f);
        }
        else {
            DrawItemPicture(
                d, Offset(s_Center, -78.f, -60.f), Offset(s_Center, 78.f, 60.f), EItemKind::Pistol, Mix(m_Accent, c_Text, 0.25f)
            );
        }

        const float s_TextX = s_Pos.x + s_Width * 0.38f;
        Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.72f, { s_TextX, s_Pos.y + 18.f }, m_Accent, "ERFUNDENE WAFFE \xC2\xB7 NUR IN DIESEM MEN\xC3\x9C");
        Text(d, SDK()->GetImGuiBlackFont(), s_Base * 1.6f, { s_TextX, s_Pos.y + 34.f }, c_Text, "Phantom X-47");

        const std::string s_Sub = std::string("Wer getroffen wird, f\xC3\xA4llt sofort um. ") +
            (s_BaseItem ? "Sieht aus wie: " + s_BaseItem->m_Name : std::string("Starte eine Mission."));
        Text(d, SDK()->GetImGuiRegularFont(), s_Base * 0.88f, { s_TextX, s_Pos.y + 34.f + s_Base * 1.7f }, c_Muted, s_Sub.c_str());

        ImGui::SetCursorScreenPos({ s_TextX, s_Pos.y + 98.f });
        ImGui::PushItemWidth(s_Max.x - s_TextX - 200.f);

        static const char* const s_Effects[] = { ICON_MD_WHATSHOT " T\xC3\xB6" "dlich", ICON_MD_BEDTIME " Bewusstlos", ICON_MD_CASINO " Zufall" };
        ImGui::BeginGroup();

        if (SegmentedControl("##PhantomEffectInv", s_Effects, 3, &m_PhantomEffect)) {
            SaveSettings();
        }

        ImGui::EndGroup();
        ImGui::PopItemWidth();

        ImGui::SetCursorScreenPos({ s_Max.x - 186.f, s_Pos.y + 96.f });
        ImGui::BeginDisabled(!s_BaseItem);

        if (ActionButton(m_PhantomGiven ? ICON_MD_ADD " Noch eine holen" : ICON_MD_ADD " Ins Inventar", 170.f, true)) {
            GivePhantom();
        }

        ImGui::EndDisabled();

        if (m_PhantomHits > 0) {
            const std::string s_Hits = std::to_string(m_PhantomHits) + " Treffer";
            const ImVec2 s_HitsSize = Measure(SDK()->GetImGuiMediumFont(), s_Base * 0.8f, s_Hits.c_str());
            Text(d, SDK()->GetImGuiMediumFont(), s_Base * 0.8f, { s_Max.x - 18.f - s_HitsSize.x, s_Pos.y + 18.f }, c_Muted, s_Hits.c_str());
        }

        ImGui::SetCursorScreenPos(Offset(s_Pos, 0.f, s_Height + 14.f));
    }

    // --- Leerer Zustand ---------------------------------------------------------------------------------------------
    if (m_Items.empty()) {
        InfoBox(
            ICON_MD_INFO_OUTLINE,
            SDK()->GetLocalPlayer()
                ? "Der Gegenstands-Katalog wird geladen \xE2\x80\x93 falls hier nichts erscheint, \xC3\xB6" "ffne den Tab gleich noch einmal."
                : "Starte eine Mission \xE2\x80\x93 dann erscheinen hier alle Waffen und Gegenst\xC3\xA4nde mit Bild.",
            false
        );
        return;
    }

    // --- Werkzeugleiste: Suche, Anzahl, Ziel ------------------------------------------------------------------------
    {
        ImGui::SetNextItemWidth(s_Width * 0.42f);
        ImGui::InputTextWithHint("##ItemSearch", ICON_MD_SEARCH " Gegenstand suchen ...", m_ItemSearch, sizeof(m_ItemSearch));

        ImGui::SameLine(0.f, 16.f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Anzahl");
        ImGui::SameLine();

        if (ImGui::Button(ICON_MD_REMOVE "##Less", { 34.f, 0.f })) m_GiveCount = std::max(1, m_GiveCount - 1);
        ImGui::SameLine(0.f, 6.f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%d", m_GiveCount);
        ImGui::SameLine(0.f, 6.f);
        if (ImGui::Button(ICON_MD_ADD "##More", { 34.f, 0.f })) m_GiveCount = std::min(10, m_GiveCount + 1);

        ImGui::SameLine(0.f, 16.f);
        int s_Target = m_GiveToWorld ? 1 : 0;
        static const char* const s_Targets[] = { ICON_MD_BACKPACK " Ins Inventar", ICON_MD_PLACE " Vor mich legen" };

        ImGui::BeginGroup();

        if (SegmentedControl("##Target", s_Targets, 2, &s_Target)) {
            m_GiveToWorld = s_Target == 1;
        }

        ImGui::EndGroup();
    }

    // --- Kategorien als Chips ---------------------------------------------------------------------------------------
    {
        ImGui::Dummy({ 0.f, 2.f });
        const float s_Right = ImGui::GetCursorScreenPos().x + s_Width;

        const auto s_Chip = [&](int p_Kind, const char* p_Label, int p_Count) {
            const bool s_Active = m_ItemFilter == p_Kind;
            const std::string s_Text = std::string(p_Label) + "  " + std::to_string(p_Count);
            const ImVec2 s_TextSize = Measure(SDK()->GetImGuiMediumFont(), s_Base * 0.84f, s_Text.c_str());
            const ImVec2 s_Size = { s_TextSize.x + (p_Kind >= 0 ? 34.f : 24.f), 30.f };

            if (ImGui::GetItemRectMax().x + s_Size.x + 8.f < s_Right && p_Kind != -1) {
                ImGui::SameLine(0.f, 8.f);
            }

            ImGui::PushID(p_Kind);
            const ImVec2 s_Pos = ImGui::GetCursorScreenPos();

            if (ImGui::InvisibleButton("##Chip", s_Size)) {
                m_ItemFilter = s_Active ? -1 : p_Kind;
            }

            const float s_T = Animate(ImGui::GetItemID(), s_Active ? 1.f : (ImGui::IsItemHovered() ? 0.4f : 0.f));
            const ImVec4 s_Color = p_Kind >= 0 ? KindColor(static_cast<EItemKind>(p_Kind)) : m_Accent;
            d->AddRectFilled(s_Pos, Offset(s_Pos, s_Size.x, s_Size.y), Col(Mix(c_Raised, Alpha(s_Color, 0.35f), s_T)), 15.f);

            float s_X = s_Pos.x + 12.f;

            if (p_Kind >= 0) {
                d->AddCircleFilled({ s_X + 4.f, s_Pos.y + s_Size.y * 0.5f }, 4.f, Col(s_Color));
                s_X += 14.f;
            }

            Text(d, SDK()->GetImGuiMediumFont(), s_Base * 0.84f, { s_X, s_Pos.y + (s_Size.y - s_TextSize.y) * 0.5f }, s_Active ? c_Text : Mix(c_Muted, c_Text, s_T), s_Text.c_str());
            ImGui::PopID();
        };

        s_Chip(-1, "Alle", static_cast<int>(m_Items.size()));

        for (int i = 0; i < static_cast<int>(EItemKind::Count); ++i) {
            if (m_KindCounts[i] > 0) {
                s_Chip(i, KindName(static_cast<EItemKind>(i)), m_KindCounts[i]);
            }
        }
    }

    // --- Raster -----------------------------------------------------------------------------------------------------
    const std::string s_Filter = ToLower(m_ItemSearch);
    std::vector<const SItemEntry*> s_Visible;
    s_Visible.reserve(m_Items.size());

    for (const auto& s_Item : m_Items) {
        if (m_ItemFilter >= 0 && static_cast<int>(s_Item.m_Kind) != m_ItemFilter) {
            continue;
        }

        if (!s_Filter.empty() && s_Item.m_NameLower.find(s_Filter) == std::string::npos && s_Item.m_IdText.find(s_Filter) == std::string::npos) {
            continue;
        }

        s_Visible.push_back(&s_Item);
    }

    ImGui::Dummy({ 0.f, 4.f });

    if (s_Visible.empty()) {
        ImGui::TextDisabled("Nichts gefunden.");
        return;
    }

    constexpr float s_Gap = 12.f;
    const int s_Columns = std::max(2, static_cast<int>((s_Width + s_Gap) / (158.f + s_Gap)));
    const float s_CardWidth = (s_Width - s_Gap * (s_Columns - 1)) / s_Columns;
    const ImVec2 s_CardSize = { s_CardWidth, s_CardWidth * 1.08f + 26.f };
    const int s_Rows = (static_cast<int>(s_Visible.size()) + s_Columns - 1) / s_Columns;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { s_Gap, s_Gap });
    ImGuiListClipper s_Clipper;
    s_Clipper.Begin(s_Rows, s_CardSize.y + s_Gap);

    while (s_Clipper.Step()) {
        for (int s_Row = s_Clipper.DisplayStart; s_Row < s_Clipper.DisplayEnd; ++s_Row) {
            for (int s_Col = 0; s_Col < s_Columns; ++s_Col) {
                const int s_Index = s_Row * s_Columns + s_Col;

                if (s_Index >= static_cast<int>(s_Visible.size())) {
                    break;
                }

                if (s_Col > 0) {
                    ImGui::SameLine();
                }

                if (ItemCard(*s_Visible[s_Index], s_CardSize)) {
                    GiveItem(*s_Visible[s_Index], m_GiveCount);
                }
            }
        }
    }

    s_Clipper.End();
    ImGui::PopStyleVar();
    ImGui::Dummy({ 0.f, 8.f });
}
