#include "cfg_system.hpp"
#include "cfg.hpp"
#include "cfg_holy.hpp"
#include "../func/combat.hpp"
#include "../func/props.hpp"
#include "../func/gfx.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace cfg_system {

namespace {
    enum CfgType { T_BOOL, T_INT, T_FLOAT, T_COL4 };

    struct Entry {
        const char* name;
        CfgType     type;
        void*       ptr;
    };

#define E(ns, f, t) { "ns.f", t, &cfg::ns::f }
#define H(ns, f, t) { "ns.f", t, &cfg_holy::ns::f }

    const Entry g_entries[] = {
        E(esp, box,            T_BOOL),
        E(esp, box_type,       T_INT),
        E(esp, box_rounding,   T_FLOAT),
        E(esp, box_col,        T_COL4),
        E(esp, box_rgb,        T_BOOL),
        E(esp, box_fill,       T_BOOL),
        E(esp, fill_alpha,     T_FLOAT),
        E(esp, fill_type,      T_INT),
        E(esp, fill_col,       T_COL4),
        E(esp, fill_rgb,       T_BOOL),
        E(esp, fill_col_top,   T_COL4),
        E(esp, fill_col_bot,   T_COL4),
        E(esp, name,           T_BOOL),
        E(esp, name_col,       T_COL4),
        E(esp, name_rgb,       T_BOOL),
        E(esp, health,         T_BOOL),
        E(esp, health_col,     T_COL4),
        E(esp, health_rgb,     T_BOOL),
        E(esp, health_text,    T_BOOL),
        E(esp, hptext_col,     T_COL4),
        E(esp, hptext_rgb,     T_BOOL),
        E(esp, distance,       T_BOOL),
        E(esp, distance_col,   T_COL4),
        E(esp, distance_rgb,   T_BOOL),
        E(esp, line,           T_BOOL),
        E(esp, line_col,       T_COL4),
        E(esp, line_rgb,       T_BOOL),
        E(esp, snapline_head,  T_BOOL),
        E(esp, snapline_head_col, T_COL4),
        E(esp, bullet_trace,     T_BOOL),
        E(esp, bullet_trace_col, T_COL4),
        E(esp, bullet_trace_time,T_FLOAT),

        E(combat, touch_aim,     T_BOOL),
        E(combat, touch_trigger, T_BOOL),
        E(combat, autowall,      T_BOOL),
        E(combat, aim_360,       T_BOOL),
        E(combat, back_camera,   T_BOOL),
        E(esp, snapline_head_rgb, T_BOOL),
        E(esp, head_circle,    T_BOOL),
        E(esp, head_circle_col,T_COL4),
        E(esp, head_circle_rgb,T_BOOL),
        E(esp, skeleton,       T_BOOL),
        E(esp, skeleton_col,   T_COL4),
        E(esp, skeleton_rgb,   T_BOOL),
        E(esp, skeleton_thickness, T_FLOAT),
        E(esp, joint_col,      T_COL4),
        E(esp, joint_size,     T_FLOAT),
        E(esp, armor_bar,      T_BOOL),
        E(esp, armor_bar_col,  T_COL4),
        E(esp, armor_bar_rgb,  T_BOOL),
        E(esp, skel_chams,     T_BOOL),
        E(esp, skel_style,     T_INT),
        E(esp, skel_thick,     T_FLOAT),
        E(esp, skel_joints,    T_BOOL),
        E(esp, skel_hp_color,  T_BOOL),
        E(esp, skel_dist_scale,T_BOOL),
        E(esp, hp_color_box,   T_BOOL),
        E(esp, hp_gradient_bar,T_BOOL),
        E(esp, danger_zone,    T_BOOL),
        E(esp, danger_zone_dist, T_FLOAT),
        E(esp, danger_zone_col,  T_COL4),
        E(esp, offscreen,      T_BOOL),
        E(esp, offscreen_col,  T_COL4),
        E(esp, player_count,   T_BOOL),
        E(esp, closest_arrow,  T_BOOL),
        E(esp, closest_arrow_col, T_COL4),
        E(esp, chams_body,     T_BOOL),
        E(esp, chams_body_col, T_COL4),
        E(esp, chams_body_alpha, T_FLOAT),
        E(esp, chams_body_rgb, T_BOOL),
        E(esp, hit_zone,       T_BOOL),
        E(esp, hit_head_col,   T_COL4),
        E(esp, hit_body_col,   T_COL4),
        E(esp, hit_zone_alpha, T_FLOAT),
        E(esp, footprints,     T_BOOL),
        E(esp, footprints_col, T_COL4),
        E(esp, footprints_life, T_FLOAT),
        E(esp, footprints_size, T_FLOAT),
        E(esp, shadow_esp,     T_BOOL),
        E(esp, shadow_offset,  T_FLOAT),
        E(esp, shadow_alpha,   T_FLOAT),
        E(esp, device_tag,     T_BOOL),
        E(esp, thick_bones,    T_BOOL),
        E(esp, thick_spine,    T_FLOAT),
        E(esp, thick_arms,     T_FLOAT),
        E(esp, thick_legs,     T_FLOAT),
        E(esp, hitlog,         T_BOOL),
        E(esp, preview_visible,T_BOOL),
        E(esp, enemy_panel,    T_BOOL),
        E(esp, enemy_panel_x,  T_FLOAT),
        E(esp, enemy_panel_y,  T_FLOAT),
        E(esp, enemy_panel_scale, T_FLOAT),
        E(esp, enemy_panel_max,   T_FLOAT),
        E(esp, enemy_panel_sort,  T_INT),
        E(esp, enemy_panel_visible_only, T_BOOL),
        E(esp, enemy_panel_bg_alpha, T_FLOAT),
        E(esp, enemy_panel_col,  T_COL4),
        E(esp, enemy_panel_rgb,  T_BOOL),
        E(esp, enemy_panel_show_bar,   T_BOOL),
        E(esp, enemy_panel_show_dist,  T_BOOL),
        E(esp, enemy_panel_show_state, T_BOOL),

        E(crosshair, enabled, T_BOOL),
        E(crosshair, type,    T_INT),
        E(crosshair, size,    T_FLOAT),
        E(crosshair, thick,   T_FLOAT),
        E(crosshair, gap,     T_FLOAT),
        E(crosshair, outline, T_BOOL),
        E(crosshair, rgb,     T_BOOL),
        E(crosshair, color,   T_COL4),
        E(crosshair, spin,    T_BOOL),
        E(crosshair, spin_speed, T_FLOAT),

        E(radar, enabled,  T_BOOL),
        E(radar, size,     T_FLOAT),
        E(radar, range,    T_FLOAT),
        E(radar, pos_x,    T_FLOAT),
        E(radar, pos_y,    T_FLOAT),
        E(radar, bg_col,   T_COL4),
        E(radar, dot_col,  T_COL4),
        E(radar, self_col, T_COL4),

        E(info_panel, enabled, T_BOOL),

        E(effects, death_particles, T_BOOL),
        E(effects, particle_col,    T_COL4),

        E(gfx, low_gfx,        T_BOOL),
        E(gfx, texture_potato, T_BOOL),

        E(aim, enabled,       T_BOOL),
        E(aim, bone,          T_INT),
        E(aim, fov_size,      T_FLOAT),
        E(aim, smooth,        T_FLOAT),
        E(aim, show_fov,      T_BOOL),
        E(aim, fov_rgb,       T_BOOL),
        E(aim, fov_rgb_speed, T_FLOAT),
        E(aim, fov_glow,      T_FLOAT),
        E(aim, fov_color,     T_COL4),

        E(combat, aimbot,                T_BOOL),
        E(combat, aimbot_hitbox,         T_INT),
        E(combat, aimbot_visible,        T_BOOL),
        E(combat, aimbot_allow_fallback, T_BOOL),
        E(combat, aimbot_fov,            T_FLOAT),
        E(combat, aimbot_smooth,         T_FLOAT),
        E(combat, aimbot_max_dist,       T_FLOAT),
        E(combat, aimbot_fov_draw,       T_BOOL),
        E(combat, aimbot_lock_line,      T_BOOL),
        E(combat, aimbot_lock_dot,       T_BOOL),
        E(combat, triggerbot,           T_BOOL),
        E(combat, trigger_delay,        T_FLOAT),
        E(combat, trigger_range,        T_FLOAT),
        E(combat, trigger_visible_only, T_BOOL),

        E(arms, enabled, T_BOOL),
        E(arms, pos_x,   T_FLOAT),
        E(arms, pos_y,   T_FLOAT),
        E(arms, pos_z,   T_FLOAT),

        H(wallshot, enabled, T_BOOL),
        H(wallshot, value,   T_INT),
        H(inf_ammo, enabled, T_BOOL),
        H(inf_ammo, value,   T_INT),
        H(strafe,   enabled, T_BOOL),
        H(bunny_hop,enabled, T_BOOL),
        H(fov_changer, enabled, T_BOOL),
        H(fov_changer, value,   T_FLOAT),
        H(sigma, enabled, T_BOOL),
        H(sigma, damage,  T_INT),
        H(props, set_score,   T_BOOL),
        H(props, score_val,   T_INT),
        H(props, set_kills,   T_BOOL),
        H(props, kills_val,   T_INT),
        H(props, set_death,   T_BOOL),
        H(props, death_val,   T_INT),
        H(props, set_assists, T_BOOL),
        H(props, assists_val, T_INT),
        H(props, set_ping,    T_BOOL),
        H(props, ping_val,    T_INT),
        H(props, set_mvp,     T_BOOL),
        H(props, hide_id,     T_BOOL),
        H(props, hide_clan,   T_BOOL),
        H(props, fake_medal,  T_BOOL),
    };

#undef E
#undef H

    const size_t CFG_COUNT = sizeof(g_entries) / sizeof(g_entries[0]);

    static char g_status[128] = "";

    static void set_status(const char* s) {
        std::snprintf(g_status, sizeof(g_status), "%s", s ? s : "");
    }

    static void make_path(int slot, char* out, size_t size) {
        std::snprintf(out, size, "/data/local/tmp/aurelin_slot%d.cfg", slot);
    }

    static void sync_gfx_flags() {
        if (cfg::gfx::low_gfx)        gfx::low_gfx_on();
        else                          gfx::low_gfx_off();
        if (cfg::gfx::texture_potato) gfx::texture_on();
        else                          gfx::texture_off();
    }

    static void save_entry(FILE* fp, const Entry& e) {
        switch (e.type) {
            case T_BOOL:
                fprintf(fp, "%s %d\n", e.name, *(bool*)e.ptr ? 1 : 0);
                break;
            case T_INT:
                fprintf(fp, "%s %d\n", e.name, *(int*)e.ptr);
                break;
            case T_FLOAT:
                fprintf(fp, "%s %.9g\n", e.name, *(float*)e.ptr);
                break;
            case T_COL4:
            {
                const ImVec4& c = *(ImVec4*)e.ptr;
                fprintf(fp, "%s %.9g %.9g %.9g %.9g\n", e.name, c.x, c.y, c.z, c.w);
                break;
            }
        }
    }

    static bool parse_line(char* line) {
        char* key = strtok(line, " \t\r\n");
        if (!key) return false;

        float vals[4] = { 0.f, 0.f, 0.f, 0.f };
        int n = 0;
        for (char* t = strtok(nullptr, " \t\r\n"); t && n < 4; t = strtok(nullptr, " \t\r\n"))
            vals[n++] = strtof(t, nullptr);

        for (size_t i = 0; i < CFG_COUNT; ++i) {
            if (strcmp(key, g_entries[i].name) != 0) continue;
            switch (g_entries[i].type) {
                case T_BOOL:  *(bool*)g_entries[i].ptr = (n >= 1 && vals[0] != 0.f); break;
                case T_INT:   *(int*)g_entries[i].ptr  = (n >= 1 ? (int)vals[0] : 0); break;
                case T_FLOAT: *(float*)g_entries[i].ptr = (n >= 1 ? vals[0] : 0.f); break;
                case T_COL4:
                {
                    ImVec4& c = *(ImVec4*)g_entries[i].ptr;
                    c.x = vals[0];
                    c.y = vals[1];
                    c.z = vals[2];
                    c.w = (n >= 4 ? vals[3] : 1.f);
                    break;
                }
            }
            return true;
        }
        return false;
    }
} // namespace

bool save_id(int slot) {
    if (slot < 1 || slot > 4) { set_status("invalid slot"); return false; }

    char path[128];
    make_path(slot, path, sizeof(path));

    FILE* fp = fopen(path, "w");
    if (!fp) { set_status("save failed: cannot open file"); return false; }

    fprintf(fp, "# aurelin.wtf config slot %d\n", slot);
    for (size_t i = 0; i < CFG_COUNT; ++i)
        save_entry(fp, g_entries[i]);

    fclose(fp);

    char msg[96];
    std::snprintf(msg, sizeof(msg), "saved slot %d", slot);
    set_status(msg);
    return true;
}

bool load_id(int slot) {
    if (slot < 1 || slot > 4) { set_status("invalid slot"); return false; }

    char path[128];
    make_path(slot, path, sizeof(path));

    FILE* fp = fopen(path, "r");
    if (!fp) { set_status("load failed: slot is empty"); return false; }

    char line[256];
    while (fgets(line, sizeof(line), fp))
        parse_line(line);
    fclose(fp);

    sync_gfx_flags();

    char msg[96];
    std::snprintf(msg, sizeof(msg), "loaded slot %d", slot);
    set_status(msg);
    return true;
}

bool slot_exists(int slot) {
    if (slot < 1 || slot > 4) return false;
    char path[128];
    make_path(slot, path, sizeof(path));
    FILE* fp = fopen(path, "r");
    if (!fp) return false;
    fclose(fp);
    return true;
}

void reset_defaults() {
    cfg::esp::box                = false;
    cfg::esp::box_type           = 0;
    cfg::esp::box_rounding       = 0.f;
    cfg::esp::box_col            = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::box_rgb            = false;
    cfg::esp::box_fill           = false;
    cfg::esp::fill_alpha         = 0.15f;
    cfg::esp::fill_type          = 0;
    cfg::esp::fill_col           = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::fill_rgb           = false;
    cfg::esp::fill_col_top       = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::fill_col_bot       = ImVec4(110/255.f, 96/255.f, 170/255.f, 1.f);

    cfg::esp::name               = false;
    cfg::esp::name_col           = ImVec4(1.f, 1.f, 1.f, 1.f);
    cfg::esp::name_rgb           = false;

    cfg::esp::health             = false;
    cfg::esp::health_col         = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::health_rgb         = false;
    cfg::esp::health_text        = false;
    cfg::esp::hptext_col         = ImVec4(1.f, 1.f, 1.f, 1.f);
    cfg::esp::hptext_rgb         = false;

    cfg::esp::distance           = false;
    cfg::esp::distance_col       = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::distance_rgb       = false;

    cfg::esp::line               = false;
    cfg::esp::line_col           = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    cfg::esp::line_rgb           = false;

    cfg::esp::snapline_head      = false;
    cfg::esp::snapline_head_col  = ImVec4(225/255.f, 144/255.f, 144/255.f, 1.f);
    cfg::esp::snapline_head_rgb  = false;

    cfg::esp::head_circle        = false;
    cfg::esp::head_circle_col    = ImVec4(1.f, 0.f, 0.f, 1.f);
    cfg::esp::head_circle_rgb    = false;

    cfg::esp::skeleton           = false;
    cfg::esp::skeleton_col       = ImVec4(1.f, 0.f, 0.f, 1.f);
    cfg::esp::skeleton_rgb       = false;
    cfg::esp::skeleton_thickness = 1.2f;
    cfg::esp::joint_col          = ImVec4(1.f, 0.f, 0.f, 1.f);
    cfg::esp::joint_size         = 2.5f;

    cfg::esp::armor_bar          = false;
    cfg::esp::armor_bar_col      = ImVec4(0.2f, 0.4f, 1.f, 1.f);
    cfg::esp::armor_bar_rgb      = false;

    cfg::esp::skel_chams         = false;
    cfg::esp::skel_style         = 0;
    cfg::esp::skel_thick         = 1.6f;
    cfg::esp::skel_joints        = false;
    cfg::esp::skel_hp_color      = false;
    cfg::esp::skel_dist_scale    = false;

    cfg::esp::hp_color_box       = false;
    cfg::esp::hp_gradient_bar    = false;
    cfg::esp::danger_zone        = false;
    cfg::esp::danger_zone_dist   = 30.f;
    cfg::esp::danger_zone_col    = ImVec4(1.f, 0.1f, 0.1f, 1.f);
    cfg::esp::offscreen          = false;
    cfg::esp::offscreen_col      = ImVec4(1.f, 0.3f, 0.f, 1.f);
    cfg::esp::player_count       = false;
    cfg::esp::closest_arrow      = false;
    cfg::esp::closest_arrow_col  = ImVec4(1.f, 0.8f, 0.f, 1.f);

    cfg::esp::chams_body         = false;
    cfg::esp::chams_body_col     = ImVec4(0.2f, 0.6f, 1.f, 1.f);
    cfg::esp::chams_body_alpha   = 0.18f;
    cfg::esp::chams_body_rgb     = false;

    cfg::esp::hit_zone           = false;
    cfg::esp::hit_head_col       = ImVec4(1.f, 0.2f, 0.2f, 1.f);
    cfg::esp::hit_body_col       = ImVec4(1.f, 0.8f, 0.f, 1.f);
    cfg::esp::hit_zone_alpha     = 0.25f;

    cfg::esp::footprints         = false;
    cfg::esp::footprints_col     = ImVec4(1.f, 0.4f, 0.f, 1.f);
    cfg::esp::footprints_life    = 3.f;
    cfg::esp::footprints_size    = 4.f;

    cfg::esp::shadow_esp         = false;
    cfg::esp::shadow_offset      = 4.f;
    cfg::esp::shadow_alpha       = 0.3f;

    cfg::esp::device_tag         = false;
    cfg::esp::thick_bones        = false;
    cfg::esp::thick_spine        = 2.f;
    cfg::esp::thick_arms         = 1.4f;
    cfg::esp::thick_legs         = 1.6f;

    cfg::esp::hitlog             = false;
    cfg::esp::preview_visible    = false;

    cfg::esp::enemy_panel             = false;
    cfg::esp::enemy_panel_x           = 18.f;
    cfg::esp::enemy_panel_y           = 120.f;
    cfg::esp::enemy_panel_scale       = 1.f;
    cfg::esp::enemy_panel_max         = 6.f;
    cfg::esp::enemy_panel_sort        = 0;
    cfg::esp::enemy_panel_visible_only = false;
    cfg::esp::enemy_panel_bg_alpha    = 0.55f;
    cfg::esp::enemy_panel_col         = ImVec4(1.f, 0.55f, 0.f, 1.f);
    cfg::esp::enemy_panel_rgb         = false;
    cfg::esp::enemy_panel_show_bar    = true;
    cfg::esp::enemy_panel_show_dist   = true;
    cfg::esp::enemy_panel_show_state  = true;

    cfg::crosshair::enabled    = false;
    cfg::crosshair::type       = 1;
    cfg::crosshair::size       = 8.f;
    cfg::crosshair::thick      = 1.8f;
    cfg::crosshair::gap        = 2.f;
    cfg::crosshair::outline    = true;
    cfg::crosshair::rgb        = false;
    cfg::crosshair::color      = ImVec4(1.f, 1.f, 1.f, 1.f);
    cfg::crosshair::spin       = false;
    cfg::crosshair::spin_speed = 1.5f;

    cfg::radar::enabled          = false;
    cfg::radar::size             = 180.f;
    cfg::radar::range            = 100.f;
    cfg::radar::pos_x            = 0.f;
    cfg::radar::pos_y            = 0.f;
    cfg::radar::bg_col           = ImVec4(6/255.f,  6/255.f,  6/255.f,  0.85f);
    cfg::radar::dot_col          = ImVec4(225/255.f, 100/255.f, 100/255.f, 1.f);
    cfg::radar::self_col         = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);

    cfg::info_panel::enabled     = false;

    cfg::effects::death_particles = false;
    cfg::effects::particle_col    = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);

    cfg::gfx::low_gfx        = false;
    cfg::gfx::texture_potato = false;

    cfg::aim::enabled       = false;
    cfg::aim::bone          = 0;
    cfg::aim::fov_size      = 70.f;
    cfg::aim::smooth        = 5.f;
    cfg::aim::show_fov      = true;
    cfg::aim::fov_rgb       = false;
    cfg::aim::fov_rgb_speed = 2.f;
    cfg::aim::fov_glow      = 4.f;
    cfg::aim::fov_color     = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);

    cfg::combat::aimbot           = false;
    cfg::combat::aimbot_hitbox    = 0;
    cfg::combat::aimbot_visible   = false;
    cfg::combat::aimbot_allow_fallback = true;
    cfg::combat::aimbot_fov       = 70.f;
    cfg::combat::aimbot_smooth    = 5.f;
    cfg::combat::aimbot_max_dist  = 250.f;
    cfg::combat::aimbot_fov_draw  = true;
    cfg::combat::aimbot_lock_line = true;
    cfg::combat::aimbot_lock_dot  = true;
    cfg::combat::triggerbot        = false;
    cfg::combat::trigger_delay     = 0.05f;
    cfg::combat::trigger_range     = 6.f;
    cfg::combat::trigger_visible_only = true;

    cfg::arms::enabled = false;
    cfg::arms::pos_x   = 0.f;
    cfg::arms::pos_y   = 0.f;
    cfg::arms::pos_z   = 0.f;

    cfg_holy::wallshot::enabled    = false;
    cfg_holy::wallshot::value      = 2147483646;
    cfg_holy::inf_ammo::enabled    = false;
    cfg_holy::inf_ammo::value      = 10000;
    cfg_holy::strafe::enabled      = false;
    cfg_holy::bunny_hop::enabled   = false;
    cfg_holy::fov_changer::enabled = false;
    cfg_holy::fov_changer::value   = 60.f;
    cfg_holy::sigma::enabled       = false;
    cfg_holy::sigma::damage        = 130;

    cfg_holy::props::set_score   = false;
    cfg_holy::props::score_val   = 0;
    cfg_holy::props::set_kills   = false;
    cfg_holy::props::kills_val   = 0;
    cfg_holy::props::set_death   = false;
    cfg_holy::props::death_val   = 0;
    cfg_holy::props::set_assists = false;
    cfg_holy::props::assists_val = 0;
    cfg_holy::props::set_ping    = false;
    cfg_holy::props::ping_val    = 0;
    cfg_holy::props::set_mvp     = false;
    cfg_holy::props::hide_id     = false;
    cfg_holy::props::hide_clan   = false;
    cfg_holy::props::fake_medal  = false;

    sync_gfx_flags();
    set_status("reset to defaults");
}

const char* last_status() { return g_status; }

} // namespace cfg_system