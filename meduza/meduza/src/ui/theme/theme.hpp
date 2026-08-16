#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <unordered_map>

inline float g_sw, g_sh;

namespace ui {

namespace clr {
    // macOS Liquid Glass Palette
    inline ImVec4 bg           = ImVec4(15/255.f, 18/255.f, 28/255.f, 0.78f);
    inline ImVec4 bg_two       = ImVec4(20/255.f, 24/255.f, 38/255.f, 0.82f);
    inline ImVec4 sidebar      = ImVec4(12/255.f, 15/255.f, 22/255.f, 0.85f);
    inline ImVec4 panel        = ImVec4(25/255.f, 30/255.f, 45/255.f, 0.55f);
    inline ImVec4 widget       = ImVec4(35/255.f, 42/255.f, 62/255.f, 0.60f);
    inline ImVec4 accent       = ImVec4(0/255.f, 122/255.f, 255/255.f, 1.f); // macOS Blue
    inline ImVec4 accent_light = ImVec4(64/255.f, 156/255.f, 255/255.f, 1.f);
    inline ImVec4 accent_dark  = ImVec4(0/255.f, 90/255.f, 200/255.f, 1.f);
    inline ImVec4 mac_close    = ImVec4(255/255.f, 95/255.f, 87/255.f, 1.f);  // Red dot
    inline ImVec4 mac_min      = ImVec4(255/255.f, 189/255.f, 46/255.f, 1.f); // Yellow dot
    inline ImVec4 mac_max      = ImVec4(39/255.f, 201/255.f, 63/255.f, 1.f);  // Green dot
    inline ImVec4 text         = ImVec4(245/255.f, 247/255.f, 250/255.f, 1.f);
    inline ImVec4 text_light   = ImVec4(255/255.f, 255/255.f, 255/255.f, 1.f);
    inline ImVec4 text_dim     = ImVec4(140/255.f, 150/255.f, 165/255.f, 1.f);
    inline ImVec4 border       = ImVec4(255/255.f, 255/255.f, 255/255.f, 0.12f);
    inline ImVec4 border_light = ImVec4(255/255.f, 255/255.f, 255/255.f, 0.22f);
    inline ImVec4 border_dark  = ImVec4(0/255.f, 0/255.f, 0/255.f, 0.5f);
    inline ImVec4 subtab_bg    = ImVec4(20/255.f, 20/255.f, 20/255.f, 1.f);
}

namespace style {
    inline std::unordered_map<std::string, float> anims;
    inline std::unordered_map<std::string, ImVec4> anim_colors;
    inline float content_w = 0.f;
    inline float content_alpha = 1.f;
    inline bool popup_open = false;
    inline std::string active_popup = "";
    inline constexpr float S = 2.5f;

    void tick();
    float anim(const std::string& id, float tgt, float spd = 12.f);
    ImVec4 anim_col(const std::string& id, const ImVec4& tgt, float spd = 12.f);
    ImU32 col(const ImVec4& c, float a = 1.f);
    bool popup();
    void close();
    void popups();
}

}


