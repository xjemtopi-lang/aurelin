#pragma once

#include "imgui.h"
#include "imgui_internal.h"   // ImRect
#include "../game/game.hpp"   // matrix
#include "../other/vector3.h" // Vector3

namespace visuals {
    void draw();

    // ── Примитивы бокса ──────────────────────────────────────
    void dbox_full         (ImDrawList* dl, const ImRect& r, float alpha);
    void dbox_corner       (ImDrawList* dl, const ImRect& r, float alpha);
    void dbox_fill         (ImDrawList* dl, const ImRect& r);

    // ── ESP элементы ─────────────────────────────────────────
    void dnick             (ImDrawList* dl, const ImRect& r, const char* nick);
    void dhp_bar           (ImDrawList* dl, const ImRect& r, int hp);
    void dhp_text          (ImDrawList* dl, const ImRect& r, int hp);
    void ddist             (ImDrawList* dl, const ImRect& r, float dist);
    void dline             (ImDrawList* dl, const ImRect& r);
    void dhead_circle      (ImDrawList* dl, const ImRect& r, uint64_t player, const matrix& vm);
    void dskeleton         (ImDrawList* dl, const ImRect& r, uint64_t player, const matrix& vm);
    void dsnapline_head    (ImDrawList* dl, const ImRect& r, const ImVec2& screen_head);
    void darmor_bar        (ImDrawList* dl, const ImRect& r, int armor);

    // ── Новые ESP функции ────────────────────────────────────
    void dskel_chams       (ImDrawList* dl, const ImRect& r, int hp, float dist);
    void dhp_color_box     (ImDrawList* dl, const ImRect& r, int hp);
    void dhp_gradient_bar  (ImDrawList* dl, const ImRect& r, int hp);
    void ddanger_zone      (ImDrawList* dl, const ImRect& r, float dist);
    void doffscreen        (ImDrawList* dl, const ImVec2& world_screen,
                            float sw, float sh, const ImVec2& world_pos_screen);

    // ── Новые функции ────────────────────────────────────────
    void dchams_body   (ImDrawList* dl, const ImRect& r);
    void dhit_zone     (ImDrawList* dl, const ImRect& r);
    void dshadow_esp   (ImDrawList* dl, const ImRect& r);
    void ddevice_tag   (ImDrawList* dl, const ImRect& r, uint64_t player_addr);

    // ── Footprints (буфер следов, мировые координаты) ────────
    void footprint_push(uint64_t player_addr, const Vector3& world_foot);
    void dfootprints   (ImDrawList* dl, const matrix& vm);
    void dthick_bones  (ImDrawList* dl, const ImRect& r);

    // ── Overlay (1 раз за кадр, не per-player) ───────────────
    void dplayer_count     (ImDrawList* dl, int count);
    void dclosest_arrow    (ImDrawList* dl, float sw, float sh,
                            const ImVec2& closest_screen);

    // ── Прицел ───────────────────────────────────────────────
    void dcrosshair        (ImDrawList* dl);

    // ── Enemy Info Panel (сводный список врагов) ────────────
    void enemy_panel_push  (const char* name, int hp, float dist, int vis);
    void denemy_panel      (ImDrawList* dl);

    // ── Утилиты ──────────────────────────────────────────────
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size,
                            const ImVec2& pos, ImU32 color, const char* text);
}
