#pragma once
#include "imgui.h"
#include <math.h>

// ── RGB helpers ──────────────────────────────────────────────
namespace cfg {

namespace rgb {
    inline float  speed      = 1.2f;

    inline ImVec4 color4(float offset = 0.f) {
        float t = (float)ImGui::GetTime() * speed + offset;
        return ImVec4(
            sinf(t)          * 0.5f + 0.5f,
            sinf(t + 2.094f) * 0.5f + 0.5f,
            sinf(t + 4.189f) * 0.5f + 0.5f,
            1.0f);
    }

    inline ImU32 color32(int alpha = 255, float offset = 0.f) {
        ImVec4 c = color4(offset);
        return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), alpha);
    }
}

namespace esp {
    // ── Toggles ──────────────────────────────────────────────
    inline bool box             = false;
    inline bool name            = false;
    inline bool health          = false;
    inline bool health_text     = false;
    inline bool distance        = false;
    inline bool line            = false;
    inline bool   head_circle     = false;
    inline ImVec4 head_circle_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   head_circle_rgb = false;
    inline bool   skeleton        = false;
    inline ImVec4 skeleton_col       = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   skeleton_rgb       = false;
    inline float  skeleton_thickness = 1.0f;
    inline ImVec4 joint_col          = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline float  joint_size         = 1.4f;
    inline bool box_fill        = false;

    // ── Box ──────────────────────────────────────────────────
    inline int   box_type       = 0;        // 0=full 1=corner
    inline float box_rounding   = 0.f;
    inline ImVec4 box_col       = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   box_rgb       = false;

    // ── Fill ─────────────────────────────────────────────────
    inline float  fill_alpha    = 0.15f;
    inline int    fill_type     = 0;        // 0=Normal 1=Gradient
    inline ImVec4 fill_col      = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   fill_rgb      = false;
    inline ImVec4 fill_col_top  = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 fill_col_bot  = ImVec4(110/255.f, 96/255.f, 170/255.f, 1.f);

    // ── Name ─────────────────────────────────────────────────
    inline ImVec4 name_col      = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline bool   name_rgb      = false;

    // ── HP bar ───────────────────────────────────────────────
    inline ImVec4 health_col    = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   health_rgb    = false;

    // ── HP text ──────────────────────────────────────────────
    inline ImVec4 hptext_col    = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline bool   hptext_rgb    = false;

    // ── Distance ─────────────────────────────────────────────
    inline ImVec4 distance_col  = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   distance_rgb  = false;

    // ── Line ─────────────────────────────────────────────────
    inline ImVec4 line_col      = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline bool   line_rgb      = false;

    // ── ESP Preview Window ────────────────────────────────────
    inline bool   preview_visible = false;

    // ── Hitlogs (наш hitlog.hpp) ─────────────────────────────
    inline bool   hitlog           = false;

    // ── Chams Body (заливка тела) ─────────────────────────────
    inline bool   chams_body         = false;
    inline ImVec4 chams_body_col     = ImVec4(0.2f, 0.6f, 1.f, 1.f);
    inline float  chams_body_alpha   = 0.18f;
    inline bool   chams_body_rgb     = false;

    // ── Hit Zone (подсветка головы/торса) ────────────────────
    inline bool   hit_zone           = false;
    inline ImVec4 hit_head_col       = ImVec4(1.f,  0.2f, 0.2f, 1.f);
    inline ImVec4 hit_body_col       = ImVec4(1.f,  0.8f, 0.f,  1.f);
    inline float  hit_zone_alpha     = 0.25f;

    // ── Footprints (следы) ────────────────────────────────────
    inline bool   footprints         = false;
    inline ImVec4 footprints_col     = ImVec4(1.f,  0.4f, 0.f,  1.f);
    inline float  footprints_life    = 3.0f;   // секунды жизни следа
    inline float  footprints_size    = 4.0f;   // размер точки следа

    // ── Shadow ESP (тень бокса) ───────────────────────────────
    inline bool   shadow_esp         = false;
    inline float  shadow_offset      = 4.0f;
    inline float  shadow_alpha       = 0.30f;

    // ── Device Tag (рандом iOS/Android) ──────────────────────
    inline bool  device_tag          = false;

    // ── Thick Bones (разная толщина костей) ───────────────────
    inline bool  thick_bones         = false;
    inline float thick_spine         = 2.0f;    // позвоночник
    inline float thick_arms          = 1.4f;    // руки
    inline float thick_legs          = 1.6f;    // ноги

    // ── Chams Skeleton ────────────────────────────────────────
    inline bool  skel_chams          = false;
    inline int   skel_style          = 0;      // 0=Normal 1=Neon 2=Rainbow
    inline float skel_thick          = 1.6f;   // базовая толщина
    inline bool  skel_joints         = false;  // кружки на суставах
    inline bool  skel_hp_color       = false;  // цвет по HP
    inline bool  skel_dist_scale     = false;  // толщина по дистанции

    // ── HP Color Box ──────────────────────────────────────────
    inline bool  hp_color_box        = false;

    // ── HP Gradient Bar ───────────────────────────────────────
    inline bool  hp_gradient_bar     = false;

    // ── Danger Zone ───────────────────────────────────────────
    inline bool  danger_zone         = false;
    inline float danger_zone_dist    = 30.f;
    inline ImVec4 danger_zone_col    = ImVec4(1.f, 0.1f, 0.1f, 1.f);

    // ── Offscreen Indicator ───────────────────────────────────
    inline bool  offscreen           = false;
    inline ImVec4 offscreen_col      = ImVec4(1.f, 0.3f, 0.f, 1.f);

    // ── Player Count Overlay ──────────────────────────────────
    inline bool  player_count        = false;

    // ── Closest Arrow ─────────────────────────────────────────
    inline bool  closest_arrow       = false;
    inline ImVec4 closest_arrow_col  = ImVec4(1.f, 0.8f, 0.f, 1.f);

    // ── Snapline to Head ─────────────────────────────────────
    inline bool   snapline_head     = false;
    inline ImVec4 snapline_head_col = ImVec4(225/255.f, 144/255.f, 144/255.f, 1.f);
    inline bool   snapline_head_rgb = false;

    // ── Bullet Traces ────────────────────────────────────────
    inline bool   bullet_trace      = false;
    inline ImVec4 bullet_trace_col  = ImVec4(1.f, 0.2f, 0.2f, 1.f);
    inline float  bullet_trace_time = 2.0f;

    // ── Armor Bar (справа от бокса) ───────────────────────────
    inline bool   armor_bar         = false;
    inline ImVec4 armor_bar_col     = ImVec4(0.2f, 0.4f, 1.f, 1.f);
    inline bool   armor_bar_rgb     = false;

    // ── Enemy Info Panel (сводный список врагов) ─────────────
    inline bool   enemy_panel              = false;
    inline float  enemy_panel_x            = 18.f;
    inline float  enemy_panel_y            = 120.f;
    inline float  enemy_panel_scale        = 1.0f;
    inline float  enemy_panel_max          = 6.f;
    inline int    enemy_panel_sort         = 0;       // 0=Distance, 1=HP
    inline bool   enemy_panel_visible_only = false;
    inline float  enemy_panel_bg_alpha     = 0.55f;
    inline ImVec4 enemy_panel_col          = ImVec4(1.f, 0.55f, 0.f, 1.f);
    inline bool   enemy_panel_rgb          = false;
    inline bool   enemy_panel_show_bar     = true;
    inline bool   enemy_panel_show_dist    = true;
    inline bool   enemy_panel_show_state   = true;
}

namespace crosshair {
    inline bool  enabled        = false;
    inline int   type           = 1;
    inline float size           = 8.0f;
    inline float thick          = 1.8f;
    inline float gap            = 2.0f;
    inline bool  outline        = true;
    inline bool  rgb            = false;
    inline ImVec4 color         = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline bool  spin           = false;
    inline float spin_speed     = 1.5f;
}

// ── Aimbot (aimbot.cpp, пока не в сборке) ────────────────────
namespace aim {
    inline bool    enabled       = false;
    inline int     bone          = 0;
    inline float   fov_size      = 70.f;
    inline float   smooth        = 5.f;
    inline bool    show_fov      = true;
    inline bool    fov_rgb       = false;
    inline float   fov_rgb_speed = 2.f;
    inline float   fov_glow      = 4.f;
    inline ImVec4  fov_color     = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
}

// Примечание: cfg::combat и cfg::arms определены в ../func/combat.hpp

// ── Наши namespace (сохранены из meduza) ─────────────────────

namespace radar {
    inline bool  enabled   = false;
    inline float size      = 180.f;
    inline float range     = 100.f;
    inline float pos_x     = 0.f;
    inline float pos_y     = 0.f;
    inline ImVec4 bg_col   = ImVec4(6/255.f,  6/255.f,  6/255.f,  0.85f);
    inline ImVec4 dot_col  = ImVec4(225/255.f, 100/255.f, 100/255.f, 1.f);
    inline ImVec4 self_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
}

namespace info_panel {
    inline bool enabled = false;
}

namespace effects {
    inline bool death_particles = false;
    inline ImVec4 particle_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
}

namespace gfx {
    inline bool low_gfx        = false;
    inline bool texture_potato = false;
}

namespace theme {
    inline bool night_mode     = true;   // macOS Dark Liquid Glass
    inline float blur_strength = 0.85f;  // Glass opacity factor
}

} // namespace cfg
