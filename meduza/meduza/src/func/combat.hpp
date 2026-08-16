#pragma once
#include <stdint.h>

// ── Aimbot offsets (из lowin offsets.hpp) ────────────────────────────────────
#define OFF_PM_LOCAL_PLAYER         0x70
#define OFF_PM_PLAYER_LIST          0x28
#define OFF_LIST_COUNT              0x20
#define OFF_LIST_BUFFER             0x18
#define OFF_LIST_ENTRY_BASE         0x30
#define OFF_LIST_ENTRY_STRIDE       0x18
#define OFF_PLAYER_TEAM             0x79
#define OFF_PLAYER_WEAPON_ROOT      0x88
#define OFF_WRC_ACTIVE_WEAPON       0xA0
#define OFF_WC_FIRE_BUTTON          0x148
#define OFF_WC_WEAPON_PROPS         0xA8
#define OFF_WP_WEAPON_ID            0x18
#define OFF_PLAYER_AIM_CONTROLLER   0x80
#define OFF_AIM_CONTROLLER_DATA     0x90
#define OFF_AIMDATA_PITCH           0x18
#define OFF_AIMDATA_YAW             0x1C
#define OFF_AIMDATA_PITCH_YAW_VEC3  0x18
#define OFF_AIMDATA_SECOND_VEC3     0x24
#define OFF_PLAYER_OCCLUSION        0xB8
#define OFF_OCCLUSION_CURRENT       0x34
#define OFF_OCCLUSION_NEXT          0x38
#define OFF_PLAYER_VIEW_1           0x48
#define OFF_PLAYER_VIEW_2           0x50
#define OFF_VIEW_BIPED_MAP          0x48
#define OFF_PLAYER_MAIN_CAMERA      0xE8
#define OFF_CAM_TRANSFORM           0x20
#define OFF_CAM_TRANSFORM_MATRIX    0x10
#define OFF_CAM_MATRIX_DATA         0xF0
#define OFF_PLAYER_MOVEMENT_CTRL    0x98
#define OFF_MC_TRANSFORM_DATA       0xB0
#define OFF_TD_POSITION             0x44

// Bones
#define OFF_BIPED_MAP_BONE_COUNT    22
#define OFF_BIPED_MAP_PTRS_BASE     0x20
#define OFF_TRANSFORM_NATIVE        0x10
#define OFF_NATIVE_TRANSFORM_DATA   0x38
#define OFF_NATIVE_TRANSFORM_INDEX  0x40
#define OFF_NATIVE_TRANSFORM_DIRECT 0x90
#define OFF_TRANSFORM_DATA_ARRAY    0x18
#define OFF_TRANSFORM_DATA_INDICES  0x20
#define BONE_MATRIX_DATA            0x28
#define BONE_TRANSFORM_IDX          0x30
#define BONE_MATRIX_LIST            0x18
#define BONE_MATRIX_INDICES         0x20

// Triggerbot
#define TRIGGER_SHOT_DURATION       0.08f
#define TRIGGER_MAX_RADIUS_PX       80.f
#define TRIGGER_MIN_RADIUS_PX       12.f
#define TRIGGER_REF_DIST_M          100.f

// Aimbot
#define AIMBOT_VERTICAL_FOV_DEG     70.0f
#define AIMBOT_PI                   3.14159265f
#define AIMBOT_RAD_TO_DEG           57.2957795f

// ── Config namespace ──────────────────────────────────────────────────────────
namespace cfg {
namespace combat {
    inline bool  aimbot               = false;
    inline bool  aimbot_visible       = false;
    inline float aimbot_fov           = 70.0f;
    inline float aimbot_smooth        = 5.0f;
    inline bool  aimbot_fov_draw      = true;
    inline bool  aimbot_lock_line     = true;
    inline bool  aimbot_lock_dot      = true;
    inline float aimbot_max_dist      = 250.0f;
    inline int   aimbot_hitbox        = 0;    // 0=Head, 1=Chest
    inline bool  aimbot_allow_fallback = true;

    inline bool  triggerbot           = false;
    inline float trigger_delay        = 0.05f;
    inline float trigger_range        = 30.0f;
    inline bool  trigger_visible_only = true;

    // Новые функции
    inline bool  touch_aim            = false;  // Аимбот только при касании
    inline bool  touch_trigger        = false;  // Триггер только при касании
    inline bool  autowall             = false;  // Стрельба/Аим через стены
    inline bool  aim_360              = false;  // Аимбот 360
    inline bool  back_camera          = false;  // Возврат камеры в исходную позицию
} // namespace combat
} // namespace cfg

// ── Arms config ───────────────────────────────────────────────────────────────
#define OFF_PLAYER_AIM_ANIM_CTRL    0xA0
#define OFF_AAC_VIEWMODEL_OFFSET    0xE8

namespace cfg {
namespace arms {
    inline bool  enabled = false;
    inline float pos_x   = 0.0f;
    inline float pos_y   = 0.0f;
    inline float pos_z   = 0.0f;
} // namespace arms
} // namespace cfg

// ── Public interface ──────────────────────────────────────────────────────────
namespace combat {
    void tick(uint64_t local_player);
    void aimbot_tick(uint64_t local_player);
    void aimbot_draw_fov(void* draw_list, float screen_w, float screen_h);

    const char* aimbot_selected_label();
    const char* aimbot_resolved_label();
    const char* aimbot_status_label();
    float       aimbot_target_distance();
    bool        aimbot_has_target();
}

namespace arms {
    void run(uint64_t local_player);
}
