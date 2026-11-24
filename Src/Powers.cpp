// Kraefte (Kraftstoss, k.o., Entwaffnen ...), Phantom-Treffer, Heilung und die Startseite mit Schnellschaltern.
// Alles, was NPCs anfasst, laeuft auf dem Spiel-Thread (UpdatePowers); Knoepfe stellen nur Auftraege ein.

#include "CheatMenu.h"

#include <IconsMaterialDesign.h>

#include <Glacier/ZActor.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZItem.h>
#include <Glacier/ZModule.h>
#include <Glacier/ZSetpieceEntity.h>
#include <Glacier/ZSpatialEntity.h>

#include "Logging.h"
#include "Theme.h"

#include <unordered_set>

#undef min
#undef max

using namespace cm;

namespace {
    // Knochen-Index aus der Clumsy-Mod des SDK: Becken.
    constexpr uint32_t c_PelvisBone = 1;
    constexpr int c_MaxActors = 1000;

    bool ActorPosition(ZActor* p_Actor, SVector3& p_Pos) {
        ZEntityRef s_Ref;
        p_Actor->GetID(s_Ref);

        if (const auto s_Spatial = s_Ref.QueryInterface<ZSpatialEntity>()) {
            p_Pos = SVector3(s_Spatial->GetObjectToWorldMatrix().Trans);
            return true;
        }

        return false;
    }

    bool PlayerPosition(SVector3& p_Pos) {
        const auto s_Player = SDK()->GetLocalPlayer();

        if (!s_Player) {
            return false;
        }

        if (const auto s_Spatial = s_Player.m_entityRef.QueryInterface<ZSpatialEntity>()) {
            p_Pos = SVector3(s_Spatial->GetObjectToWorldMatrix().Trans);
            return true;
        }

        return false;
    }

    float Distance(const SVector3& a, const SVector3& b) {
        const float x = a.x - b.x;
        const float y = a.y - b.y;
        const float z = a.z - b.z;
        return std::sqrt(x * x + y * y + z * z);
    }

    int ActorCount() {
        return Globals::ActorManager && Globals::NextActorId ? std::min(static_cast<int>(*Globals::NextActorId), c_MaxActors) : 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Auftraege
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::RequestPower(EPower p_Power) {
    std::scoped_lock s_Lock(m_PendingMutex);
    m_PendingPowers.push_back(p_Power);
}

bool CheatMenu::IsLiveActor(ZActor* p_Actor) const {
    if (!p_Actor) {
        return false;
    }

    const int s_Count = ActorCount();

    for (int i = 0; i < s_Count; ++i) {
        if (Globals::ActorManager->m_activatedActors[i].m_pInterfaceRef == p_Actor) {
            return p_Actor->IsAlive();
        }
    }

    return false;
}

void CheatMenu::KillActor(ZActor* p_Actor) {
    if (!p_Actor || !p_Actor->IsAlive()) {
        return;
    }

    TEntityRef<IItem> s_Item;
    TEntityRef<ZSetpieceEntity> s_Setpiece;
    Functions::ZActor_KillActor->Call(p_Actor, s_Item, s_Setpiece, EDamageEvent::eDE_Shoot, EDeathBehavior::eDB_IMPACT_ANIM);
}

void CheatMenu::KnockOut(ZActor* p_Actor) {
    if (!p_Actor || !p_Actor->IsAlive()) {
        return;
    }

    Functions::ZHM5BaseCharacter_ActivateRagdoll->Call(p_Actor, true);

    const double s_Now = NowSeconds();
    SVector3 s_Pos {};
    ActorPosition(p_Actor, s_Pos);

    for (auto& s_Entry : m_KnockedOut) {
        if (s_Entry.m_Actor == p_Actor) {
            s_Entry.m_Until = m_KnockOutSeconds >= 299.f ? -1.0 : s_Now + m_KnockOutSeconds;
            s_Entry.m_DownPos = s_Pos;
            return;
        }
    }

    m_KnockedOut.push_back({ p_Actor, m_KnockOutSeconds >= 299.f ? -1.0 : s_Now + m_KnockOutSeconds, s_Now + 1.5, s_Pos });
}

void CheatMenu::HealPlayer() {
    const auto s_Player = SDK()->GetLocalPlayer();

    if (!s_Player || !s_Player.m_pInterfaceRef->m_pHealth) {
        return;
    }

    ZHM5Health* s_Health = s_Player.m_pInterfaceRef->m_pHealth;
    const float s_Max = s_Health->m_fMaxHitPoints.Get();

    if (s_Max > 0.f) {
        s_Health->m_fHitPoints.Set(s_Max);
    }
}

void CheatMenu::RunPower(EPower p_Power) {
    if (p_Power == EPower::Heal) {
        HealPlayer();
        SetStatus("Voll geheilt");
        return;
    }

    if (p_Power == EPower::SummonActors) {
        TeleportAllActorsToPlayer();
        return;
    }

    SVector3 s_PlayerPos {};

    if (!PlayerPosition(s_PlayerPos)) {
        SetStatus("Lade zuerst eine Mission");
        return;
    }

    const double s_Now = NowSeconds();
    const int s_Count = ActorCount();
    int s_Affected = 0;

    for (int i = 0; i < s_Count; ++i) {
        ZActor* s_Actor = Globals::ActorManager->m_activatedActors[i].m_pInterfaceRef;
        SVector3 s_Pos {};

        if (!s_Actor || !s_Actor->IsAlive() || !ActorPosition(s_Actor, s_Pos)) {
            continue;
        }

        const float s_Distance = Distance(s_Pos, s_PlayerPos);

        if (s_Distance > m_PowerRadius || s_Distance < 0.05f) {
            continue;
        }

        switch (p_Power) {
            case EPower::KillNearby:
                KillActor(s_Actor);
                ++s_Affected;
                break;

            case EPower::KnockOutNearby:
                KnockOut(s_Actor);
                ++s_Affected;
                break;

            case EPower::ForcePush: {
                // Weg von 47, je naeher desto staerker, mit etwas Auftrieb.
                float s_Dx = s_Pos.x - s_PlayerPos.x;
                float s_Dy = s_Pos.y - s_PlayerPos.y;
                const float s_Length = std::max(0.01f, std::sqrt(s_Dx * s_Dx + s_Dy * s_Dy));
                const float s_Falloff = 0.4f + 0.6f * (1.f - s_Distance / std::max(1.f, m_PowerRadius));
                s_Dx /= s_Length;
                s_Dy /= s_Length;

                Functions::ZHM5BaseCharacter_ActivateRagdoll->Call(s_Actor, true);
                m_Impulses.push_back({
                    s_Actor,
                    { s_Dx * m_PushStrength * s_Falloff, s_Dy * m_PushStrength * s_Falloff, m_PushStrength * 0.4f * s_Falloff },
                    s_Now + 0.06
                });
                ++s_Affected;
                break;
            }

            case EPower::Launch:
                Functions::ZHM5BaseCharacter_ActivateRagdoll->Call(s_Actor, true);
                m_Impulses.push_back({ s_Actor, { 0.f, 0.f, m_PushStrength * 1.6f }, s_Now + 0.06 });
                ++s_Affected;
                break;

            case EPower::DisarmNearby: {
                ZActorInventoryHandler* s_Inventory = s_Actor->m_pInventoryHandler;

                if (!s_Inventory || !s_Inventory->m_rMainWeapon) {
                    break;
                }

                // Wie die Editor-Mod des SDK, aber "in die Physikwelt": die Waffe faellt sichtbar zu Boden.
                const TEntityRef<IItem> s_Weapon(s_Inventory->m_rMainWeapon.m_entityRef);
                s_Actor->ReleaseItem(s_Weapon, true);
                s_Inventory->m_rMainWeapon = {};
                s_Actor->m_bWeaponUnholstered = false;
                ++s_Affected;
                break;
            }

            default:
                break;
        }
    }

    const std::string s_Radius = std::to_string(static_cast<int>(m_PowerRadius)) + " m";

    switch (p_Power) {
        case EPower::KillNearby: SetStatus(std::to_string(s_Affected) + " NPCs im Umkreis von " + s_Radius + " ausgeschaltet"); break;
        case EPower::KnockOutNearby: SetStatus(std::to_string(s_Affected) + " NPCs bewusstlos"); break;
        case EPower::ForcePush: SetStatus("Kraftsto\xC3\x9F: " + std::to_string(s_Affected) + " NPCs weggeschleudert"); break;
        case EPower::Launch: SetStatus(std::to_string(s_Affected) + " NPCs in die Luft geschleudert"); break;
        case EPower::DisarmNearby: SetStatus(std::to_string(s_Affected) + " Waffen fallen zu Boden"); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Jedes Bild (Spiel-Thread)
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::UpdatePowers() {
    std::vector<EPower> s_Powers;
    std::vector<SPendingHit> s_Hits;

    {
        std::scoped_lock s_Lock(m_PendingMutex);
        s_Powers.swap(m_PendingPowers);
        s_Hits.swap(m_PendingHits);
    }

    for (const EPower s_Power : s_Powers) {
        RunPower(s_Power);
    }

    // Phantom-Treffer: ein Schuss kann mehrere Treffer-Meldungen ausloesen – jeder NPC nur einmal.
    std::unordered_set<ZActor*> s_Done;

    for (const SPendingHit& s_Hit : s_Hits) {
        if (!s_Done.insert(s_Hit.m_Actor).second || !IsLiveActor(s_Hit.m_Actor)) {
            continue;
        }

        if (s_Hit.m_Effect == EPhantomEffect::Kill) {
            KillActor(s_Hit.m_Actor);
        }
        else {
            // Den Lebenspunkte-Puffer aus dem Treffer wieder abziehen, dann umfallen lassen.
            if (s_Hit.m_RestoreHp > 0.f) {
                s_Hit.m_Actor->m_fCurrentHitPoints = s_Hit.m_RestoreHp;
            }

            KnockOut(s_Hit.m_Actor);
        }
    }

    // Stoesse, sobald der Ragdoll aktiv ist.
    if (!m_Impulses.empty()) {
        const double s_Now = NowSeconds();

        for (auto s_It = m_Impulses.begin(); s_It != m_Impulses.end();) {
            if (s_Now < s_It->m_At) {
                ++s_It;
                continue;
            }

            ZActor* s_Actor = s_It->m_Actor;

            if (IsLiveActor(s_Actor) && s_Actor->m_pRagdollHandler) {
                Functions::ZRagdollHandler_ApplyImpulseOnRagdoll->Call(
                    s_Actor->m_pRagdollHandler, float4 {},
                    float4 { s_It->m_Impulse.x, s_It->m_Impulse.y, s_It->m_Impulse.z, 0.f }, c_PelvisBone, false
                );
            }

            s_It = m_Impulses.erase(s_It);
        }
    }
}

void CheatMenu::UpdateKnockedOut() {
    if (m_KnockedOut.empty()) {
        return;
    }

    const double s_Now = NowSeconds();

    for (auto s_It = m_KnockedOut.begin(); s_It != m_KnockedOut.end();) {
        if (!IsLiveActor(s_It->m_Actor) || (s_It->m_Until > 0.0 && s_Now > s_It->m_Until)) {
            s_It = m_KnockedOut.erase(s_It);
            continue;
        }

        // Steht die Spiel-KI ihn wieder auf und laeuft los, sackt er erneut zusammen.
        if (s_Now >= s_It->m_NextCheck) {
            s_It->m_NextCheck = s_Now + 1.0;
            SVector3 s_Pos {};

            if (ActorPosition(s_It->m_Actor, s_Pos)) {
                if (Distance(s_Pos, s_It->m_DownPos) > 1.0f) {
                    Functions::ZHM5BaseCharacter_ActivateRagdoll->Call(s_It->m_Actor, true);
                }

                s_It->m_DownPos = s_Pos;
            }
        }

        ++s_It;
    }
}

void CheatMenu::UpdateFrozen() {
    if (!m_FreezeNpcs) {
        m_FrozenTransforms.clear();
        return;
    }

    SVector3 s_PlayerPos {};

    if (!PlayerPosition(s_PlayerPos)) {
        return;
    }

    std::unordered_set<ZActor*> s_Down;

    for (const auto& s_Entry : m_KnockedOut) {
        s_Down.insert(s_Entry.m_Actor);
    }

    const int s_Count = ActorCount();

    for (int i = 0; i < s_Count; ++i) {
        ZActor* s_Actor = Globals::ActorManager->m_activatedActors[i].m_pInterfaceRef;

        if (!s_Actor || !s_Actor->IsAlive() || s_Down.contains(s_Actor)) {
            continue;
        }

        ZEntityRef s_Ref;
        s_Actor->GetID(s_Ref);
        const auto s_Spatial = s_Ref.QueryInterface<ZSpatialEntity>();

        if (!s_Spatial) {
            continue;
        }

        auto s_It = m_FrozenTransforms.find(s_Actor);

        if (s_It == m_FrozenTransforms.end()) {
            const SMatrix s_Matrix = s_Spatial->GetObjectToWorldMatrix();

            if (Distance(SVector3(s_Matrix.Trans), s_PlayerPos) <= m_PowerRadius * 3.f) {
                m_FrozenTransforms.emplace(s_Actor, s_Matrix);
            }

            continue;
        }

        s_Spatial->SetObjectToWorldMatrixFromEditor(s_It->second);
    }
}

void CheatMenu::UpdateAutoHeal() {
    if (m_AutoHeal) {
        HealPlayer();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Bausteine der Oberflaeche
// ---------------------------------------------------------------------------------------------------------------------

bool CheatMenu::Tile(const char* p_Icon, const char* p_Label, const char* p_Sub, bool p_Active, const ImVec2& p_Size) {
    ImGui::PushID(p_Label);

    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const bool s_Clicked = ImGui::InvisibleButton("##Tile", p_Size);
    const bool s_Hovered = ImGui::IsItemHovered();
    const bool s_Held = ImGui::IsItemActive();
    const float s_On = Animate(ImGui::GetItemID(), p_Active ? 1.f : 0.f);
    const float s_Hover = Animate(ImGui::GetItemID() + 1, s_Hovered ? 1.f : 0.f, 18.f);
    const float s_Base = ImGui::GetFontSize();

    const float s_Press = s_Held ? 2.f : 0.f;
    const ImVec2 s_Min = Offset(s_Pos, s_Press, s_Press);
    const ImVec2 s_Max = Offset(s_Pos, p_Size.x - s_Press, p_Size.y - s_Press);

    ImVec4 s_Bg = Mix(c_CardBg, c_Raised, 0.6f * s_Hover);
    s_Bg = Mix(s_Bg, Mix(c_CardBg, m_Accent, 0.28f), s_On);
    d->AddRectFilled(s_Min, s_Max, Col(s_Bg), 14.f);
    d->AddRect(s_Min, s_Max, Col(Mix(c_Border, m_Accent, std::max(s_On, 0.5f * s_Hover))), 14.f, 0, 1.f + s_On);

    // Symbol oben links in einer runden Plakette.
    const ImVec2 s_Badge = Offset(s_Min, 16.f, 16.f);
    d->AddRectFilled(s_Badge, Offset(s_Badge, 38.f, 38.f), Col(Mix(Alpha(c_Text, 0.06f), m_Accent, s_On)), 11.f);
    const float s_IconSize = s_Base * 1.25f;
    const ImVec2 s_IconDim = Measure(nullptr, s_IconSize, p_Icon);
    Text(d, nullptr, s_IconSize, Offset(s_Badge, (38.f - s_IconDim.x) * 0.5f, (38.f - s_IconDim.y) * 0.5f), Mix(c_Muted, { 1.f, 1.f, 1.f, 1.f }, std::max(s_On, s_Hover * 0.6f)), p_Icon);

    // Status-Pille oben rechts.
    if (p_Sub == nullptr) {
        const char* s_State = p_Active ? "AN" : "AUS";
        const ImVec2 s_StateDim = Measure(SDK()->GetImGuiBoldFont(), s_Base * 0.68f, s_State);
        const ImVec2 s_PillMin = { s_Max.x - s_StateDim.x - 30.f, s_Min.y + 18.f };
        d->AddRectFilled(s_PillMin, { s_Max.x - 14.f, s_PillMin.y + s_StateDim.y + 8.f }, Col(Mix(Alpha(c_Text, 0.07f), Alpha(m_Accent, 0.9f), s_On)), 10.f);
        Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.68f, Offset(s_PillMin, 8.f, 4.f), Mix(c_Muted, { 1.f, 1.f, 1.f, 1.f }, s_On), s_State);
    }

    // Beschriftung unten.
    const float s_LabelY = s_Max.y - 16.f - s_Base - (p_Sub ? s_Base * 0.86f + 4.f : 0.f);
    const ImVec4 s_LabelClip = { s_Min.x, s_Min.y, s_Max.x - 10.f, s_Max.y };
    d->AddText(SDK()->GetImGuiBoldFont(), s_Base, { s_Min.x + 16.f, s_LabelY }, Col(c_Text), p_Label, nullptr, 0.f, &s_LabelClip);

    if (p_Sub) {
        d->AddText(SDK()->GetImGuiRegularFont(), s_Base * 0.86f, { s_Min.x + 16.f, s_LabelY + s_Base + 4.f }, Col(c_Muted), p_Sub, nullptr, 0.f, &s_LabelClip);
    }

    ImGui::PopID();
    return s_Clicked;
}

bool CheatMenu::SegmentedControl(const char* p_Id, const char* const* p_Labels, int p_Count, int* p_Value) {
    ImGui::PushID(p_Id);

    ImDrawList* d = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    // Breite: hoechstens 150 px je Feld, nie mehr als Platz da ist (oder per PushItemWidth vorgegeben).
    const float s_Width = std::max(80.f, std::min({ ImGui::CalcItemWidth(), ImGui::GetContentRegionAvail().x, 150.f * p_Count }));
    constexpr float s_Height = 36.f;
    const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
    const float s_Segment = s_Width / p_Count;
    bool s_Changed = false;

    d->AddRectFilled(s_Pos, Offset(s_Pos, s_Width, s_Height), Col(c_Raised), 10.f);

    // Gleitende Markierung unter der Auswahl.
    const float s_Slide = Animate(ImGui::GetID("##Slide"), static_cast<float>(*p_Value), 16.f);
    const ImVec2 s_MarkMin = Offset(s_Pos, 3.f + s_Slide * s_Segment, 3.f);
    d->AddRectFilled(s_MarkMin, Offset(s_MarkMin, s_Segment - 6.f, s_Height - 6.f), Col(m_Accent), 8.f);

    for (int i = 0; i < p_Count; ++i) {
        ImGui::SetCursorScreenPos(Offset(s_Pos, s_Segment * i, 0.f));
        ImGui::PushID(i);

        if (ImGui::InvisibleButton("##Seg", { s_Segment, s_Height }) && *p_Value != i) {
            *p_Value = i;
            s_Changed = true;
        }

        const bool s_Selected = *p_Value == i;
        const ImVec2 s_LabelDim = Measure(SDK()->GetImGuiMediumFont(), s_Base * 0.9f, p_Labels[i]);
        Text(
            d, SDK()->GetImGuiMediumFont(), s_Base * 0.9f,
            Offset(s_Pos, s_Segment * i + (s_Segment - s_LabelDim.x) * 0.5f, (s_Height - s_LabelDim.y) * 0.5f),
            s_Selected ? ImVec4 { 1.f, 1.f, 1.f, 1.f } : (ImGui::IsItemHovered() ? c_Text : c_Muted), p_Labels[i]
        );

        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(Offset(s_Pos, 0.f, s_Height));
    ImGui::Dummy({ s_Width, 0.f });
    ImGui::PopID();
    return s_Changed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Seiten
// ---------------------------------------------------------------------------------------------------------------------

void CheatMenu::DrawHomeTab() {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const float s_Base = ImGui::GetFontSize();
    const float s_Width = ImGui::GetContentRegionAvail().x;
    const bool s_InLevel = static_cast<bool>(SDK()->GetLocalPlayer());

    // --- Kopf: Lage auf einen Blick ---------------------------------------------------------------------------------
    {
        const ImVec2 s_Pos = ImGui::GetCursorScreenPos();
        constexpr float s_Height = 112.f;
        d->AddRectFilled(s_Pos, Offset(s_Pos, s_Width, s_Height), Col(c_CardBg), 16.f);
        d->AddRectFilledMultiColor(
            Offset(s_Pos, 1.f, 1.f), Offset(s_Pos, s_Width * 0.55f, s_Height - 1.f),
            Col(Alpha(m_Accent, 0.20f)), Col(Alpha(m_Accent, 0.f)), Col(Alpha(m_Accent, 0.f)), Col(Alpha(m_Accent, 0.10f))
        );

        Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.72f, Offset(s_Pos, 22.f, 20.f), m_Accent, s_InLevel ? "IM EINSATZ" : "KEINE MISSION GELADEN");
        Text(d, SDK()->GetImGuiBlackFont(), s_Base * 1.7f, Offset(s_Pos, 22.f, 36.f), c_Text, "Guten Abend, 47.");
        Text(
            d, SDK()->GetImGuiRegularFont(), s_Base * 0.9f, Offset(s_Pos, 22.f, 38.f + s_Base * 1.85f), c_Muted,
            s_InLevel ? "Alles bereit. Schalte unten mit einem Klick, was du brauchst." : "Starte eine Mission \xE2\x80\x93 die meisten Cheats wirken nur im Level."
        );

        // Kennzahlen rechts.
        struct SStat {
            const char* m_Label;
            int m_Value;
        };

        const SStat s_Stats[] = {
            { "AKTIV", ActiveCheatCount() },
            { "BEWUSSTLOS", static_cast<int>(m_KnockedOut.size()) },
            { "PHANTOM-TREFFER", m_PhantomHits },
        };

        float s_X = s_Pos.x + s_Width - 22.f;

        for (int i = static_cast<int>(std::size(s_Stats)) - 1; i >= 0; --i) {
            const std::string s_Value = std::to_string(s_Stats[i].m_Value);
            const ImVec2 s_ValueDim = Measure(SDK()->GetImGuiBlackFont(), s_Base * 1.9f, s_Value.c_str());
            const ImVec2 s_LabelDim = Measure(SDK()->GetImGuiBoldFont(), s_Base * 0.64f, s_Stats[i].m_Label);
            const float s_ColWidth = std::max(s_ValueDim.x, s_LabelDim.x);
            s_X -= s_ColWidth;
            Text(d, SDK()->GetImGuiBlackFont(), s_Base * 1.9f, { s_X + s_ColWidth - s_ValueDim.x, s_Pos.y + 26.f }, s_Stats[i].m_Value > 0 ? c_Text : c_Muted, s_Value.c_str());
            Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.64f, { s_X + s_ColWidth - s_LabelDim.x, s_Pos.y + 32.f + s_Base * 1.9f }, c_Muted, s_Stats[i].m_Label);
            s_X -= 34.f;
        }

        ImGui::Dummy({ s_Width, s_Height + 12.f });
    }

    const auto s_Grid = [&](int p_Columns) {
        const float s_Gap = 12.f;
        return ImVec2 { (s_Width - s_Gap * (p_Columns - 1)) / p_Columns, 118.f };
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 12.f, 12.f });

    // --- Schnellschalter --------------------------------------------------------------------------------------------
    Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.74f, Offset(ImGui::GetCursorScreenPos(), 4.f, 0.f), c_Muted, "SCHNELLSCHALTER");
    ImGui::Dummy({ 0.f, s_Base * 0.6f });

    {
        const ImVec2 s_Size = s_Grid(4);
        int s_Column = 0;
        const auto s_Next = [&]() {
            if (++s_Column % 4 != 0) {
                ImGui::SameLine();
            }
        };

        if (Tile(ICON_MD_SECURITY, "Unverwundbar", nullptr, m_GodMode, s_Size)) SetGodMode(!m_GodMode);
        s_Next();
        if (Tile(ICON_MD_VISIBILITY_OFF, "Unsichtbar", nullptr, m_Invisible, s_Size)) SetInvisible(!m_Invisible);
        s_Next();
        if (Tile(ICON_MD_ALL_INCLUSIVE, "Unendlich Munition", nullptr, m_InfiniteAmmo, s_Size)) {
            m_InfiniteAmmo = !m_InfiniteAmmo;
            RefreshCrippleBox();
        }
        s_Next();
        if (Tile(ICON_MD_REPLAY, "Kein Nachladen", nullptr, m_NoReload, s_Size)) m_NoReload = !m_NoReload;
        s_Next();
        if (Tile(ICON_MD_WHATSHOT, "One-Hit-Kill", nullptr, m_OneHitKill, s_Size)) m_OneHitKill = !m_OneHitKill;
        s_Next();
        if (Tile(ICON_MD_AUTO_AWESOME, "Phantom-Munition", nullptr, m_PhantomAllWeapons, s_Size)) m_PhantomAllWeapons = !m_PhantomAllWeapons;
        s_Next();
        if (Tile(ICON_MD_FAVORITE, "Auto-Heilung", nullptr, m_AutoHeal, s_Size)) m_AutoHeal = !m_AutoHeal;
        s_Next();
        if (Tile(ICON_MD_FLIGHT, "Noclip", nullptr, m_Noclip, s_Size)) SetNoclip(!m_Noclip);
        s_Next();
        if (Tile(ICON_MD_FAST_FORWARD, "Schnellfeuer", nullptr, m_RapidFire, s_Size)) m_RapidFire = !m_RapidFire;
        s_Next();
        if (Tile(ICON_MD_CENTER_FOCUS_STRONG, "Perfekte Pr\xC3\xA4zision", nullptr, m_PerfectAim, s_Size)) m_PerfectAim = !m_PerfectAim;
        s_Next();
        if (Tile(ICON_MD_SLOW_MOTION_VIDEO, "Zeitmanipulation", nullptr, m_TimeScaleEnabled, s_Size)) m_TimeScaleEnabled = !m_TimeScaleEnabled;
        s_Next();
        if (Tile(ICON_MD_AC_UNIT, "NPCs einfrieren", nullptr, m_FreezeNpcs, s_Size)) m_FreezeNpcs = !m_FreezeNpcs;
    }

    ImGui::Dummy({ 0.f, 6.f });

    // --- Aktionen ---------------------------------------------------------------------------------------------------
    Text(d, SDK()->GetImGuiBoldFont(), s_Base * 0.74f, Offset(ImGui::GetCursorScreenPos(), 4.f, 0.f), c_Muted, "SOFORT-AKTIONEN");
    ImGui::Dummy({ 0.f, s_Base * 0.6f });

    {
        const ImVec2 s_Size = s_Grid(3);

        if (Tile(ICON_MD_AIR, "Kraftsto\xC3\x9F", "Strg+F \xC2\xB7 schleudert alle weg", false, s_Size)) RequestPower(EPower::ForcePush);
        ImGui::SameLine();
        if (Tile(ICON_MD_BEDTIME, "Alle k.o.", "Strg+K \xC2\xB7 Umkreis bewusstlos", false, s_Size)) RequestPower(EPower::KnockOutNearby);
        ImGui::SameLine();
        if (Tile(ICON_MD_LOCAL_HOSPITAL, "Heilen", "Strg+H \xC2\xB7 volle Gesundheit", false, s_Size)) RequestPower(EPower::Heal);

        if (Tile(ICON_MD_PAN_TOOL, "Alle entwaffnen", "Waffen fallen zu Boden", false, s_Size)) RequestPower(EPower::DisarmNearby);
        ImGui::SameLine();
        if (Tile(ICON_MD_ROCKET_LAUNCH, "Abheben", "Alle in die Luft schleudern", false, s_Size)) RequestPower(EPower::Launch);
        ImGui::SameLine();
        if (Tile(ICON_MD_BACKPACK, "Phantom X-47", m_PhantomGiven ? "Schon im Inventar \xC2\xB7 noch eine?" : "Erfundene Waffe holen", false, s_Size)) GivePhantom();
    }

    ImGui::PopStyleVar();
    ImGui::Dummy({ 0.f, 8.f });
}

void CheatMenu::DrawPowersTab() {
    PageTitle(ICON_MD_BOLT, "Kr\xC3\xA4" "fte & NPCs", "Schleudere, bet\xC3\xA4ube und entwaffne alle um dich herum.");

    BeginCard("##PowerSettings", "REICHWEITE & ST\xC3\x84RKE");
    ImGui::Dummy({ 0.f, 2.f });

    if (SliderRow("Reichweite", "%.0f m", &m_PowerRadius, 3.f, 60.f)) SaveSettings();
    if (SliderRow("Sto\xC3\x9F" "kraft", "%.0f", &m_PushStrength, 10.f, 250.f)) SaveSettings();

    const char* s_KoFormat = m_KnockOutSeconds >= 299.f ? "f\xC3\xBCr immer" : "%.0f s";
    if (SliderRow("Bewusstlos f\xC3\xBCr", s_KoFormat, &m_KnockOutSeconds, 10.f, 300.f)) SaveSettings();

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    const float s_Width = ImGui::GetContentRegionAvail().x;
    const ImVec2 s_Size = { (s_Width - 24.f) / 3.f, 118.f };
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 12.f, 12.f });

    if (Tile(ICON_MD_AIR, "Kraftsto\xC3\x9F", "Strg+F", false, s_Size)) RequestPower(EPower::ForcePush);
    ImGui::SameLine();
    if (Tile(ICON_MD_ROCKET_LAUNCH, "Abheben", "Alle nach oben", false, s_Size)) RequestPower(EPower::Launch);
    ImGui::SameLine();
    if (Tile(ICON_MD_BEDTIME, "Alle k.o.", "Strg+K", false, s_Size)) RequestPower(EPower::KnockOutNearby);

    if (Tile(ICON_MD_PAN_TOOL, "Alle entwaffnen", "Waffen fallen zu Boden", false, s_Size)) RequestPower(EPower::DisarmNearby);
    ImGui::SameLine();
    if (Tile(ICON_MD_WHATSHOT, "Alle ausschalten", "T\xC3\xB6tet im Umkreis", false, s_Size)) RequestPower(EPower::KillNearby);
    ImGui::SameLine();
    if (Tile(ICON_MD_AC_UNIT, "NPCs einfrieren", m_FreezeNpcs ? "AN \xC2\xB7 nochmal = aus" : "Niemand bewegt sich", m_FreezeNpcs, s_Size)) m_FreezeNpcs = !m_FreezeNpcs;

    ImGui::PopStyleVar();
    ImGui::Dummy({ 0.f, 6.f });

    BeginCard("##Summon", "HERBEIRUFEN");
    ImGui::Dummy({ 0.f, 2.f });

    const float s_Half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ActionButton(ICON_MD_PEOPLE " Alle NPCs zu mir", s_Half, true)) {
        RequestPower(EPower::SummonActors);
    }

    ImGui::SameLine();

    if (ActionButton(ICON_MD_INVENTORY " Alle Gegenst\xC3\xA4nde zu mir", s_Half)) {
        TeleportAllItemsToPlayer();
    }

    ImGui::Dummy({ 0.f, 4.f });
    EndCard();

    if (!m_KnockedOut.empty()) {
        const std::string s_Text = std::to_string(m_KnockedOut.size()) + " NPCs liegen gerade bewusstlos am Boden.";
        InfoBox(ICON_MD_BEDTIME, s_Text.c_str(), false);
    }

    InfoBox(
        ICON_MD_WARNING,
        "Bewusstlos hei\xC3\x9Ft hier: der NPC f\xC3\xA4llt um und bleibt liegen, solange die Zeit l\xC3\xA4uft. "
        "Einfrieren und Herbeirufen k\xC3\xB6nnen Missionsskripte durcheinanderbringen \xE2\x80\x93 vorher speichern.",
        true
    );
}
