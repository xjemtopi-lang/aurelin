#include "combat.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../game/math.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include "../ui/cfg.hpp"
#include "imgui.h"
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  INTERNAL STATE
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    struct AimRuntimeInfo {
        char selected[24];
        char resolved[24];
        char status[48];
        float distance;
        bool has_target;
        AimRuntimeInfo() : distance(0.f), has_target(false) {
            std::snprintf(selected, sizeof(selected), "Head");
            std::snprintf(resolved, sizeof(resolved), "x");
            std::snprintf(status,   sizeof(status),   "Idle");
        }
    };
    static AimRuntimeInfo g_aim_info;

    static inline bool valid_addr(uint64_t a) {
        return (a >= 0x10000ULL && a <= 0x7FFFFFFFFFFFULL);
    }

    static inline uint64_t get_weapon_ctrl(uint64_t lp) {
        if (!valid_addr(lp)) return 0;
        uint64_t wrc = rpm<uint64_t>(lp + oxorany(OFF_PLAYER_WEAPON_ROOT));
        if (!valid_addr(wrc)) return 0;
        uint64_t wc  = rpm<uint64_t>(wrc + oxorany(OFF_WRC_ACTIVE_WEAPON));
        return valid_addr(wc) ? wc : 0;
    }

    static void set_status(const char* resolved, const char* status,
                           float dist, bool has_target)
    {
        std::snprintf(g_aim_info.selected, sizeof(g_aim_info.selected),
                      "%s", cfg::combat::aimbot_hitbox == 1 ? "Bone" : "Head");
        std::snprintf(g_aim_info.resolved, sizeof(g_aim_info.resolved),
                      "%s", resolved ? resolved : "x");
        std::snprintf(g_aim_info.status,   sizeof(g_aim_info.status),
                      "%s", status   ? status   : "Idle");
        g_aim_info.distance   = dist;
        g_aim_info.has_target = has_target;
    }

    // Позиция кости через biped_map → matrix-chain (player.hpp read_biped_bone)
    // bone_mode: 0=head, 1=chest(spine2)
    static bool get_bone_pos(uint64_t player, int bone_mode, Vector3& out) {
        uint64_t map = player::biped_map(player);
        if (!player::likely_ptr(map)) return false;

        int bone = BONE_HEAD;
        if (bone_mode == 1) bone = BONE_SPINE2;

        Vector3 root = player::position(player);
        return player::read_biped_bone(map, bone, root, out);
    }

    // Visibility через occlusion (строгий Welo-стиль)
    static bool is_visible(uint64_t p) {
        uint64_t occ = rpm<uint64_t>(p + oxorany(OFF_PLAYER_OCCLUSION));
        if (valid_addr(occ)) {
            int vis = rpm<int>(occ + oxorany(OFF_OCCLUSION_CURRENT));
            int nxt = rpm<int>(occ + oxorany(OFF_OCCLUSION_NEXT));
            return (vis == 2 && nxt != 1);
        }
        // fallback: view+0x30 bool
        uint64_t view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_1));
        if (valid_addr(view))
            return rpm<bool>(view + oxorany(0x30));
        return false;
    }
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  PUBLIC ACCESSORS
// ─────────────────────────────────────────────────────────────────────────────
const char* combat::aimbot_selected_label()  { return g_aim_info.selected; }
const char* combat::aimbot_resolved_label()  { return g_aim_info.resolved; }
const char* combat::aimbot_status_label()    { return g_aim_info.status;   }
float       combat::aimbot_target_distance() { return g_aim_info.distance; }
bool        combat::aimbot_has_target()      { return g_aim_info.has_target; }

// ─────────────────────────────────────────────────────────────────────────────
//  PUBLIC TICK
// ─────────────────────────────────────────────────────────────────────────────
void combat::tick(uint64_t lp) {
    if (!cfg::combat::aimbot && !cfg::combat::triggerbot) {
        set_status("x", "Disabled", 0.f, false);
        return;
    }
    if (!valid_addr(lp)) {
        set_status("x", "No local player", 0.f, false);
        return;
    }
    aimbot_tick(lp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AIM MATH HELPERS
// ─────────────────────────────────────────────────────────────────────────────
namespace aim_internal {
    static constexpr float kPi      = AIMBOT_PI;
    static constexpr float kVFovDeg = AIMBOT_VERTICAL_FOV_DEG;
    static constexpr float kRad2Deg = AIMBOT_RAD_TO_DEG;

    static float fov_radius_px(float fov_deg, float screen_h) {
        if (screen_h <= 0.f) return 0.f;
        float c  = fov_deg < 1.f ? 1.f : (fov_deg > 179.f ? 179.f : fov_deg);
        float tv = tanf((kVFovDeg * 0.5f) * (kPi / 180.f));
        if (fabsf(tv) < 0.00001f) return 0.f;
        return ((screen_h * 0.5f) / tv) * tanf((c * 0.5f) * (kPi / 180.f));
    }

    static float normalize_yaw(float y) {
        while (y >  180.f) y -= 360.f;
        while (y < -180.f) y += 360.f;
        return y;
    }
} // namespace aim_internal

// ─────────────────────────────────────────────────────────────────────────────
//  TRIGGERBOT
// ─────────────────────────────────────────────────────────────────────────────
namespace trigger_state {
    static float timer    = 0.f;
    static float shot_dur = 0.f;
    static bool  pulsing  = false;
}

static void triggerbot_tick(
    uint64_t lp,
    uint64_t PlayerManager,
    const matrix& ViewMatrix,
    const Vector3& CameraPos,
    int LocalTeam)
{
    using namespace trigger_state;

    if (!cfg::combat::triggerbot) {
        if (pulsing || timer > 0.f) {
            uint64_t wc = get_weapon_ctrl(lp);
            if (valid_addr(wc)) wpm<uint8_t>(wc + oxorany(OFF_WC_FIRE_BUTTON), 2);
            pulsing = false;
            timer   = 0.f;
        }
        return;
    }

    uint64_t wc = get_weapon_ctrl(lp);
    if (!valid_addr(wc)) { pulsing = false; timer = 0.f; return; }

    uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(OFF_PM_PLAYER_LIST));
    if (!PlayerList) return;
    int PlayerCount = rpm<int>(PlayerList + oxorany(OFF_LIST_COUNT));
    if (PlayerCount <= 0 || PlayerCount > 128) return;
    uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(OFF_LIST_BUFFER));
    if (!ListBuffer) return;

    const float cx = g_sw * 0.5f;
    const float cy = g_sh * 0.5f;
    bool can_trigger = false;

    for (int i = 0; i < PlayerCount && !can_trigger; i++) {
        uint64_t Player = rpm<uint64_t>(
            ListBuffer + oxorany(OFF_LIST_ENTRY_BASE) +
            oxorany(OFF_LIST_ENTRY_STRIDE) * i);
        if (!Player || Player == lp) continue;

        uint8_t pt = rpm<uint8_t>(Player + oxorany(OFF_PLAYER_TEAM));
        if (pt == static_cast<uint8_t>(LocalTeam)) continue;
        if (player::health(Player) <= 0) continue;

        if (cfg::combat::trigger_visible_only) {
            if (!is_visible(Player)) continue;
        }

        Vector3 proot  = player::position(Player);
        float   pdx    = proot.x - CameraPos.x;
        float   pdy    = proot.y - CameraPos.y;
        float   pdz    = proot.z - CameraPos.z;
        float   dist_m = sqrtf(pdx*pdx + pdy*pdy + pdz*pdz);
        float   ref    = dist_m < 1.f ? 1.f : dist_m;

        float radius_px = cfg::combat::trigger_range * (TRIGGER_REF_DIST_M / ref);
        if (radius_px < TRIGGER_MIN_RADIUS_PX) radius_px = TRIGGER_MIN_RADIUS_PX;
        if (radius_px > TRIGGER_MAX_RADIUS_PX) radius_px = TRIGGER_MAX_RADIUS_PX;

        // Проверяем 3 точки: голова, грудь, позиция
        const int BONE_OFFSETS[3] = { 0, 1, -1 };
        for (int j = 0; j < 3 && !can_trigger; j++) {
            Vector3 bp;
            bool got = false;
            if (BONE_OFFSETS[j] >= 0)
                got = get_bone_pos(Player, BONE_OFFSETS[j], bp);
            else
                bp = proot, got = true;

            if (!got) continue;
            ImVec2 sc;
            if (!world_to_screen(bp, ViewMatrix, sc)) continue;
            float sdx = sc.x - cx, sdy = sc.y - cy;
            if (sqrtf(sdx*sdx + sdy*sdy) < radius_px) can_trigger = true;
        }
    }

    float dt = ImGui::GetIO().DeltaTime;

    // Touch Trigger check: если включён Touch Trigger, срабатывает только при нажатии на экран
    bool touch_active = ImGui::GetIO().MouseDown[0];
    if (cfg::combat::touch_trigger && !touch_active) {
        can_trigger = false;
    }

    if (can_trigger) {
        if (!pulsing) {
            timer += dt;
            if (timer >= cfg::combat::trigger_delay) {
                wpm<uint8_t>(wc + oxorany(OFF_WC_FIRE_BUTTON), 3);
                pulsing  = true;
                shot_dur = 0.f;
            }
        } else {
            shot_dur += dt;
            if (shot_dur >= TRIGGER_SHOT_DURATION) {
                wpm<uint8_t>(wc + oxorany(OFF_WC_FIRE_BUTTON), 2);
                pulsing = false;
                timer   = 0.f;
            }
        }
    } else {
        if (pulsing || timer > 0.f) {
            wpm<uint8_t>(wc + oxorany(OFF_WC_FIRE_BUTTON), 2);
            pulsing = false;
            timer   = 0.f;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  AIMBOT TICK
// ─────────────────────────────────────────────────────────────────────────────
void combat::aimbot_tick(uint64_t lp) {
    using namespace aim_internal;

    uint64_t PlayerManager = get_player_manager();
    if (!PlayerManager) {
        set_status("x", "No player manager", 0.f, false);
        return;
    }

    matrix  ViewMatrix = player::view_matrix(lp);
    // CameraPos через AimController→камера или fallback position
    Vector3 CameraPos;
    {
        uint64_t AimCtrlTmp = rpm<uint64_t>(lp + oxorany(OFF_PLAYER_AIM_CONTROLLER));
        uint64_t camTr = valid_addr(AimCtrlTmp) ? rpm<uint64_t>(AimCtrlTmp + 0x80) : 0;
        if (valid_addr(camTr)) {
            Vector3 cp = player::get_transform_position(camTr);
            CameraPos = (cp.x != 0 || cp.y != 0 || cp.z != 0) ? cp : player::position(lp);
        } else {
            CameraPos = player::position(lp);
        }
    }
    int LocalTeam = rpm<uint8_t>(lp + oxorany(OFF_PLAYER_TEAM));

    uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(OFF_PM_PLAYER_LIST));
    if (!PlayerList) { set_status("x", "No player list", 0.f, false); return; }

    int PlayerCount = rpm<int>(PlayerList + oxorany(OFF_LIST_COUNT));
    if (PlayerCount <= 0 || PlayerCount > 64) {
        set_status("x", "Player list empty", 0.f, false); return;
    }

    uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(OFF_LIST_BUFFER));
    if (!ListBuffer) { set_status("x", "No list buffer", 0.f, false); return; }

    // ── Triggerbot ────────────────────────────────────────────────────────────
    triggerbot_tick(lp, PlayerManager, ViewMatrix, CameraPos, LocalTeam);

    if (!cfg::combat::aimbot) {
        set_status("x", "Disabled", 0.f, false);
        return;
    }

    // ── AimController / AimingData ────────────────────────────────────────────
    uint64_t AimController = rpm<uint64_t>(lp + oxorany(OFF_PLAYER_AIM_CONTROLLER));
    if (!valid_addr(AimController)) {
        set_status("x", "No aim ctrl", 0.f, false); return;
    }
    uint64_t AimingData = rpm<uint64_t>(AimController + oxorany(OFF_AIM_CONTROLLER_DATA));
    if (!valid_addr(AimingData)) {
        set_status("x", "No aiming data", 0.f, false); return;
    }

    float current_pitch = rpm<float>(AimingData + oxorany(OFF_AIMDATA_PITCH));
    float current_yaw   = rpm<float>(AimingData + oxorany(OFF_AIMDATA_YAW));
    if (current_pitch != current_pitch || current_yaw != current_yaw) return; // NaN guard

    current_yaw = normalize_yaw(current_yaw);

    // Weapon-change guard
    static int   s_last_weapon = 0;
    static float s_last_pitch  = 0.f;
    static float s_last_yaw    = 0.f;
    {
        int cur_wid = 0;
        uint64_t wrc = rpm<uint64_t>(lp + oxorany(OFF_PLAYER_WEAPON_ROOT));
        if (valid_addr(wrc)) {
            uint64_t wc = rpm<uint64_t>(wrc + oxorany(OFF_WRC_ACTIVE_WEAPON));
            if (valid_addr(wc)) {
                uint64_t props = rpm<uint64_t>(wc + oxorany(OFF_WC_WEAPON_PROPS));
                if (valid_addr(props))
                    cur_wid = rpm<int>(props + oxorany(OFF_WP_WEAPON_ID));
            }
        }
        float pd = fabsf(current_pitch - s_last_pitch);
        float yd = fabsf(normalize_yaw(current_yaw - s_last_yaw));
        if (cur_wid != s_last_weapon || pd > 45.f || yd > 45.f) {
            s_last_weapon = cur_wid;
            s_last_pitch  = current_pitch;
            s_last_yaw    = current_yaw;
        }
    }

    const ImVec2 center(g_sw * 0.5f, g_sh * 0.5f);
    const float  radius   = fov_radius_px(cfg::combat::aimbot_fov, g_sh);
    float        best_d   = radius + 1.f;
    Vector3      best_pos(0.f, 0.f, 0.f);
    float        best_dist3d = 0.f;
    bool         found    = false;

    // Сохраняем исходные углы камеры до аиминга (для Back Camera)
    static float pre_aim_pitch = 0.f;
    static float pre_aim_yaw   = 0.f;
    static bool  was_aiming    = false;

    // Touch Aim check: если включён Touch Aim, аимбот активируется только при таче экрана
    bool is_touching = ImGui::GetIO().MouseDown[0];
    if (cfg::combat::touch_aim && !is_touching) {
        if (cfg::combat::back_camera && was_aiming) {
            wpm<float>(AimingData + oxorany(OFF_AIMDATA_PITCH), pre_aim_pitch);
            wpm<float>(AimingData + oxorany(OFF_AIMDATA_YAW),   pre_aim_yaw);
            was_aiming = false;
        }
        set_status("x", "Waiting for touch", 0.f, false);
        return;
    }

    for (int i = 0; i < PlayerCount; i++) {
        uint64_t Player = rpm<uint64_t>(
            ListBuffer + oxorany(OFF_LIST_ENTRY_BASE) +
            oxorany(OFF_LIST_ENTRY_STRIDE) * i);
        if (!Player || Player == lp) continue;

        uint8_t pt = rpm<uint8_t>(Player + oxorany(OFF_PLAYER_TEAM));
        if (pt == static_cast<uint8_t>(LocalTeam)) continue;
        if (player::health(Player) <= 0) continue;

        // Если включён Autowall, игнорируем проверку видимости через стены
        if (cfg::combat::aimbot_visible && !cfg::combat::autowall) {
            if (!is_visible(Player)) continue;
        }

        Vector3 pos = player::position(Player);
        if (pos.x == 0.f && pos.y == 0.f && pos.z == 0.f) continue;

        float dx3 = pos.x - CameraPos.x;
        float dy3 = pos.y - CameraPos.y;
        float dz3 = pos.z - CameraPos.z;
        float dist3d = sqrtf(dx3*dx3 + dy3*dy3 + dz3*dz3);
        if (dist3d > cfg::combat::aimbot_max_dist) continue;

        Vector3 target_pos = pos;
        // Пытаемся взять кость; при неудаче — position (fallback)
        Vector3 bone_pos;
        bool got_bone = get_bone_pos(Player, cfg::combat::aimbot_hitbox, bone_pos);
        if (got_bone) target_pos = bone_pos;

        ImVec2 sc;
        if (!world_to_screen(target_pos, ViewMatrix, sc)) continue;

        float ddx = sc.x - center.x;
        float ddy = sc.y - center.y;
        float d   = sqrtf(ddx*ddx + ddy*ddy);

        // Если включён aim_360, игнорируем экранный FOV и берем лучшую цель в круге 360
        if (cfg::combat::aim_360 || d <= radius) {
            if (d < best_d) {
                best_d      = d;
                best_pos    = target_pos;
                best_dist3d = dist3d;
                found       = true;
            }
        }
    }

    if (!found) {
        // Если была включена функция Back Camera и цель потеряна, возвращаем углы обзора
        if (cfg::combat::back_camera && was_aiming) {
            wpm<float>(AimingData + oxorany(OFF_AIMDATA_PITCH), pre_aim_pitch);
            wpm<float>(AimingData + oxorany(OFF_AIMDATA_YAW),   pre_aim_yaw);
            was_aiming = false;
        }
        set_status("x", "No target in FOV", 0.f, false);
        return;
    }

    if (!was_aiming) {
        pre_aim_pitch = current_pitch;
        pre_aim_yaw   = current_yaw;
        was_aiming    = true;
    }

    const char* bone_label = (cfg::combat::aimbot_hitbox == 1) ? "Bone" : "Head";
    set_status(bone_label, "Target acquired", best_dist3d, true);

    // Визуалы
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (dl) {
        if (cfg::combat::aimbot_lock_line) {
            ImVec2 best_sc;
            world_to_screen(best_pos, ViewMatrix, best_sc);
            dl->AddLine(center, best_sc, IM_COL32(0,   0,   0,   180), 2.f);
            dl->AddLine(center, best_sc, IM_COL32(255, 255, 255, 220), 1.f);
        }
        if (cfg::combat::aimbot_lock_dot) {
            ImVec2 best_sc;
            world_to_screen(best_pos, ViewMatrix, best_sc);
            dl->AddCircleFilled(best_sc, 3.5f, IM_COL32(255, 255, 255, 230), 12);
            dl->AddCircle(      best_sc, 5.5f, IM_COL32(0,   0,   0,   170), 14, 1.f);
        }
    }

    // ── SMOOTH (Welo-кривая) + WRITE ─────────────────────────────────────────
    float dir_x = best_pos.x - CameraPos.x;
    float dir_y = best_pos.y - CameraPos.y;
    float dir_z = best_pos.z - CameraPos.z;
    float dist = sqrtf(dir_x*dir_x + dir_y*dir_y + dir_z*dir_z);
    if (dist < 0.001f) return;

    float s = dir_y / dist;
    if (s < -1.f) s = -1.f;
    if (s >  1.f) s =  1.f;

    float target_pitch = -asinf(s) * kRad2Deg;
    float target_yaw   = normalize_yaw(atan2f(dir_x, dir_z) * kRad2Deg);

    float out_pitch = target_pitch;
    float out_yaw   = target_yaw;

    float cfg_smooth = cfg::combat::aimbot_smooth;
    bool  hard_lock  = (cfg_smooth <= 0.0001f);

    if (!hard_lock) {
        float smooth_factor;
        if (cfg_smooth <= 1.f) {
            smooth_factor = 0.7f + 0.3f * cfg_smooth;
        } else if (cfg_smooth <= 5.f) {
            smooth_factor = 0.2f + 0.5f * (5.f - cfg_smooth) / 4.f;
        } else {
            float norm = (cfg_smooth - 5.f) / 15.f;
            if (norm > 1.f) norm = 1.f;
            smooth_factor = 0.2f * (1.f - norm) + 0.01f * norm;
        }

        float p_delta = target_pitch - current_pitch;
        float y_delta = normalize_yaw(target_yaw - current_yaw);
        float dlen    = sqrtf(p_delta*p_delta + y_delta*y_delta);
        if      (dlen > 10.f) smooth_factor *= 0.7f;
        else if (dlen <  2.f) smooth_factor *= 0.9f;

        out_pitch = current_pitch + p_delta * smooth_factor;
        out_yaw   = normalize_yaw(current_yaw + y_delta * smooth_factor);
    }

    if (out_pitch >  89.f) out_pitch =  89.f;
    if (out_pitch < -89.f) out_pitch = -89.f;
    out_yaw = normalize_yaw(out_yaw);

    // Double-check AimingData pointer не сдвинулся
    if (!valid_addr(AimingData) ||
        rpm<uint64_t>(AimController + oxorany(OFF_AIM_CONTROLLER_DATA)) != AimingData)
        return;

    float vec3_val[3] = { out_pitch, out_yaw, 0.f };
    wpm<float>(AimingData + oxorany(OFF_AIMDATA_PITCH),       out_pitch);
    wpm<float>(AimingData + oxorany(OFF_AIMDATA_YAW),         out_yaw);
    mem_write(AimingData  + oxorany(OFF_AIMDATA_PITCH_YAW_VEC3), vec3_val, sizeof(vec3_val));
    mem_write(AimingData  + oxorany(OFF_AIMDATA_SECOND_VEC3),    vec3_val, sizeof(vec3_val));

    s_last_pitch = out_pitch;
    s_last_yaw   = out_yaw;

    set_status(bone_label, "Tracking", best_dist3d, true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  FOV DRAW
// ─────────────────────────────────────────────────────────────────────────────
void combat::aimbot_draw_fov(void* draw_list, float screen_w, float screen_h) {
    if (!draw_list || !cfg::combat::aimbot || !cfg::combat::aimbot_fov_draw) return;
    if (screen_w <= 0.f || screen_h <= 0.f) return;

    ImDrawList* dl = (ImDrawList*)draw_list;
    float cx = screen_w * 0.5f;
    float cy = screen_h * 0.5f;
    float r  = aim_internal::fov_radius_px(cfg::combat::aimbot_fov, screen_h);

    dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(0,   0,   0,   130), 64, 2.f);
    dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(255, 255, 255, 180), 64, 1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ARMS
// ─────────────────────────────────────────────────────────────────────────────
void arms::run(uint64_t local_player) {
    if (!cfg::arms::enabled) return;
    if (!valid_addr(local_player)) return;

    uint64_t aim_anim_ctrl = rpm<uint64_t>(
        local_player + oxorany(OFF_PLAYER_AIM_ANIM_CTRL));
    if (!valid_addr(aim_anim_ctrl)) return;

    Vector3 pos = rpm<Vector3>(aim_anim_ctrl + oxorany(OFF_AAC_VIEWMODEL_OFFSET));

    pos.x += cfg::arms::pos_x;
    pos.y += cfg::arms::pos_y;
    pos.z += cfg::arms::pos_z;

    wpm<Vector3>(aim_anim_ctrl + oxorany(OFF_AAC_VIEWMODEL_OFFSET), pos);
}
