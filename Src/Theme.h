#pragma once

// Farben und Zeichenhilfen des Menues (gemeinsam fuer alle Seiten).

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <iterator>
#include <string>

#include <imgui.h>

#include "IModSDK.h"

namespace cm {
    // Echtzeit in Sekunden (laeuft auch in der Pause und auf jedem Thread).
    inline double NowSeconds() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    inline constexpr ImVec4 c_WindowBg { 0.063f, 0.071f, 0.086f, 0.985f };
    inline constexpr ImVec4 c_SidebarBg { 0.047f, 0.054f, 0.066f, 1.f };
    inline constexpr ImVec4 c_CardBg { 0.094f, 0.106f, 0.129f, 1.f };
    inline constexpr ImVec4 c_Raised { 0.141f, 0.153f, 0.184f, 1.f };
    inline constexpr ImVec4 c_Border { 0.149f, 0.161f, 0.192f, 1.f };
    inline constexpr ImVec4 c_Text { 0.925f, 0.933f, 0.949f, 1.f };
    inline constexpr ImVec4 c_Muted { 0.541f, 0.569f, 0.616f, 1.f };
    inline constexpr ImVec4 c_SwitchOff { 0.196f, 0.212f, 0.251f, 1.f };
    inline constexpr ImVec4 c_Success { 0.247f, 0.808f, 0.553f, 1.f };
    inline constexpr ImVec4 c_Warning { 0.961f, 0.690f, 0.243f, 1.f };

    struct SAccent {
        const char* m_Name;
        ImVec4 m_Color;
    };

    inline constexpr SAccent c_Accents[] = {
        { "Agentur-Rot", { 0.898f, 0.267f, 0.290f, 1.f } },
        { "Eisblau", { 0.290f, 0.573f, 0.976f, 1.f } },
        { "Smaragd", { 0.204f, 0.792f, 0.537f, 1.f } },
        { "Violett", { 0.616f, 0.431f, 0.973f, 1.f } },
        { "Gold", { 0.949f, 0.722f, 0.259f, 1.f } },
    };

    inline constexpr int c_AccentCount = static_cast<int>(std::size(c_Accents));

    inline constexpr float c_HeaderHeight = 68.f;
    inline constexpr float c_FooterHeight = 46.f;
    inline constexpr float c_SidebarWidth = 206.f;

    inline ImVec4 Alpha(const ImVec4& p_Color, float p_Alpha) {
        return { p_Color.x, p_Color.y, p_Color.z, p_Alpha };
    }

    inline ImVec4 Mix(const ImVec4& p_A, const ImVec4& p_B, float p_T) {
        return {
            p_A.x + (p_B.x - p_A.x) * p_T,
            p_A.y + (p_B.y - p_A.y) * p_T,
            p_A.z + (p_B.z - p_A.z) * p_T,
            p_A.w + (p_B.w - p_A.w) * p_T
        };
    }

    inline ImU32 Col(const ImVec4& p_Color) {
        return ImGui::ColorConvertFloat4ToU32(p_Color);
    }

    inline ImVec2 Offset(const ImVec2& p_Pos, float p_X, float p_Y) {
        return { p_Pos.x + p_X, p_Pos.y + p_Y };
    }

    inline ImFont* FontOr(ImFont* p_Font) {
        return p_Font ? p_Font : ImGui::GetFont();
    }

    inline ImVec2 Measure(ImFont* p_Font, float p_Size, const char* p_Text) {
        return FontOr(p_Font)->CalcTextSizeA(p_Size, FLT_MAX, 0.f, p_Text);
    }

    inline void Text(ImDrawList* p_DrawList, ImFont* p_Font, float p_Size, const ImVec2& p_Pos, const ImVec4& p_Color, const char* p_Text) {
        p_DrawList->AddText(FontOr(p_Font), p_Size, p_Pos, Col(p_Color), p_Text);
    }

    // Smoothly animates a value stored in ImGui's state storage towards a target.
    inline float Animate(ImGuiID p_Id, float p_Target, float p_Speed = 14.f) {
        ImGuiStorage* s_Storage = ImGui::GetStateStorage();
        float s_Value = s_Storage->GetFloat(p_Id, p_Target);
        s_Value += (p_Target - s_Value) * std::min(1.f, ImGui::GetIO().DeltaTime * p_Speed);

        if (std::fabs(s_Value - p_Target) < 0.001f) {
            s_Value = p_Target;
        }

        s_Storage->SetFloat(p_Id, s_Value);
        return s_Value;
    }

    inline void DrawSwitch(ImDrawList* p_DrawList, const ImVec2& p_Pos, float p_T, bool p_Hovered, const ImVec4& p_Accent) {
        constexpr float s_Width = 42.f;
        constexpr float s_Height = 24.f;
        constexpr float s_Knob = s_Height * 0.5f - 3.f;

        ImVec4 s_Track = Mix(c_SwitchOff, p_Accent, p_T);

        if (p_Hovered) {
            s_Track = Mix(s_Track, c_Text, 0.08f);
        }

        p_DrawList->AddRectFilled(p_Pos, Offset(p_Pos, s_Width, s_Height), Col(s_Track), s_Height * 0.5f);

        const ImVec2 s_Center = Offset(p_Pos, 3.f + s_Knob + p_T * (s_Width - 6.f - s_Knob * 2.f), s_Height * 0.5f);
        p_DrawList->AddCircleFilled(Offset(s_Center, 0.f, 1.f), s_Knob, IM_COL32(0, 0, 0, 60));
        p_DrawList->AddCircleFilled(s_Center, s_Knob, IM_COL32(255, 255, 255, 255));
    }

    inline std::string ToLower(std::string p_Value) {
        std::transform(p_Value.begin(), p_Value.end(), p_Value.begin(), [](unsigned char c) { return std::tolower(c); });
        return p_Value;
    }
}
