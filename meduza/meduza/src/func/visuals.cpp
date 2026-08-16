#include "visuals.hpp"
#include "hitlog.hpp"
#include "death_effect.hpp"
#include "../game/game.hpp"
#include "../game/offsets.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <unordered_map>

extern ImFont* espFont;

// ── Данные для нашего круглого радара ────────────────────────────────────────
namespace {
    struct EnemyEntry {
        float  world_x, world_y, world_z;
        float  local_x, local_y, local_z;
        int    health;
        float  distance;
        bool   valid;
        char   name[32];
    };
    const int MAX_ENEMIES = 64;
    EnemyEntry g_enemies[MAX_ENEMIES];
    int        g_enemy_count = 0;

    float g_view_right_x = 1.f;
    float g_view_right_z = 0.f;
    float g_view_fwd_x   = 0.f;
    float g_view_fwd_z   = 1.f;
}

// ── Head circle smooth cache (real-bone head ESP) ────────────────────────────
#include <map>
struct head_esp_cache_t {
    float last_head_to_neck = 0.f;
    float smoothed_radius   = 0.f;
};
static std::map<uint64_t, head_esp_cache_t> g_head_cache;

// ─────────────────────────────────────────────────────────────────────────────
//  INTERNAL HELPERS
// ─────────────────────────────────────────────────────────────────────────────

static inline ImU32 esp_col32(const ImVec4& c, bool rgb, float phase = 0.f) {
    if (rgb) return cfg::rgb::color32(255, phase);
    return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255),(int)(c.w*255));
}

static inline ImU32 esp_col32a(const ImVec4& c, bool rgb, int alpha, float phase = 0.f) {
    if (rgb) return cfg::rgb::color32(alpha, phase);
    return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), alpha);
}

static inline ImVec2 rotate_around(float cx, float cy,
                                   float dx, float dy, float a)
{
    float c = cosf(a), s = sinf(a);
    return ImVec2(cx + dx * c - dy * s,
                  cy + dx * s + dy * c);
}

// ─────────────────────────────────────────────────────────────────────────────
//  DrawGradientRounded
// ─────────────────────────────────────────────────────────────────────────────
static void draw_gradient_rounded(ImDrawList* dl,
                                  ImVec2 pMin, ImVec2 pMax,
                                  ImU32 col_top, ImU32 col_bot,
                                  float rnd)
{
    if (rnd < 0.5f) {
        dl->AddRectFilledMultiColor(pMin, pMax, col_top, col_top, col_bot, col_bot);
        return;
    }

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    if (w < 1.f || h < 1.f) return;

    float half_min = (w < h ? w : h) * 0.5f;
    if (rnd > half_min) rnd = half_min;

    float tr = (float)((col_top >>  0) & 0xFF);
    float tg = (float)((col_top >>  8) & 0xFF);
    float tb = (float)((col_top >> 16) & 0xFF);
    float ta = (float)((col_top >> 24) & 0xFF);
    float br = (float)((col_bot >>  0) & 0xFF);
    float bg = (float)((col_bot >>  8) & 0xFF);
    float bb = (float)((col_bot >> 16) & 0xFF);
    float ba = (float)((col_bot >> 24) & 0xFF);

    #define LGCOL(yn) IM_COL32( \
        (int)(tr + (br - tr) * (yn)), \
        (int)(tg + (bg - tg) * (yn)), \
        (int)(tb + (bb - tb) * (yn)), \
        (int)(ta + (ba - ta) * (yn))  \
    )

    const int ARC_SEGS = 8;
    const int N_PERIM  = 4 * ARC_SEGS;
    const int N_VTX    = N_PERIM + 1;
    const int N_IDX    = N_PERIM * 3;

    dl->PrimReserve(N_IDX, N_VTX);

    ImVec2      uv        = dl->_Data->TexUvWhitePixel;
    ImDrawVert* vw        = dl->_VtxWritePtr;
    ImDrawIdx*  iw        = dl->_IdxWritePtr;
    ImDrawIdx   base      = (ImDrawIdx)dl->_VtxCurrentIdx;

    float cxm = (pMin.x + pMax.x) * 0.5f;
    float cym = (pMin.y + pMax.y) * 0.5f;
    vw[0].pos = ImVec2(cxm, cym);
    vw[0].uv  = uv;
    vw[0].col = LGCOL(0.5f);

    float cx0 = pMin.x + rnd;
    float cx1 = pMax.x - rnd;
    float cy0 = pMin.y + rnd;
    float cy1 = pMax.y - rnd;

    int vi = 1;

    for (int i = 0; i < ARC_SEGS; i++) {
        float a  = IM_PI * 1.0f + (IM_PI * 0.5f) * ((float)i / (float)ARC_SEGS);
        float vx = cx0 + cosf(a) * rnd;
        float vy = cy0 + sinf(a) * rnd;
        vw[vi].pos = ImVec2(vx, vy);
        vw[vi].uv  = uv;
        vw[vi].col = LGCOL((vy - pMin.y) / h);
        vi++;
    }
    for (int i = 0; i < ARC_SEGS; i++) {
        float a  = IM_PI * 1.5f + (IM_PI * 0.5f) * ((float)i / (float)ARC_SEGS);
        float vx = cx1 + cosf(a) * rnd;
        float vy = cy0 + sinf(a) * rnd;
        vw[vi].pos = ImVec2(vx, vy);
        vw[vi].uv  = uv;
        vw[vi].col = LGCOL((vy - pMin.y) / h);
        vi++;
    }
    for (int i = 0; i < ARC_SEGS; i++) {
        float a  = 0.f + (IM_PI * 0.5f) * ((float)i / (float)ARC_SEGS);
        float vx = cx1 + cosf(a) * rnd;
        float vy = cy1 + sinf(a) * rnd;
        vw[vi].pos = ImVec2(vx, vy);
        vw[vi].uv  = uv;
        vw[vi].col = LGCOL((vy - pMin.y) / h);
        vi++;
    }
    for (int i = 0; i < ARC_SEGS; i++) {
        float a  = IM_PI * 0.5f + (IM_PI * 0.5f) * ((float)i / (float)ARC_SEGS);
        float vx = cx0 + cosf(a) * rnd;
        float vy = cy1 + sinf(a) * rnd;
        vw[vi].pos = ImVec2(vx, vy);
        vw[vi].uv  = uv;
        vw[vi].col = LGCOL((vy - pMin.y) / h);
        vi++;
    }

    for (int i = 0; i < N_PERIM; i++) {
        iw[i * 3 + 0] = base;
        iw[i * 3 + 1] = (ImDrawIdx)(base + 1 + i);
        iw[i * 3 + 2] = (ImDrawIdx)(base + 1 + (i + 1) % N_PERIM);
    }

    dl->_VtxWritePtr   += N_VTX;
    dl->_IdxWritePtr   += N_IDX;
    dl->_VtxCurrentIdx += (unsigned int)N_VTX;

    #undef LGCOL
}

// ─────────────────────────────────────────────────────────────────────────────
//  PUBLIC API
// ─────────────────────────────────────────────────────────────────────────────

void visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size,
                                 const ImVec2& pos, ImU32 color, const char* text)
{
    if (!font || !dl || !text) return;
    int a  = (color >> IM_COL32_A_SHIFT) & 0xFF;
    int s1 = (int)(a * 0.4f);
    int s2 = (int)(a * 0.7f);
    dl->AddText(font, size, ImVec2(pos.x + 2.f, pos.y + 2.f), IM_COL32(0,0,0,s1), text);
    dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0,0,0,s2), text);
    dl->AddText(font, size, pos,                               color,               text);
}

// ── Аккуратный ESP-текст: espFont + мягкая тень ──────────────────────────────
static void esp_text(ImDrawList* dl, float x, float y, float size, ImU32 col,
                     const char* text)
{
    if (!dl || !text) return;
    ImFont* f = espFont ? espFont : ImGui::GetFont();
    dl->AddText(f, size, ImVec2(x + 1.f, y + 1.f), IM_COL32(0, 0, 0, 190), text);
    dl->AddText(f, size, ImVec2(x, y),               col,                   text);
}

static ImVec2 esp_text_size(const char* text, float size) {
    ImFont* f = espFont ? espFont : ImGui::GetFont();
    return f ? f->CalcTextSizeA(size, FLT_MAX, 0.f, text) : ImVec2(0, 0);
}

// ── Full box (outline + тень) ─────────────────────────────────────────────────
void visuals::dbox_full(ImDrawList* dl, const ImRect& r, float alpha) {
    ImU32 col   = esp_col32a(cfg::esp::box_col, cfg::esp::box_rgb,
                             (int)(cfg::esp::box_col.w * alpha * 255.f), 0.f);
    ImU32 black = IM_COL32(0, 0, 0, (int)(255 * alpha));
    float rnd   = cfg::esp::box_rounding;
    dl->AddRect(r.Min, r.Max, black, rnd, 0, 1.0f);
    dl->AddRect(r.Min, r.Max, col,   rnd, 0, 1.0f);
}

// ── Corner box ───────────────────────────────────────────────────────────────
void visuals::dbox_corner(ImDrawList* dl, const ImRect& r, float alpha) {
    ImU32 col   = esp_col32a(cfg::esp::box_col, cfg::esp::box_rgb,
                             (int)(cfg::esp::box_col.w * alpha * 255.f), 0.3f);
    ImU32 black = IM_COL32(0, 0, 0, (int)(255 * alpha));
    float w  = r.Max.x - r.Min.x;
    float h  = r.Max.y - r.Min.y;
    float cl = fminf(w, h) * 0.25f;

    dl->AddLine(ImVec2(r.Min.x,      r.Min.y),      ImVec2(r.Min.x + cl, r.Min.y),      black, 1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Min.y),      ImVec2(r.Min.x,      r.Min.y + cl), black, 1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Min.y),      ImVec2(r.Min.x + cl, r.Min.y),      col,   1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Min.y),      ImVec2(r.Min.x,      r.Min.y + cl), col,   1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Min.y),      ImVec2(r.Max.x - cl, r.Min.y),      black, 1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Min.y),      ImVec2(r.Max.x,      r.Min.y + cl), black, 1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Min.y),      ImVec2(r.Max.x - cl, r.Min.y),      col,   1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Min.y),      ImVec2(r.Max.x,      r.Min.y + cl), col,   1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Max.y),      ImVec2(r.Min.x + cl, r.Max.y),      black, 1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Max.y),      ImVec2(r.Min.x,      r.Max.y - cl), black, 1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Max.y),      ImVec2(r.Min.x + cl, r.Max.y),      col,   1.0f);
    dl->AddLine(ImVec2(r.Min.x,      r.Max.y),      ImVec2(r.Min.x,      r.Max.y - cl), col,   1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Max.y),      ImVec2(r.Max.x - cl, r.Max.y),      black, 1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Max.y),      ImVec2(r.Max.x,      r.Max.y - cl), black, 1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Max.y),      ImVec2(r.Max.x - cl, r.Max.y),      col,   1.0f);
    dl->AddLine(ImVec2(r.Max.x,      r.Max.y),      ImVec2(r.Max.x,      r.Max.y - cl), col,   1.0f);
}

// ── Fill (normal / gradient) ──────────────────────────────────────────────────
void visuals::dbox_fill(ImDrawList* dl, const ImRect& r) {
    if (!cfg::esp::box_fill) return;
    float rnd = cfg::esp::box_rounding;
    int   a   = (int)(cfg::esp::fill_alpha * 255.f);

    if (cfg::esp::fill_type == 1) {
        ImVec4 ct4, cb4;
        if (cfg::esp::fill_rgb) {
            ct4 = cfg::rgb::color4(0.0f); ct4.w = cfg::esp::fill_alpha;
            cb4 = cfg::rgb::color4(0.5f); cb4.w = cfg::esp::fill_alpha;
        } else {
            ct4 = ImVec4(cfg::esp::fill_col_top.x, cfg::esp::fill_col_top.y,
                         cfg::esp::fill_col_top.z, cfg::esp::fill_alpha);
            cb4 = ImVec4(cfg::esp::fill_col_bot.x, cfg::esp::fill_col_bot.y,
                         cfg::esp::fill_col_bot.z, cfg::esp::fill_alpha);
        }
        ImU32 cu = IM_COL32((int)(ct4.x*255),(int)(ct4.y*255),(int)(ct4.z*255),(int)(ct4.w*255));
        ImU32 cl = IM_COL32((int)(cb4.x*255),(int)(cb4.y*255),(int)(cb4.z*255),(int)(cb4.w*255));
        draw_gradient_rounded(dl, r.Min, r.Max, cu, cl, rnd);
    } else {
        ImU32 col = esp_col32a(cfg::esp::fill_col, cfg::esp::fill_rgb, a, 2.0f);
        dl->AddRectFilled(r.Min, r.Max, col, rnd);
    }
}

// ── ESP line (от низа экрана к игроку) ───────────────────────────────────────
void visuals::dline(ImDrawList* dl, const ImRect& r) {
    if (!cfg::esp::line) return;
    ImVec2 src((float)g_sw * 0.5f, (float)g_sh);
    ImVec2 dst((r.Min.x + r.Max.x) * 0.5f, r.Max.y);
    ImU32 col = esp_col32(cfg::esp::line_col, cfg::esp::line_rgb, 1.5f);
    dl->AddLine(src, dst, IM_COL32(0,0,0,130), 1.8f);
    dl->AddLine(src, dst, col,                  1.2f);
}

// ── Head circle (real bone, smoothed radius + glow) ──────────────────────────
void visuals::dhead_circle(ImDrawList* dl, const ImRect& r,
                           uint64_t player, const matrix& vm)
{
    if (!cfg::esp::head_circle) return;

    player::bones_t bones;
    if (!player::get_bones(player, bones)) {
        float w      = r.Max.x - r.Min.x;
        float radius = w * 0.22f;
        if (radius < 2.f) radius = 2.f;
        ImVec2 center((r.Min.x + r.Max.x) * 0.5f, r.Min.y + radius);
        ImU32 col = esp_col32(cfg::esp::head_circle_col, cfg::esp::head_circle_rgb, 0.f);
        dl->AddCircle(center, radius + 1.5f, IM_COL32(0,0,0,200), 20, 1.8f);
        dl->AddCircle(center, radius,        col,                   20, 1.2f);
        return;
    }

    ImVec2 head_2d, neck_2d;
    if (!world_to_screen(bones.head, vm, head_2d) ||
        !world_to_screen(bones.neck, vm, neck_2d))
        return;

    float bh = fabsf(r.Max.y - r.Min.y);
    float head_to_neck = fabsf(head_2d.y - neck_2d.y);

    auto& cache = g_head_cache[player];

    if (head_to_neck > 1.0f && head_to_neck < bh * 0.5f) {
        cache.last_head_to_neck = head_to_neck;
    } else if (cache.last_head_to_neck > 0.1f) {
        head_to_neck = cache.last_head_to_neck;
    } else {
        if (head_to_neck < 1.0f) head_to_neck = 5.0f;
    }
    if (head_to_neck < 2.0f) head_to_neck = 2.0f;

    float target_radius = bh * 0.08f;

    if (cache.smoothed_radius < 0.1f)
        cache.smoothed_radius = target_radius;

    cache.smoothed_radius += (target_radius - cache.smoothed_radius) * 0.2f;
    float radius = cache.smoothed_radius;
    if (radius < 2.f) radius = 2.f;

    ImU32 col_base = esp_col32(cfg::esp::head_circle_col,
                               cfg::esp::head_circle_rgb, 0.f);

    // Чистый контур: мягкая тень + линия, без glow-колец и заливки
    dl->AddCircle(head_2d, radius + 1.2f, IM_COL32(0,0,0,180), 32, 1.6f);
    dl->AddCircle(head_2d, radius,        col_base,            32, 1.2f);
}

// ── Distance text (справа от бокса) ──────────────────────────────────────────
void visuals::ddist(ImDrawList* dl, const ImRect& r, float dist) {
    if (!cfg::esp::distance) return;
    char buf[16];
    int dm = (int)(dist + 0.5f);
    if (dm < 0) dm = 0;
    snprintf(buf, sizeof(buf), "%dm", dm);
    ImVec2 tsz = esp_text_size(buf, 13.f);
    float  tx  = r.Max.x + 5.f;
    float  ty  = (r.Min.y + r.Max.y) * 0.5f - tsz.y * 0.5f;
    ImU32 col = esp_col32(cfg::esp::distance_col, cfg::esp::distance_rgb, 0.f);
    esp_text(dl, tx, ty, 13.f, col, buf);
}

// ── Nickname (над боксом) ─────────────────────────────────────────────────────
void visuals::dnick(ImDrawList* dl, const ImRect& r, const char* nick) {
    if (!cfg::esp::name || !nick || nick[0] == '\0') return;
    ImVec2 tsz = esp_text_size(nick, 14.f);
    float  tx  = (r.Min.x + r.Max.x) * 0.5f - tsz.x * 0.5f;
    float  ty  = r.Min.y - tsz.y - 4.0f;
    ImU32 col = esp_col32(cfg::esp::name_col, cfg::esp::name_rgb, 0.6f);
    esp_text(dl, tx, ty, 14.f, col, nick);
}

// ── HP bar (слева от бокса) ───────────────────────────────────────────────────
void visuals::dhp_bar(ImDrawList* dl, const ImRect& r, int hp) {
    if (!cfg::esp::health) return;
    float bw  = 3.0f;
    float bh  = r.Max.y - r.Min.y;
    float bx  = r.Min.x - bw - 5.0f;
    float by  = r.Min.y;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(0,0,0,200));
    float pct = hp / 100.0f;
    if (pct > 1.f) pct = 1.f;
    if (pct < 0.f) pct = 0.f;
    float hpH = bh * pct;
    float hpY = by + (bh - hpH);
    ImU32 col;
    if (cfg::esp::health_rgb) {
        col = esp_col32(cfg::esp::health_col, true, 1.0f);
    } else {
        col = IM_COL32(
            (int)(cfg::esp::health_col.x * 255),
            (int)(cfg::esp::health_col.y * 255),
            (int)(cfg::esp::health_col.z * 255), 255);
    }
    dl->AddRectFilled(ImVec2(bx, hpY), ImVec2(bx + bw, by + bh), col);
    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(0,0,0,255), 0, 0, 1.0f);
}

// ── HP text (под боксом) ──────────────────────────────────────────────────────
void visuals::dhp_text(ImDrawList* dl, const ImRect& r, int hp) {
    if (!cfg::esp::health_text) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d HP", hp);
    ImVec2 tsz = esp_text_size(buf, 13.f);
    float  tx  = (r.Min.x + r.Max.x) * 0.5f - tsz.x * 0.5f;
    float  ty  = r.Max.y + 4.0f;
    ImU32 col;
    if (cfg::esp::hptext_rgb) {
        col = esp_col32(cfg::esp::hptext_col, true, 1.2f);
    } else {
        col = IM_COL32(
            (int)(cfg::esp::hptext_col.x * 255),
            (int)(cfg::esp::hptext_col.y * 255),
            (int)(cfg::esp::hptext_col.z * 255), 255);
    }
    esp_text(dl, tx, ty, 13.f, col, buf);
}

// ── Real Skeleton (реальные кости через get_bones) ──────────────────────────
void visuals::dskeleton(ImDrawList* dl, const ImRect& r,
                        uint64_t player, const matrix& vm)
{
    if (!cfg::esp::skeleton) return;

    player::bones_t bones;
    if (!player::get_bones(player, bones)) return;

    float thick = cfg::esp::skeleton_thickness;

    ImU32 col = esp_col32(cfg::esp::skeleton_col, cfg::esp::skeleton_rgb, 0.f);
    ImU32 joint_col = IM_COL32(
        (int)(cfg::esp::skeleton_col.x * 255),
        (int)(cfg::esp::skeleton_col.y * 255),
        (int)(cfg::esp::skeleton_col.z * 255),
        200);

    auto draw_line = [&](const Vector3& p1, const Vector3& p2) {
        ImVec2 s1, s2;
        if (world_to_screen(p1, vm, s1) && world_to_screen(p2, vm, s2)) {
            dl->AddLine(s1, s2, IM_COL32(0,0,0,150), thick + 1.0f);
            dl->AddLine(s1, s2, col,                  thick);
        }
    };

    auto draw_joint = [&](const Vector3& p) {
        ImVec2 s;
        if (world_to_screen(p, vm, s)) {
            float js = cfg::esp::joint_size;
            dl->AddCircleFilled(s, js + 0.8f, IM_COL32(0,0,0,170), 10);
            dl->AddCircleFilled(s, js,         joint_col,          10);
        }
    };

    draw_line(bones.head,   bones.neck);
    draw_line(bones.neck,   bones.spine2);
    draw_line(bones.spine2, bones.spine1);
    draw_line(bones.spine1, bones.spine);
    draw_line(bones.spine,  bones.pelvis);

    draw_line(bones.neck,       bones.l_shoulder);
    draw_line(bones.l_shoulder, bones.l_arm);
    draw_line(bones.l_arm,      bones.l_forearm);
    draw_line(bones.l_forearm,  bones.l_hand);

    draw_line(bones.neck,       bones.r_shoulder);
    draw_line(bones.r_shoulder, bones.r_arm);
    draw_line(bones.r_arm,      bones.r_forearm);
    draw_line(bones.r_forearm,  bones.r_hand);

    draw_line(bones.pelvis,  bones.l_thigh);
    draw_line(bones.l_thigh, bones.l_knee);
    draw_line(bones.l_knee,  bones.l_foot);

    draw_line(bones.pelvis,  bones.r_thigh);
    draw_line(bones.r_thigh, bones.r_knee);
    draw_line(bones.r_knee,  bones.r_foot);

    draw_joint(bones.head);
    draw_joint(bones.neck);
    draw_joint(bones.l_shoulder);
    draw_joint(bones.r_shoulder);
    draw_joint(bones.l_hand);
    draw_joint(bones.r_hand);
    draw_joint(bones.l_knee);
    draw_joint(bones.r_knee);
    draw_joint(bones.l_foot);
    draw_joint(bones.r_foot);
}

// ── Crosshair (8 типов) ───────────────────────────────────────────────────────
void visuals::dcrosshair(ImDrawList* fg) {
    if (!cfg::crosshair::enabled) return;

    float cx = g_sw * 0.5f;
    float cy = g_sh * 0.5f;

    float angle = cfg::crosshair::spin
        ? ((float)ImGui::GetTime() * cfg::crosshair::spin_speed)
        : 0.f;

    ImU32 col;
    if (cfg::crosshair::rgb) {
        col = cfg::rgb::color32((int)(cfg::crosshair::color.w * 255.f), 0.7f);
    } else {
        col = IM_COL32(
            (int)(cfg::crosshair::color.x * 255),
            (int)(cfg::crosshair::color.y * 255),
            (int)(cfg::crosshair::color.z * 255),
            (int)(cfg::crosshair::color.w * 255));
    }

    ImU32 outline = IM_COL32(0, 0, 0, 200);
    float s = cfg::crosshair::size;
    float t = cfg::crosshair::thick;
    float g = cfg::crosshair::gap;

    switch (cfg::crosshair::type) {

    case 0: {
        float rad = t * 1.5f;
        if (cfg::crosshair::outline)
            fg->AddCircleFilled(ImVec2(cx, cy), rad + 1.2f, outline, 16);
        fg->AddCircleFilled(ImVec2(cx, cy), rad, col, 16);
        if (cfg::crosshair::spin) {
            float orbit = s * 0.9f;
            ImVec2 sat = rotate_around(cx, cy, orbit, 0.f, angle);
            if (cfg::crosshair::outline)
                fg->AddCircleFilled(sat, rad * 0.7f + 1.f, outline, 8);
            fg->AddCircleFilled(sat, rad * 0.7f, col, 8);
        }
        break;
    }

    case 1: {
        ImVec2 l1 = rotate_around(cx, cy, -s,  0.f, angle);
        ImVec2 l2 = rotate_around(cx, cy, -g,  0.f, angle);
        ImVec2 r1 = rotate_around(cx, cy,  g,  0.f, angle);
        ImVec2 r2 = rotate_around(cx, cy,  s,  0.f, angle);
        ImVec2 u1 = rotate_around(cx, cy, 0.f, -s,  angle);
        ImVec2 u2 = rotate_around(cx, cy, 0.f, -g,  angle);
        ImVec2 d1 = rotate_around(cx, cy, 0.f,  g,  angle);
        ImVec2 d2 = rotate_around(cx, cy, 0.f,  s,  angle);
        if (cfg::crosshair::outline) {
            fg->AddLine(l1, l2, outline, t + 2.f);
            fg->AddLine(r1, r2, outline, t + 2.f);
            fg->AddLine(u1, u2, outline, t + 2.f);
            fg->AddLine(d1, d2, outline, t + 2.f);
        }
        fg->AddLine(l1, l2, col, t);
        fg->AddLine(r1, r2, col, t);
        fg->AddLine(u1, u2, col, t);
        fg->AddLine(d1, d2, col, t);
        break;
    }

    case 2: {
        if (cfg::crosshair::outline)
            fg->AddCircle(ImVec2(cx, cy), s + 1.5f, outline, 32, t + 2.f);
        fg->AddCircle(ImVec2(cx, cy), s, col, 32, t);
        if (cfg::crosshair::spin) {
            ImVec2 ta = rotate_around(cx, cy, 0.f, -(s + t + 3.f), angle);
            ImVec2 tb = rotate_around(cx, cy, 0.f, -(s - t - 1.f), angle);
            if (cfg::crosshair::outline)
                fg->AddLine(ta, tb, outline, t + 2.f);
            fg->AddLine(ta, tb, col, t + 0.5f);
        }
        break;
    }

    case 3: {
        ImVec2 ba = rotate_around(cx, cy, -s, 0.f, angle);
        ImVec2 bb = rotate_around(cx, cy,  s, 0.f, angle);
        if (cfg::crosshair::outline)
            fg->AddLine(ba, bb, outline, t + 2.f);
        fg->AddLine(ba, bb, col, t);
        if (cfg::crosshair::outline)
            fg->AddCircleFilled(ImVec2(cx, cy), t * 0.6f + 1.f, outline, 8);
        fg->AddCircleFilled(ImVec2(cx, cy), t * 0.6f, col, 8);
        break;
    }

    case 4: {
        float hs = s * 0.55f;
        ImVec2 tl  = rotate_around(cx, cy, -s,  -hs, angle);
        ImVec2 tr  = rotate_around(cx, cy,  s,  -hs, angle);
        ImVec2 bot = rotate_around(cx, cy, 0.f,  hs, angle);
        if (cfg::crosshair::outline) {
            fg->AddLine(tl, bot, outline, t + 2.f);
            fg->AddLine(tr, bot, outline, t + 2.f);
        }
        fg->AddLine(tl, bot, col, t);
        fg->AddLine(tr, bot, col, t);
        if (cfg::crosshair::outline)
            fg->AddCircleFilled(bot, t * 0.7f + 1.f, outline, 8);
        fg->AddCircleFilled(bot, t * 0.7f, col, 8);
        break;
    }

    case 5: {
        ImVec2 tp = rotate_around(cx, cy, 0.f, -s,  angle);
        ImVec2 rp = rotate_around(cx, cy,  s,  0.f, angle);
        ImVec2 bp = rotate_around(cx, cy, 0.f,  s,  angle);
        ImVec2 lp = rotate_around(cx, cy, -s,  0.f, angle);
        if (cfg::crosshair::outline) {
            fg->AddLine(tp, rp, outline, t + 2.f);
            fg->AddLine(rp, bp, outline, t + 2.f);
            fg->AddLine(bp, lp, outline, t + 2.f);
            fg->AddLine(lp, tp, outline, t + 2.f);
        }
        fg->AddLine(tp, rp, col, t);
        fg->AddLine(rp, bp, col, t);
        fg->AddLine(bp, lp, col, t);
        fg->AddLine(lp, tp, col, t);
        break;
    }

    case 6: {
        float a45 = angle + 0.7854f;
        float ss  = s * 0.65f;
        float gg  = g * 0.65f;

        ImVec2 l1  = rotate_around(cx, cy, -s,   0.f, angle);
        ImVec2 l2  = rotate_around(cx, cy, -g,   0.f, angle);
        ImVec2 r1  = rotate_around(cx, cy,  g,   0.f, angle);
        ImVec2 r2  = rotate_around(cx, cy,  s,   0.f, angle);
        ImVec2 u1  = rotate_around(cx, cy, 0.f,  -s,  angle);
        ImVec2 u2  = rotate_around(cx, cy, 0.f,  -g,  angle);
        ImVec2 d1  = rotate_around(cx, cy, 0.f,   g,  angle);
        ImVec2 d2  = rotate_around(cx, cy, 0.f,   s,  angle);
        ImVec2 dl1 = rotate_around(cx, cy, -ss,  0.f, a45);
        ImVec2 dl2 = rotate_around(cx, cy, -gg,  0.f, a45);
        ImVec2 dr1 = rotate_around(cx, cy,  gg,  0.f, a45);
        ImVec2 dr2 = rotate_around(cx, cy,  ss,  0.f, a45);
        ImVec2 du1 = rotate_around(cx, cy, 0.f,  -ss, a45);
        ImVec2 du2 = rotate_around(cx, cy, 0.f,  -gg, a45);
        ImVec2 dd1 = rotate_around(cx, cy, 0.f,   gg, a45);
        ImVec2 dd2 = rotate_around(cx, cy, 0.f,   ss, a45);

        if (cfg::crosshair::outline) {
            fg->AddLine(l1,  l2,  outline, t + 2.f);
            fg->AddLine(r1,  r2,  outline, t + 2.f);
            fg->AddLine(u1,  u2,  outline, t + 2.f);
            fg->AddLine(d1,  d2,  outline, t + 2.f);
            fg->AddLine(dl1, dl2, outline, t + 1.5f);
            fg->AddLine(dr1, dr2, outline, t + 1.5f);
            fg->AddLine(du1, du2, outline, t + 1.5f);
            fg->AddLine(dd1, dd2, outline, t + 1.5f);
        }
        fg->AddLine(l1,  l2,  col, t);
        fg->AddLine(r1,  r2,  col, t);
        fg->AddLine(u1,  u2,  col, t);
        fg->AddLine(d1,  d2,  col, t);
        fg->AddLine(dl1, dl2, col, t * 0.8f);
        fg->AddLine(dr1, dr2, col, t * 0.8f);
        fg->AddLine(du1, du2, col, t * 0.8f);
        fg->AddLine(dd1, dd2, col, t * 0.8f);
        break;
    }

    case 7: {
        ImVec2 tl = rotate_around(cx, cy, -s,  -g,  angle);
        ImVec2 tr = rotate_around(cx, cy,  s,  -g,  angle);
        ImVec2 tc = rotate_around(cx, cy, 0.f, -g,  angle);
        ImVec2 bc = rotate_around(cx, cy, 0.f,  s,  angle);
        if (cfg::crosshair::outline) {
            fg->AddLine(tl, tr, outline, t + 2.f);
            fg->AddLine(tc, bc, outline, t + 2.f);
        }
        fg->AddLine(tl, tr, col, t);
        fg->AddLine(tc, bc, col, t);
        if (cfg::crosshair::outline)
            fg->AddCircleFilled(ImVec2(cx, cy), t * 0.55f + 1.f, outline, 8);
        fg->AddCircleFilled(ImVec2(cx, cy), t * 0.55f, col, 8);
        break;
    }

    } // switch
}

// ── Snapline to Head ─────────────────────────────────────────────────────────
void visuals::dsnapline_head(ImDrawList* dl, const ImRect& r,
                              const ImVec2& screen_head)
{
    if (!cfg::esp::snapline_head) return;
    (void)r;
    ImVec2 src(g_sw * 0.5f, g_sh * 0.5f);
    ImU32 col = esp_col32(cfg::esp::snapline_head_col,
                          cfg::esp::snapline_head_rgb, 0.9f);
    dl->AddLine(src, screen_head, IM_COL32(0,0,0,130), 2.0f);
    dl->AddLine(src, screen_head, col,                  1.0f);
}

// ── Armor Bar (справа) ───────────────────────────────────────────────────────
void visuals::darmor_bar(ImDrawList* dl, const ImRect& r, int armor)
{
    if (!cfg::esp::armor_bar) return;
    float bw = 3.0f;
    float bh = r.Max.y - r.Min.y;
    float bx = r.Max.x + 5.0f;
    float by = r.Min.y;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx+bw, by+bh), IM_COL32(0,0,0,200));
    float pct = (float)armor / 100.f;
    if (pct > 1.f) pct = 1.f;
    if (pct < 0.f) pct = 0.f;
    float arH = bh * pct;
    float arY = by + (bh - arH);
    ImU32 col;
    if (cfg::esp::armor_bar_rgb)
        col = esp_col32(cfg::esp::armor_bar_col, true, 2.0f);
    else
        col = IM_COL32((int)(cfg::esp::armor_bar_col.x*255),
                       (int)(cfg::esp::armor_bar_col.y*255),
                       (int)(cfg::esp::armor_bar_col.z*255), 255);
    dl->AddRectFilled(ImVec2(bx, arY), ImVec2(bx+bw, by+bh), col);
    dl->AddRect(ImVec2(bx, by), ImVec2(bx+bw, by+bh),
                IM_COL32(0,0,0,255), 0, 0, 1.0f);
}

// ── Chams Skeleton (улучшенный скелет) ───────────────────────────────────────
void visuals::dskel_chams(ImDrawList* dl, const ImRect& r, int hp, float dist)
{
    if (!cfg::esp::skel_chams) return;

    float w   = r.Max.x - r.Min.x;
    float h   = r.Max.y - r.Min.y;
    float cx  = (r.Min.x + r.Max.x) * 0.5f;
    float top = r.Min.y;
    float bot = r.Max.y;

    float thick = cfg::esp::skel_thick;
    if (cfg::esp::skel_dist_scale) {
        float t = 1.f - (dist / 100.f);
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        thick = cfg::esp::skel_thick + t * 1.4f;
    }

    float hp_t = (float)hp / 100.f;
    if (hp_t > 1.f) hp_t = 1.f;
    if (hp_t < 0.f) hp_t = 0.f;

    ImU32 col;
    float t_now = (float)ImGui::GetTime();

    if (cfg::esp::skel_style == 2) {
        col = IM_COL32(
            (int)(sinf(t_now)          * 127.f + 128.f),
            (int)(sinf(t_now + 2.094f) * 127.f + 128.f),
            (int)(sinf(t_now + 4.189f) * 127.f + 128.f),
            255);
    } else if (cfg::esp::skel_hp_color) {
        int r_c, g_c;
        if (hp_t > 0.5f) {
            float f = (hp_t - 0.5f) * 2.f;
            r_c = (int)((1.f - f) * 255.f);
            g_c = 255;
        } else {
            float f = hp_t * 2.f;
            r_c = 255;
            g_c = (int)(f * 255.f);
        }
        col = IM_COL32(r_c, g_c, 0, 255);
    } else {
        col = esp_col32(cfg::esp::skeleton_col, cfg::esp::skeleton_rgb, 0.f);
    }

    float hcr = w * 0.22f;
    ImVec2 neck   (cx,             top + hcr * 2.0f);
    ImVec2 chest  (cx,             top + h * 0.28f);
    ImVec2 pelvis (cx,             top + h * 0.55f);
    ImVec2 l_shldr(cx - w*0.38f,  top + h * 0.28f);
    ImVec2 l_elbow(cx - w*0.44f,  top + h * 0.42f);
    ImVec2 l_hand (cx - w*0.38f,  top + h * 0.55f);
    ImVec2 r_shldr(cx + w*0.38f,  top + h * 0.28f);
    ImVec2 r_elbow(cx + w*0.44f,  top + h * 0.42f);
    ImVec2 r_hand (cx + w*0.38f,  top + h * 0.55f);
    ImVec2 l_hip  (cx - w*0.14f,  top + h * 0.55f);
    ImVec2 l_knee (cx - w*0.16f,  top + h * 0.76f);
    ImVec2 l_foot (cx - w*0.13f,  bot);
    ImVec2 r_hip  (cx + w*0.14f,  top + h * 0.55f);
    ImVec2 r_knee (cx + w*0.16f,  top + h * 0.76f);
    ImVec2 r_foot (cx + w*0.13f,  bot);

    if (cfg::esp::skel_style == 1) {
        ImU32 glow1 = (col & 0x00FFFFFF) | 0x18000000;
        ImU32 glow2 = (col & 0x00FFFFFF) | 0x30000000;
        float gt = thick + 4.f;
        float gt2 = thick + 2.f;
        dl->AddLine(neck,    chest,   glow1, gt);
        dl->AddLine(chest,   pelvis,  glow1, gt);
        dl->AddLine(chest,   l_shldr, glow1, gt);
        dl->AddLine(chest,   r_shldr, glow1, gt);
        dl->AddLine(l_shldr, l_elbow, glow1, gt);
        dl->AddLine(l_elbow, l_hand,  glow1, gt);
        dl->AddLine(r_shldr, r_elbow, glow1, gt);
        dl->AddLine(r_elbow, r_hand,  glow1, gt);
        dl->AddLine(pelvis,  l_hip,   glow1, gt);
        dl->AddLine(pelvis,  r_hip,   glow1, gt);
        dl->AddLine(l_hip,   l_knee,  glow1, gt);
        dl->AddLine(l_knee,  l_foot,  glow1, gt);
        dl->AddLine(r_hip,   r_knee,  glow1, gt);
        dl->AddLine(r_knee,  r_foot,  glow1, gt);
        dl->AddLine(neck,    chest,   glow2, gt2);
        dl->AddLine(chest,   pelvis,  glow2, gt2);
        dl->AddLine(chest,   l_shldr, glow2, gt2);
        dl->AddLine(chest,   r_shldr, glow2, gt2);
        dl->AddLine(l_shldr, l_elbow, glow2, gt2);
        dl->AddLine(l_elbow, l_hand,  glow2, gt2);
        dl->AddLine(r_shldr, r_elbow, glow2, gt2);
        dl->AddLine(r_elbow, r_hand,  glow2, gt2);
        dl->AddLine(pelvis,  l_hip,   glow2, gt2);
        dl->AddLine(pelvis,  r_hip,   glow2, gt2);
        dl->AddLine(l_hip,   l_knee,  glow2, gt2);
        dl->AddLine(l_knee,  l_foot,  glow2, gt2);
        dl->AddLine(r_hip,   r_knee,  glow2, gt2);
        dl->AddLine(r_knee,  r_foot,  glow2, gt2);
    }

    ImU32 black = IM_COL32(0,0,0,200);
    float ot = thick + 1.2f;

    #define CLINE(a,b) \
        dl->AddLine((a),(b), black, ot); \
        dl->AddLine((a),(b), col,   thick);

    CLINE(neck,    chest)
    CLINE(chest,   pelvis)
    CLINE(chest,   l_shldr)
    CLINE(chest,   r_shldr)
    CLINE(l_shldr, l_elbow)
    CLINE(l_elbow, l_hand)
    CLINE(r_shldr, r_elbow)
    CLINE(r_elbow, r_hand)
    CLINE(pelvis,  l_hip)
    CLINE(pelvis,  r_hip)
    CLINE(l_hip,   l_knee)
    CLINE(l_knee,  l_foot)
    CLINE(r_hip,   r_knee)
    CLINE(r_knee,  r_foot)
    #undef CLINE

    if (cfg::esp::skel_joints) {
        float jr = thick * 0.9f;
        if (jr < 1.5f) jr = 1.5f;
        ImU32 jc = (col & 0x00FFFFFF) | 0xFF000000;
        dl->AddCircleFilled(neck,    jr,   jc, 8);
        dl->AddCircleFilled(chest,   jr,   jc, 8);
        dl->AddCircleFilled(pelvis,  jr,   jc, 8);
        dl->AddCircleFilled(l_shldr, jr,   jc, 8);
        dl->AddCircleFilled(l_elbow, jr*0.8f, jc, 6);
        dl->AddCircleFilled(l_hand,  jr*0.7f, jc, 6);
        dl->AddCircleFilled(r_shldr, jr,   jc, 8);
        dl->AddCircleFilled(r_elbow, jr*0.8f, jc, 6);
        dl->AddCircleFilled(r_hand,  jr*0.7f, jc, 6);
        dl->AddCircleFilled(l_knee,  jr*0.8f, jc, 6);
        dl->AddCircleFilled(r_knee,  jr*0.8f, jc, 6);
        dl->AddCircleFilled(l_foot,  jr*0.6f, jc, 6);
        dl->AddCircleFilled(r_foot,  jr*0.6f, jc, 6);
    }
}

// ── HP Color Box (рамка по цвету здоровья) ───────────────────────────────────
void visuals::dhp_color_box(ImDrawList* dl, const ImRect& r, int hp)
{
    if (!cfg::esp::hp_color_box) return;
    float t = (float)hp / 100.f;
    if (t > 1.f) t = 1.f;
    if (t < 0.f) t = 0.f;
    int rc, gc;
    if (t > 0.5f) {
        float f = (t - 0.5f) * 2.f;
        rc = (int)((1.f - f) * 255.f);
        gc = 255;
    } else {
        float f = t * 2.f;
        rc = 255;
        gc = (int)(f * 255.f);
    }
    ImU32 col   = IM_COL32(rc, gc, 0, 255);
    ImU32 black = IM_COL32(0,  0,  0, 200);
    float rnd   = cfg::esp::box_rounding;
    dl->AddRect(r.Min, r.Max, black, rnd, 0, 2.0f);
    dl->AddRect(r.Min, r.Max, col,   rnd, 0, 1.4f);
}

// ── HP Gradient Bar ───────────────────────────────────────────────────────────
void visuals::dhp_gradient_bar(ImDrawList* dl, const ImRect& r, int hp)
{
    if (!cfg::esp::hp_gradient_bar) return;
    float bw = 3.0f;
    float bh = r.Max.y - r.Min.y;
    float bx = r.Min.x - bw - 5.0f;
    float by = r.Min.y;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx+bw, by+bh), IM_COL32(0,0,0,200));
    float pct = (float)hp / 100.f;
    if (pct > 1.f) pct = 1.f;
    if (pct < 0.f) pct = 0.f;
    float hpH = bh * pct;
    float hpY = by + (bh - hpH);
    ImU32 c_top = IM_COL32(0,  200, 0,   255);
    ImU32 c_bot = IM_COL32(220, 30, 30,  255);
    dl->AddRectFilledMultiColor(
        ImVec2(bx, hpY), ImVec2(bx+bw, by+bh),
        c_top, c_top, c_bot, c_bot);
    dl->AddRect(ImVec2(bx, by), ImVec2(bx+bw, by+bh),
                IM_COL32(0,0,0,255), 0, 0, 1.0f);
}

// ── Danger Zone (пульсирующий круг если враг близко) ─────────────────────────
void visuals::ddanger_zone(ImDrawList* dl, const ImRect& r, float dist)
{
    if (!cfg::esp::danger_zone) return;
    if (dist > cfg::esp::danger_zone_dist) return;

    float t   = (float)ImGui::GetTime();
    float pulse = sinf(t * 6.28f * 3.f) * 0.5f + 0.5f;

    float cx = (r.Min.x + r.Max.x) * 0.5f;
    float cy = (r.Min.y + r.Max.y) * 0.5f;
    float base_r = (r.Max.x - r.Min.x) * 0.6f;
    float anim_r = base_r + pulse * base_r * 0.4f;

    int alpha = (int)(pulse * 180.f + 40.f);
    ImU32 col = IM_COL32(
        (int)(cfg::esp::danger_zone_col.x * 255),
        (int)(cfg::esp::danger_zone_col.y * 255),
        (int)(cfg::esp::danger_zone_col.z * 255),
        alpha);

    dl->AddCircle(ImVec2(cx, cy), anim_r, col, 32, 1.5f);
    dl->AddCircle(ImVec2(cx, cy), base_r * 0.5f, col, 24, 1.0f);
}

// ── Offscreen Indicator (стрелка на краю если враг за экраном) ───────────────
void visuals::doffscreen(ImDrawList* dl, const ImVec2& sp,
                          float sw, float sh,
                          const ImVec2& /*unused*/)
{
    if (!cfg::esp::offscreen) return;

    bool on_screen = (sp.x > 0.f && sp.x < sw &&
                      sp.y > 0.f && sp.y < sh);
    if (on_screen) return;

    float margin = 20.f;
    float cx = sw * 0.5f;
    float cy = sh * 0.5f;

    float dx = sp.x - cx;
    float dy = sp.y - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.01f) return;
    float nx = dx / len;
    float ny = dy / len;

    float max_x = sw * 0.5f - margin;
    float max_y = sh * 0.5f - margin;
    float scale = 1.f;
    if (fabsf(nx) > 0.f) scale = max_x / fabsf(dx);
    if (fabsf(ny) > 0.f) {
        float sy2 = max_y / fabsf(dy);
        if (sy2 < scale) scale = sy2;
    }
    float ex = cx + dx * scale;
    float ey = cy + dy * scale;
    ex = ImClamp(ex, margin, sw - margin);
    ey = ImClamp(ey, margin, sh - margin);

    float ar = 10.f;
    float angle = atan2f(ny, nx);
    float a1 = angle;
    float a2 = angle + 2.356f;
    float a3 = angle - 2.356f;

    ImVec2 p1(ex + cosf(a1)*ar,      ey + sinf(a1)*ar);
    ImVec2 p2(ex + cosf(a2)*ar*0.6f, ey + sinf(a2)*ar*0.6f);
    ImVec2 p3(ex + cosf(a3)*ar*0.6f, ey + sinf(a3)*ar*0.6f);

    ImU32 col = IM_COL32(
        (int)(cfg::esp::offscreen_col.x * 255),
        (int)(cfg::esp::offscreen_col.y * 255),
        (int)(cfg::esp::offscreen_col.z * 255),
        220);
    ImU32 black = IM_COL32(0,0,0,200);

    dl->AddTriangleFilled(p1, p2, p3, (col & 0x00FFFFFF) | 0x60000000);
    dl->AddTriangle(p1, p2, p3, black, 2.5f);
    dl->AddTriangle(p1, p2, p3, col,   1.5f);
}

// ── Player Count Overlay ──────────────────────────────────────────────────────
void visuals::dplayer_count(ImDrawList* dl, int count)
{
    if (!cfg::esp::player_count) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "Enemies: %d", count);
    float x = 10.f;
    float y = 10.f;
    dl->AddText(ImVec2(x+1.f, y+1.f), IM_COL32(0,0,0,255), buf);
    dl->AddText(ImVec2(x,     y),
        IM_COL32(225, 225, 225, 255), buf);
}

// ── Closest Enemy Arrow (стрелка к ближайшему) ───────────────────────────────
void visuals::dclosest_arrow(ImDrawList* dl, float sw, float sh,
                              const ImVec2& closest_screen)
{
    if (!cfg::esp::closest_arrow) return;

    float cx = sw * 0.5f;
    float cy = sh * 0.5f;
    float dx = closest_screen.x - cx;
    float dy = closest_screen.y - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1.f) return;

    float angle = atan2f(dy, dx);
    float radius = 60.f;
    float ax = cx + cosf(angle) * radius;
    float ay = cy + sinf(angle) * radius;

    float ar = 12.f;
    float a1 = angle;
    float a2 = angle + 2.356f;
    float a3 = angle - 2.356f;

    ImVec2 p1(ax + cosf(a1)*ar,      ay + sinf(a1)*ar);
    ImVec2 p2(ax + cosf(a2)*ar*0.5f, ay + sinf(a2)*ar*0.5f);
    ImVec2 p3(ax + cosf(a3)*ar*0.5f, ay + sinf(a3)*ar*0.5f);

    float t = (float)ImGui::GetTime();
    float pulse = sinf(t * 4.f) * 0.3f + 0.7f;
    int alpha = (int)(pulse * 255.f);

    ImU32 col = IM_COL32(
        (int)(cfg::esp::closest_arrow_col.x * 255),
        (int)(cfg::esp::closest_arrow_col.y * 255),
        (int)(cfg::esp::closest_arrow_col.z * 255),
        alpha);

    dl->AddTriangleFilled(p1, p2, p3, (col & 0x00FFFFFF) | 0x50000000);
    dl->AddTriangle(p1, p2, p3, IM_COL32(0,0,0,200), 2.5f);
    dl->AddTriangle(p1, p2, p3, col,                  1.5f);
}

// ── Chams Body ───────────────────────────────────────────────────────────────
void visuals::dchams_body(ImDrawList* dl, const ImRect& r)
{
    if (!cfg::esp::chams_body) return;

    float w   = r.Max.x - r.Min.x;
    float h   = r.Max.y - r.Min.y;
    float cx  = (r.Min.x + r.Max.x) * 0.5f;
    float top = r.Min.y;
    float bot = r.Max.y;

    ImU32 col;
    if (cfg::esp::chams_body_rgb) {
        float t = (float)ImGui::GetTime();
        col = IM_COL32(
            (int)(sinf(t)          * 127.f + 128.f),
            (int)(sinf(t + 2.094f) * 127.f + 128.f),
            (int)(sinf(t + 4.189f) * 127.f + 128.f),
            (int)(cfg::esp::chams_body_alpha * 255.f));
    } else {
        col = IM_COL32(
            (int)(cfg::esp::chams_body_col.x * 255),
            (int)(cfg::esp::chams_body_col.y * 255),
            (int)(cfg::esp::chams_body_col.z * 255),
            (int)(cfg::esp::chams_body_alpha * 255.f));
    }

    float hcr = w * 0.22f;

    ImVec2 neck   (cx,             top + hcr * 2.0f);
    ImVec2 chest  (cx,             top + h * 0.28f);
    ImVec2 pelvis (cx,             top + h * 0.55f);
    ImVec2 l_shldr(cx - w*0.38f,  top + h * 0.28f);
    ImVec2 l_elbow(cx - w*0.44f,  top + h * 0.42f);
    ImVec2 l_hand (cx - w*0.38f,  top + h * 0.55f);
    ImVec2 r_shldr(cx + w*0.38f,  top + h * 0.28f);
    ImVec2 r_elbow(cx + w*0.44f,  top + h * 0.42f);
    ImVec2 r_hand (cx + w*0.38f,  top + h * 0.55f);
    ImVec2 l_hip  (cx - w*0.14f,  top + h * 0.55f);
    ImVec2 l_knee (cx - w*0.16f,  top + h * 0.76f);
    ImVec2 l_foot (cx - w*0.13f,  bot);
    ImVec2 r_hip  (cx + w*0.14f,  top + h * 0.55f);
    ImVec2 r_knee (cx + w*0.16f,  top + h * 0.76f);
    ImVec2 r_foot (cx + w*0.13f,  bot);

    float tb = w * 0.06f;
    float ab = w * 0.04f;
    float lb = w * 0.05f;

    auto bone_quad = [&](const ImVec2& a, const ImVec2& b, float thick) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.01f) return;
        float nx = -dy / len * thick;
        float ny =  dx / len * thick;
        ImVec2 p[4] = {
            ImVec2(a.x+nx, a.y+ny),
            ImVec2(a.x-nx, a.y-ny),
            ImVec2(b.x-nx, b.y-ny),
            ImVec2(b.x+nx, b.y+ny)
        };
        dl->AddConvexPolyFilled(p, 4, col);
    };

    ImU32 out_col = (col & 0x00FFFFFF) |
        (ImMin((int)((cfg::esp::chams_body_alpha + 0.25f) * 255.f), 255) << 24);

    auto bone_outline = [&](const ImVec2& a, const ImVec2& b, float thick) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.01f) return;
        float nx = -dy / len * thick;
        float ny =  dx / len * thick;
        ImVec2 p[4] = {
            ImVec2(a.x+nx, a.y+ny),
            ImVec2(a.x-nx, a.y-ny),
            ImVec2(b.x-nx, b.y-ny),
            ImVec2(b.x+nx, b.y+ny)
        };
        dl->AddPolyline(p, 4, out_col, true, 1.0f);
    };

    bone_quad(neck,   chest,  tb);    bone_outline(neck,   chest,  tb);
    bone_quad(chest,  pelvis, tb);    bone_outline(chest,  pelvis, tb);

    bone_quad(chest, l_shldr, ab);    bone_outline(chest, l_shldr, ab);
    bone_quad(chest, r_shldr, ab);    bone_outline(chest, r_shldr, ab);

    bone_quad(l_shldr, l_elbow, ab);  bone_outline(l_shldr, l_elbow, ab);
    bone_quad(l_elbow, l_hand,  ab);  bone_outline(l_elbow, l_hand,  ab);
    bone_quad(r_shldr, r_elbow, ab);  bone_outline(r_shldr, r_elbow, ab);
    bone_quad(r_elbow, r_hand,  ab);  bone_outline(r_elbow, r_hand,  ab);

    bone_quad(pelvis, l_hip, lb);     bone_outline(pelvis, l_hip, lb);
    bone_quad(pelvis, r_hip, lb);     bone_outline(pelvis, r_hip, lb);

    bone_quad(l_hip,  l_knee, lb);    bone_outline(l_hip,  l_knee, lb);
    bone_quad(l_knee, l_foot, lb);    bone_outline(l_knee, l_foot, lb);
    bone_quad(r_hip,  r_knee, lb);    bone_outline(r_hip,  r_knee, lb);
    bone_quad(r_knee, r_foot, lb);    bone_outline(r_knee, r_foot, lb);

    ImU32 jcol = (col & 0x00FFFFFF) | 0xCC000000;
    float jr = tb * 0.9f;
    dl->AddCircleFilled(neck,    jr,        jcol, 8);
    dl->AddCircleFilled(chest,   jr,        jcol, 8);
    dl->AddCircleFilled(pelvis,  jr,        jcol, 8);
    dl->AddCircleFilled(l_shldr, jr*0.85f, jcol, 7);
    dl->AddCircleFilled(r_shldr, jr*0.85f, jcol, 7);
    dl->AddCircleFilled(l_elbow, ab*0.85f, jcol, 6);
    dl->AddCircleFilled(r_elbow, ab*0.85f, jcol, 6);
    dl->AddCircleFilled(l_hand,  ab*0.7f,  jcol, 6);
    dl->AddCircleFilled(r_hand,  ab*0.7f,  jcol, 6);
    dl->AddCircleFilled(l_hip,   lb*0.8f,  jcol, 7);
    dl->AddCircleFilled(r_hip,   lb*0.8f,  jcol, 7);
    dl->AddCircleFilled(l_knee,  lb*0.75f, jcol, 6);
    dl->AddCircleFilled(r_knee,  lb*0.75f, jcol, 6);
    dl->AddCircleFilled(l_foot,  lb*0.55f, jcol, 6);
    dl->AddCircleFilled(r_foot,  lb*0.55f, jcol, 6);
}

// ── Hit Zone ─────────────────────────────────────────────────────────────────
void visuals::dhit_zone(ImDrawList* dl, const ImRect& r)
{
    if (!cfg::esp::hit_zone) return;

    float w   = r.Max.x - r.Min.x;
    float h   = r.Max.y - r.Min.y;
    float cx  = (r.Min.x + r.Max.x) * 0.5f;
    float top = r.Min.y;

    int a = (int)(cfg::esp::hit_zone_alpha * 255.f);

    ImU32 head_col = IM_COL32(
        (int)(cfg::esp::hit_head_col.x * 255),
        (int)(cfg::esp::hit_head_col.y * 255),
        (int)(cfg::esp::hit_head_col.z * 255), a);
    ImU32 body_col = IM_COL32(
        (int)(cfg::esp::hit_body_col.x * 255),
        (int)(cfg::esp::hit_body_col.y * 255),
        (int)(cfg::esp::hit_body_col.z * 255), a);
    ImU32 leg_col  = IM_COL32(80, 180, 255, a);

    float hcr = w * 0.22f;

    dl->AddCircleFilled(ImVec2(cx, top + hcr), hcr, head_col, 24);
    dl->AddCircle      (ImVec2(cx, top + hcr), hcr,
        (head_col & 0x00FFFFFF) | 0xAA000000, 24, 1.2f);

    float t_top = top + hcr * 2.f;
    float t_bot = top + h * 0.55f;
    float t_hw  = w * 0.36f;
    dl->AddRectFilled(
        ImVec2(cx - t_hw, t_top), ImVec2(cx + t_hw, t_bot),
        body_col, 2.f);
    dl->AddRect(
        ImVec2(cx - t_hw, t_top), ImVec2(cx + t_hw, t_bot),
        (body_col & 0x00FFFFFF) | 0xAA000000, 2.f, 0, 1.2f);

    float l_top = top + h * 0.55f;
    float l_bot = r.Max.y;
    float lhw   = w * 0.14f;
    dl->AddRectFilled(
        ImVec2(cx - lhw*1.8f, l_top), ImVec2(cx - lhw*0.1f, l_bot),
        leg_col, 2.f);
    dl->AddRectFilled(
        ImVec2(cx + lhw*0.1f, l_top), ImVec2(cx + lhw*1.8f, l_bot),
        leg_col, 2.f);
}

// ── Shadow ESP ───────────────────────────────────────────────────────────────
void visuals::dshadow_esp(ImDrawList* dl, const ImRect& r)
{
    if (!cfg::esp::shadow_esp) return;
    float off = cfg::esp::shadow_offset;
    int   a   = (int)(cfg::esp::shadow_alpha * 255.f);
    ImU32 scol = IM_COL32(0, 0, 0, a);
    float rnd  = cfg::esp::box_rounding;
    dl->AddRect(
        ImVec2(r.Min.x+off*3, r.Min.y+off*3),
        ImVec2(r.Max.x+off*3, r.Max.y+off*3),
        (scol & 0x00FFFFFF) | (ImMin(a/3,255) << 24), rnd, 0, 2.5f);
    dl->AddRect(
        ImVec2(r.Min.x+off*2, r.Min.y+off*2),
        ImVec2(r.Max.x+off*2, r.Max.y+off*2),
        (scol & 0x00FFFFFF) | (ImMin(a*2/3,255) << 24), rnd, 0, 1.8f);
    dl->AddRect(
        ImVec2(r.Min.x+off,   r.Min.y+off),
        ImVec2(r.Max.x+off,   r.Max.y+off),
        scol, rnd, 0, 1.2f);
}

// ── Footprints ───────────────────────────────────────────────────────────────
struct Footprint {
    Vector3 pos;
    float  time;
    bool   used;
};

static const int MAX_FP_PLAYERS = 32;
static const int MAX_FP_PER     = 24;

struct FootprintTrack {
    uint64_t  addr;
    Footprint pts[MAX_FP_PER];
    int       head;
    bool      active;
};

static FootprintTrack g_fp_tracks[MAX_FP_PLAYERS];
static bool           g_fp_init = false;

static void fp_init() {
    if (g_fp_init) return;
    for (int i = 0; i < MAX_FP_PLAYERS; i++) {
        g_fp_tracks[i].active = false;
        g_fp_tracks[i].addr   = 0;
        g_fp_tracks[i].head   = 0;
        for (int j = 0; j < MAX_FP_PER; j++)
            g_fp_tracks[i].pts[j].used = false;
    }
    g_fp_init = true;
}

void visuals::footprint_push(uint64_t player_addr, const Vector3& sf)
{
    if (!cfg::esp::footprints) return;
    fp_init();

    FootprintTrack* track = nullptr;
    for (int i = 0; i < MAX_FP_PLAYERS; i++) {
        if (g_fp_tracks[i].active && g_fp_tracks[i].addr == player_addr) {
            track = &g_fp_tracks[i];
            break;
        }
    }
    if (!track) {
        for (int i = 0; i < MAX_FP_PLAYERS; i++) {
            if (!g_fp_tracks[i].active) {
                g_fp_tracks[i].active = true;
                g_fp_tracks[i].addr   = player_addr;
                g_fp_tracks[i].head   = 0;
                for (int j = 0; j < MAX_FP_PER; j++)
                    g_fp_tracks[i].pts[j].used = false;
                track = &g_fp_tracks[i];
                break;
            }
        }
    }
    if (!track) return;

    float now = (float)ImGui::GetTime();

    int prev = (track->head - 1 + MAX_FP_PER) % MAX_FP_PER;
    if (track->pts[prev].used) {
        float dx = sf.x - track->pts[prev].pos.x;
        float dy = sf.y - track->pts[prev].pos.y;
        float dz = sf.z - track->pts[prev].pos.z;
        if (dx*dx + dy*dy + dz*dz < 0.36f) return;   // шаг < 0.6м
    }

    track->pts[track->head].pos  = sf;
    track->pts[track->head].time = now;
    track->pts[track->head].used = true;
    track->head = (track->head + 1) % MAX_FP_PER;
}

void visuals::dfootprints(ImDrawList* dl, const matrix& vm)
{
    if (!cfg::esp::footprints) return;
    fp_init();

    float now  = (float)ImGui::GetTime();
    float life = cfg::esp::footprints_life;
    float sz   = cfg::esp::footprints_size;

    ImU32 base_col = IM_COL32(
        (int)(cfg::esp::footprints_col.x * 255),
        (int)(cfg::esp::footprints_col.y * 255),
        (int)(cfg::esp::footprints_col.z * 255), 255);

    struct FPDraw {
        float time;
        float t;
        ImVec2 scr;
    };
    FPDraw draw_pts[MAX_FP_PER];
    int    n = 0;

    for (int i = 0; i < MAX_FP_PLAYERS; i++) {
        if (!g_fp_tracks[i].active) continue;

        for (int j = 0; j < MAX_FP_PER; j++) {
            Footprint& fp = g_fp_tracks[i].pts[j];
            if (!fp.used) continue;

            float age = now - fp.time;
            if (age > life || age < 0.f) {
                fp.used = false;
                continue;
            }

            ImVec2 scr;
            if (!world_to_screen(fp.pos, vm, scr)) continue;

            float t = 1.f - (age / life);
            if (n < MAX_FP_PER)
                draw_pts[n++] = { fp.time, t, scr };
        }

        bool any_alive = false;
        for (int j = 0; j < MAX_FP_PER; j++)
            if (g_fp_tracks[i].pts[j].used) { any_alive = true; break; }
        if (!any_alive) g_fp_tracks[i].active = false;
    }

    if (n <= 0) return;

    // сортировка от старой к свежей
    for (int a = 1; a < n; a++) {
        FPDraw key = draw_pts[a];
        int b = a - 1;
        while (b >= 0 && draw_pts[b].time > key.time) {
            draw_pts[b + 1] = draw_pts[b];
            --b;
        }
        draw_pts[b + 1] = key;
    }

    // трейл: линия между соседними точками + точка на конце
    float trail_w = sz * 1.8f;
    for (int a = 0; a < n; a++) {
        ImU32 col = (base_col & 0x00FFFFFF) |
                    ((int)(draw_pts[a].t * 220.f) << 24);
        if (a > 0) {
            dl->AddLine(draw_pts[a - 1].scr, draw_pts[a].scr,
                        col, trail_w);
            dl->AddLine(draw_pts[a - 1].scr, draw_pts[a].scr,
                        IM_COL32(0,0,0,(int)(draw_pts[a].t * 90.f)),
                        trail_w + 1.2f);
        }
        dl->AddCircleFilled(draw_pts[a].scr,
                            sz * (0.4f + draw_pts[a].t * 0.4f),
                            col, 8);
    }
}

// ── Device Tag ───────────────────────────────────────────────────────────────
void visuals::ddevice_tag(ImDrawList* dl, const ImRect& r,
                           uint64_t player_addr)
{
    if (!cfg::esp::device_tag) return;

    // Реальный детектор: Photon PlayerProperty "platform" (int).
    // 1 = Android (APK), 2 = iOS. Неизвестное → [UNK].
    int plat = player::property<int>(player_addr, oxorany("platform"));

    const char* tag;
    ImVec4      col4;
    if (plat == 1) {
        tag  = "[APK]";
        col4 = ImVec4(0.4f, 0.85f, 0.4f, 1.f);
    } else if (plat == 2) {
        tag  = "[iOS]";
        col4 = ImVec4(0.85f, 0.85f, 0.85f, 1.f);
    } else {
        tag  = "[UNK]";
        col4 = ImVec4(0.7f, 0.7f, 0.7f, 1.f);
    }

    ImU32 col   = IM_COL32((int)(col4.x*255),(int)(col4.y*255),
                            (int)(col4.z*255), 255);
    ImU32 black = IM_COL32(0,0,0,200);

    ImFont* font = ImGui::GetFont();
    float   fs   = ImGui::GetFontSize() * 0.75f;

    float tx = r.Max.x + 5.f;
    float ty = r.Min.y - fs - 2.f;

    dl->AddText(font, fs, ImVec2(tx+1.f, ty+1.f), black, tag);
    dl->AddText(font, fs, ImVec2(tx,     ty),      col,   tag);
}

// ── Thick Bones Skeleton ──────────────────────────────────────────────────────
void visuals::dthick_bones(ImDrawList* dl, const ImRect& r)
{
    if (!cfg::esp::thick_bones) return;

    float w   = r.Max.x - r.Min.x;
    float h   = r.Max.y - r.Min.y;
    float cx  = (r.Min.x + r.Max.x) * 0.5f;
    float top = r.Min.y;
    float bot = r.Max.y;

    ImU32 col   = esp_col32(cfg::esp::skeleton_col, cfg::esp::skeleton_rgb, 0.f);
    ImU32 black = IM_COL32(0,0,0,180);

    float sp = cfg::esp::thick_spine;
    float ar = cfg::esp::thick_arms;
    float lg = cfg::esp::thick_legs;

    float hcr = w * 0.22f;
    ImVec2 neck   (cx,             top + hcr * 2.0f);
    ImVec2 chest  (cx,             top + h * 0.28f);
    ImVec2 pelvis (cx,             top + h * 0.55f);
    ImVec2 l_shldr(cx - w*0.38f,  top + h * 0.28f);
    ImVec2 l_elbow(cx - w*0.44f,  top + h * 0.42f);
    ImVec2 l_hand (cx - w*0.38f,  top + h * 0.55f);
    ImVec2 r_shldr(cx + w*0.38f,  top + h * 0.28f);
    ImVec2 r_elbow(cx + w*0.44f,  top + h * 0.42f);
    ImVec2 r_hand (cx + w*0.38f,  top + h * 0.55f);
    ImVec2 l_hip  (cx - w*0.14f,  top + h * 0.55f);
    ImVec2 l_knee (cx - w*0.16f,  top + h * 0.76f);
    ImVec2 l_foot (cx - w*0.13f,  bot);
    ImVec2 r_hip  (cx + w*0.14f,  top + h * 0.55f);
    ImVec2 r_knee (cx + w*0.16f,  top + h * 0.76f);
    ImVec2 r_foot (cx + w*0.13f,  bot);

#define TBONE(a, b, t) \
    dl->AddLine((a),(b), black, (t)+1.2f); \
    dl->AddLine((a),(b), col,   (t));

    TBONE(neck,    chest,   sp)
    TBONE(chest,   pelvis,  sp)

    TBONE(chest,   l_shldr, ar)
    TBONE(chest,   r_shldr, ar)

    TBONE(l_shldr, l_elbow, ar)
    TBONE(l_elbow, l_hand,  ar * 0.8f)
    TBONE(r_shldr, r_elbow, ar)
    TBONE(r_elbow, r_hand,  ar * 0.8f)

    TBONE(pelvis,  l_hip,   lg * 0.8f)
    TBONE(pelvis,  r_hip,   lg * 0.8f)

    TBONE(l_hip,   l_knee,  lg)
    TBONE(l_knee,  l_foot,  lg * 0.85f)
    TBONE(r_hip,   r_knee,  lg)
    TBONE(r_knee,  r_foot,  lg * 0.85f)

#undef TBONE

    float js = sp * 0.45f;
    dl->AddCircleFilled(neck,    js,          col, 8);
    dl->AddCircleFilled(chest,   js,          col, 8);
    dl->AddCircleFilled(pelvis,  js,          col, 8);
    dl->AddCircleFilled(l_shldr, js*0.85f,   col, 7);
    dl->AddCircleFilled(r_shldr, js*0.85f,   col, 7);
    dl->AddCircleFilled(l_elbow, ar*0.6f,    col, 6);
    dl->AddCircleFilled(r_elbow, ar*0.6f,    col, 6);
    dl->AddCircleFilled(l_hand,  ar*0.5f,    col, 6);
    dl->AddCircleFilled(r_hand,  ar*0.5f,    col, 6);
    dl->AddCircleFilled(l_knee,  lg*0.55f,   col, 6);
    dl->AddCircleFilled(r_knee,  lg*0.55f,   col, 6);
    dl->AddCircleFilled(l_foot,  lg*0.4f,    col, 6);
    dl->AddCircleFilled(r_foot,  lg*0.4f,    col, 6);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ENEMY INFO PANEL — сводный список врагов
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    struct EnemyPanelEntry {
        char  name[32];
        int   hp;
        float dist;
        int   vis;
    };

    static EnemyPanelEntry g_ep_entries[16];
    static int             g_ep_count = 0;

    static inline void ep_copy_name(char* dst, const char* src) {
        if (!src) { dst[0] = '\0'; return; }
        int i = 0;
        for (; i < 31 && src[i]; ++i) dst[i] = src[i];
        dst[i] = '\0';
    }

    static void ep_sort_by_dist(EnemyPanelEntry* a, int n) {
        for (int i = 1; i < n; ++i) {
            EnemyPanelEntry key = a[i];
            int j = i - 1;
            while (j >= 0 && a[j].dist > key.dist) { a[j+1] = a[j]; --j; }
            a[j+1] = key;
        }
    }
    static void ep_sort_by_hp(EnemyPanelEntry* a, int n) {
        for (int i = 1; i < n; ++i) {
            EnemyPanelEntry key = a[i];
            int j = i - 1;
            while (j >= 0 && a[j].hp > key.hp) { a[j+1] = a[j]; --j; }
            a[j+1] = key;
        }
    }

    static inline ImU32 ep_hp_color(int hp, int alpha) {
        if (hp < 0)   hp = 0;
        if (hp > 100) hp = 100;
        float t = (float)hp / 100.f;
        int r, g;
        if (t >= 0.5f) {
            float k = (t - 0.5f) * 2.f;
            r = (int)((1.f - k) * 255.f);
            g = 220;
        } else {
            float k = t * 2.f;
            r = 255;
            g = (int)(k * 220.f);
        }
        return IM_COL32(r, g, 40, alpha);
    }
}

void visuals::enemy_panel_push(const char* name, int hp, float dist, int vis)
{
    if (!cfg::esp::enemy_panel) return;
    if (g_ep_count >= 16) return;
    if (hp <= 0)   return;
    if (hp > 500)  return;
    if (cfg::esp::enemy_panel_visible_only && vis < 1) return;

    EnemyPanelEntry& e = g_ep_entries[g_ep_count++];
    ep_copy_name(e.name, (name && name[0]) ? name : "Enemy");
    e.hp   = hp;
    e.dist = dist;
    e.vis  = vis;
}

void visuals::denemy_panel(ImDrawList* dl)
{
    if (!cfg::esp::enemy_panel || !dl || g_ep_count <= 0) {
        g_ep_count = 0;
        return;
    }

    if (cfg::esp::enemy_panel_sort == 1) ep_sort_by_hp   (g_ep_entries, g_ep_count);
    else                                 ep_sort_by_dist (g_ep_entries, g_ep_count);

    int max_rows = (int)(cfg::esp::enemy_panel_max + 0.5f);
    if (max_rows < 1)  max_rows = 1;
    if (max_rows > 10) max_rows = 10;
    int rows = g_ep_count < max_rows ? g_ep_count : max_rows;

    float sc = cfg::esp::enemy_panel_scale;
    if (sc < 0.6f) sc = 0.6f;
    if (sc > 1.6f) sc = 1.6f;

    float row_h    = 20.f * sc;
    float pad_x    = 10.f * sc;
    float pad_y    = 8.f  * sc;
    float title_h  = 18.f * sc;
    float bar_w    = 60.f * sc;
    float bar_h    = 6.f  * sc;
    float name_w   = 110.f * sc;
    float dist_w   = cfg::esp::enemy_panel_show_dist ? (52.f * sc) : 0.f;
    float state_w  = cfg::esp::enemy_panel_show_state ? (16.f * sc) : 0.f;
    float bar_area = cfg::esp::enemy_panel_show_bar ? (bar_w + 8.f * sc) : 0.f;

    float panel_w  = pad_x * 2.f + name_w + bar_area + dist_w + state_w;
    if (panel_w < 180.f * sc) panel_w = 180.f * sc;
    float panel_h  = pad_y * 2.f + title_h + row_h * (float)rows;

    float x0 = cfg::esp::enemy_panel_x;
    float y0 = cfg::esp::enemy_panel_y;
    if (x0 < 0.f) x0 = 0.f;
    if (y0 < 0.f) y0 = 0.f;
    if (x0 + panel_w > g_sw - 4.f) x0 = g_sw - panel_w - 4.f;
    if (y0 + panel_h > g_sh - 4.f) y0 = g_sh - panel_h - 4.f;
    if (x0 < 4.f) x0 = 4.f;
    if (y0 < 4.f) y0 = 4.f;

    ImVec2 p_min(x0, y0);
    ImVec2 p_max(x0 + panel_w, y0 + panel_h);

    int bg_a = (int)(cfg::esp::enemy_panel_bg_alpha * 255.f);
    if (bg_a < 0)   bg_a = 0;
    if (bg_a > 255) bg_a = 255;
    dl->AddRectFilled(p_min, p_max, IM_COL32(8, 8, 10, bg_a), 6.f * sc);
    dl->AddRect      (p_min, p_max, IM_COL32(60, 60, 68, 220), 6.f * sc, 0, 1.2f);

    ImU32 accent = esp_col32(cfg::esp::enemy_panel_col, cfg::esp::enemy_panel_rgb);
    dl->AddRectFilled(
        ImVec2(p_min.x, p_min.y),
        ImVec2(p_min.x + 3.f * sc, p_max.y),
        accent, 6.f * sc,
        ImDrawFlags_RoundCornersLeft);

    ImFont* font = ImGui::GetFont();
    float   fs_h = 13.f * sc;
    float   fs_r = 12.f * sc;

    char header[64];
    snprintf(header, sizeof(header), "ENEMIES  %d", g_ep_count);
    draw_text_outlined(dl, font, fs_h,
                       ImVec2(p_min.x + pad_x, p_min.y + pad_y * 0.4f),
                       IM_COL32(230, 230, 230, 255), header);

    dl->AddLine(
        ImVec2(p_min.x + pad_x, p_min.y + pad_y + title_h - 2.f),
        ImVec2(p_max.x - pad_x, p_min.y + pad_y + title_h - 2.f),
        IM_COL32(50, 50, 55, 200), 1.f);

    float ry = p_min.y + pad_y + title_h;
    for (int i = 0; i < rows; ++i) {
        const EnemyPanelEntry& e = g_ep_entries[i];

        if ((i & 1) == 0) {
            dl->AddRectFilled(
                ImVec2(p_min.x + 4.f * sc, ry + 1.f),
                ImVec2(p_max.x - 4.f * sc, ry + row_h - 1.f),
                IM_COL32(18, 18, 22, 130), 3.f * sc);
        }

        char idx_buf[8];
        snprintf(idx_buf, sizeof(idx_buf), "%d", i + 1);
        draw_text_outlined(dl, font, fs_r,
                           ImVec2(p_min.x + pad_x, ry + (row_h - fs_r) * 0.5f),
                           accent, idx_buf);

        float col_name_x = p_min.x + pad_x + 14.f * sc;
        char name_buf[32];
        {
            int max_chars = (int)(name_w / (6.5f * sc));
            if (max_chars < 4)  max_chars = 4;
            if (max_chars > 24) max_chars = 24;
            int j = 0;
            for (; j < max_chars && e.name[j]; ++j) name_buf[j] = e.name[j];
            if (e.name[j]) {
                if (j > 2) { name_buf[j-1] = '.'; name_buf[j-2] = '.'; }
            }
            name_buf[j] = '\0';
        }
        draw_text_outlined(dl, font, fs_r,
                           ImVec2(col_name_x, ry + (row_h - fs_r) * 0.5f),
                           IM_COL32(235, 235, 235, 255), name_buf);

        float cursor_x = col_name_x + name_w;
        if (cfg::esp::enemy_panel_show_bar) {
            float bx0 = cursor_x;
            float by0 = ry + (row_h - bar_h) * 0.5f;
            float bx1 = bx0 + bar_w;
            float by1 = by0 + bar_h;
            dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1),
                              IM_COL32(0, 0, 0, 190), 2.f * sc);
            int hp_clamped = e.hp > 100 ? 100 : (e.hp < 0 ? 0 : e.hp);
            float fill_w = (float)hp_clamped / 100.f * bar_w;
            if (fill_w > 0.f) {
                dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx0 + fill_w, by1),
                                  ep_hp_color(hp_clamped, 235), 2.f * sc);
            }
            dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1),
                        IM_COL32(70, 70, 75, 220), 2.f * sc, 0, 1.f);

            char hp_buf[8];
            snprintf(hp_buf, sizeof(hp_buf), "%d", hp_clamped);
            draw_text_outlined(dl, font, fs_r,
                               ImVec2(bx1 + 4.f * sc, ry + (row_h - fs_r) * 0.5f),
                               ep_hp_color(hp_clamped, 255), hp_buf);
            cursor_x = bx1 + 4.f * sc + 22.f * sc;
        }

        if (cfg::esp::enemy_panel_show_dist) {
            char dist_buf[16];
            if (e.dist >= 100.f)      snprintf(dist_buf, sizeof(dist_buf), "%.0fm",  e.dist);
            else if (e.dist >= 10.f)  snprintf(dist_buf, sizeof(dist_buf), "%.1fm",  e.dist);
            else                      snprintf(dist_buf, sizeof(dist_buf), "%.1fm",  e.dist);
            draw_text_outlined(dl, font, fs_r,
                               ImVec2(cursor_x, ry + (row_h - fs_r) * 0.5f),
                               IM_COL32(200, 200, 205, 255), dist_buf);
            cursor_x += dist_w;
        }

        if (cfg::esp::enemy_panel_show_state) {
            ImU32 dot_col;
            if      (e.vis >= 2) dot_col = IM_COL32(80,  240, 90,  240);
            else if (e.vis == 1) dot_col = IM_COL32(240, 200, 60,  240);
            else                 dot_col = IM_COL32(120, 120, 128, 220);
            float cx = p_max.x - pad_x - 5.f * sc;
            float cy = ry + row_h * 0.5f;
            dl->AddCircleFilled(ImVec2(cx, cy), 4.f * sc, IM_COL32(0,0,0,200), 12);
            dl->AddCircleFilled(ImVec2(cx, cy), 3.f * sc, dot_col,             12);
        }

        ry += row_h;
    }

    g_ep_count = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RADAR (наш круглый, с поворотом за игроком)
// ─────────────────────────────────────────────────────────────────────────────
static void draw_radar(float alpha) {
    if (alpha < 0.01f || g_enemy_count == 0) return;
    if (!espFont) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    float R    = cfg::radar::size * 0.5f;
    float pad  = 16.f;

    float cx = g_sw - R - pad;
    float cy = g_sh - R - pad - 60.f;

    ImU32 bg = IM_COL32(
        static_cast<int>(cfg::radar::bg_col.x * 255),
        static_cast<int>(cfg::radar::bg_col.y * 255),
        static_cast<int>(cfg::radar::bg_col.z * 255),
        static_cast<int>(cfg::radar::bg_col.w * 255 * alpha));

    dl->AddCircleFilled(ImVec2(cx,cy), R + 2.f, IM_COL32(0,0,0,(int)(180*alpha)));
    dl->AddCircleFilled(ImVec2(cx,cy), R,        bg);

    ImU32 ring_col = IM_COL32(255,255,255,(int)(18*alpha));
    dl->AddCircle(ImVec2(cx,cy), R * 0.5f, ring_col, 0, 1.f);
    dl->AddCircle(ImVec2(cx,cy), R * 0.25f, ring_col, 0, 1.f);

    ImU32 cross_col = IM_COL32(255,255,255,(int)(22*alpha));
    dl->AddLine(ImVec2(cx, cy-R), ImVec2(cx, cy+R), cross_col, 1.f);
    dl->AddLine(ImVec2(cx-R, cy), ImVec2(cx+R, cy), cross_col, 1.f);

    dl->AddCircle(ImVec2(cx,cy), R,     IM_COL32(162,144,225,(int)(120*alpha)), 0, 1.5f);
    dl->AddCircle(ImVec2(cx,cy), R+1.f, IM_COL32(0,0,0,(int)(200*alpha)),       0, 1.f);

    if (espFont) {
        const char* n_lbl = "N";
        ImVec2 n_sz = espFont->CalcTextSizeA(9.f, FLT_MAX, 0.f, n_lbl);
        dl->AddText(espFont, 9.f,
            ImVec2(cx - n_sz.x*0.5f, cy - R + 3.f),
            IM_COL32(255,255,255,(int)(120*alpha)), n_lbl);
    }

    ImU32 self_col = IM_COL32(
        static_cast<int>(cfg::radar::self_col.x * 255),
        static_cast<int>(cfg::radar::self_col.y * 255),
        static_cast<int>(cfg::radar::self_col.z * 255),
        static_cast<int>(cfg::radar::self_col.w * 255 * alpha));

    dl->AddCircleFilled(ImVec2(cx, cy), 3.5f, self_col);
    dl->AddCircle(ImVec2(cx, cy),       3.5f, IM_COL32(0,0,0,(int)(180*alpha)));

    float range = cfg::radar::range;

    ImU32 dot_col = IM_COL32(
        static_cast<int>(cfg::radar::dot_col.x * 255),
        static_cast<int>(cfg::radar::dot_col.y * 255),
        static_cast<int>(cfg::radar::dot_col.z * 255),
        static_cast<int>(cfg::radar::dot_col.w * 255 * alpha));

    const float rx_ = g_view_right_x, rz_ = g_view_right_z;
    const float fx_ = g_view_fwd_x,   fz_ = g_view_fwd_z;

    for (int i = 0; i < g_enemy_count; i++) {
        const EnemyEntry& e = g_enemies[i];
        if (!e.valid) continue;

        float dx = e.world_x - e.local_x;
        float dz = e.world_z - e.local_z;

        float dist2d = sqrtf(dx*dx + dz*dz);
        if (dist2d > range || dist2d < 0.001f) continue;

        float x_local = dx * rx_ + dz * rz_;
        float z_local = dx * fx_ + dz * fz_;

        float px = cx + (x_local / range) * R;
        float py = cy - (z_local / range) * R;

        float ddx = px - cx, ddy = py - cy;
        float d = sqrtf(ddx*ddx + ddy*ddy);
        if (d > R - 3.f) {
            float s = (R - 3.f) / d;
            px = cx + ddx * s;
            py = cy + ddy * s;
        }

        float hpf = e.health / 100.f;
        ImU32 hp_col = IM_COL32(
            (int)((1.f - hpf) * 255),
            (int)(hpf * 200),
            60,
            (int)(255 * alpha));

        dl->AddCircleFilled(ImVec2(px, py), 3.f, IM_COL32(0,0,0,(int)(150*alpha)));
        dl->AddCircleFilled(ImVec2(px, py), 2.5f, hp_col);
    }

    if (espFont && g_enemy_count > 0) {
        char cnt[8];
        snprintf(cnt, sizeof(cnt), "%d", g_enemy_count);
        ImVec2 cs = espFont->CalcTextSizeA(9.f, FLT_MAX, 0.f, cnt);
        dl->AddText(espFont, 9.f,
            ImVec2(cx + R*0.6f - cs.x*0.5f, cy + R*0.72f),
            IM_COL32(255,255,255,(int)(100*alpha)), cnt);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN DRAW LOOP
// ─────────────────────────────────────────────────────────────────────────────
void visuals::draw() {
    g_enemy_count = 0;
    static bool closest_valid = false;
    static ImVec2 closest_scr(0.f, 0.f);
    static float closest_dist = 1e9f;
    closest_valid = false;
    closest_dist  = 1e9f;

    bool any = cfg::esp::box           ||
               cfg::esp::name          || cfg::esp::health      ||
               cfg::esp::health_text   || cfg::esp::distance    ||
               cfg::esp::line          || cfg::esp::head_circle ||
               cfg::esp::skeleton      || cfg::esp::box_fill    ||
               cfg::esp::snapline_head || cfg::esp::armor_bar   ||
               cfg::esp::skel_chams    || cfg::esp::device_tag  ||
               cfg::esp::thick_bones   || cfg::esp::hp_color_box ||
               cfg::esp::hp_gradient_bar ||
               cfg::esp::danger_zone   || cfg::esp::offscreen   ||
               cfg::esp::player_count  || cfg::esp::closest_arrow ||
               cfg::esp::chams_body    || cfg::esp::hit_zone      ||
               cfg::esp::shadow_esp    || cfg::esp::footprints    ||
               cfg::esp::hitlog        || cfg::esp::enemy_panel   ||
               cfg::radar::enabled     || cfg::info_panel::enabled ||
               cfg::effects::death_particles ||
               cfg::crosshair::enabled;
    if (!any) return;

    death_effect::init();
    if (cfg::effects::death_particles)
        death_effect::begin_frame();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dcrosshair(dl);

    uint64_t PlayerManager = get_player_manager();
    if (!PlayerManager) return;

    uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(OFF_PM_LOCAL_PLAYER));
    if (!LocalPlayer) return;

    matrix    ViewMatrix     = player::view_matrix(LocalPlayer);
    Vector3   LocalPosition  = player::position(LocalPlayer);
    int       LocalTeam      = rpm<uint8_t>(LocalPlayer + oxorany(OFF_PLAYER_TEAM));

    // Направления взгляда для радара (right = строка 1, forward = строка 3)
    {
        float vrx = ViewMatrix.m11, vrz = ViewMatrix.m13;
        float vfx = ViewMatrix.m31, vfz = ViewMatrix.m33;
        float vrlen = sqrtf(vrx * vrx + vrz * vrz);
        float vflen = sqrtf(vfx * vfx + vfz * vfz);
        if (vrlen > 0.0001f) { g_view_right_x = vrx / vrlen; g_view_right_z = vrz / vrlen; }
        else                 { g_view_right_x = 1.f;          g_view_right_z = 0.f; }
        if (vflen > 0.0001f) { g_view_fwd_x = vfx / vflen;    g_view_fwd_z = vfz / vflen; }
        else                 { g_view_fwd_x = 0.f;            g_view_fwd_z = 1.f; }
    }

    uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(OFF_PM_PLAYER_LIST));
    if (!PlayerList) return;

    int PlayerCount = rpm<int>(PlayerList + oxorany(OFF_LIST_COUNT));
    if (PlayerCount <= 0 || PlayerCount > 64) return;

    uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(OFF_LIST_BUFFER));
    if (!ListBuffer) return;

    for (int i = 0; i < PlayerCount; i++) {
        uint64_t Player = rpm<uint64_t>(
            ListBuffer + oxorany(OFF_LIST_ENTRY_BASE) +
            (uint64_t)oxorany(OFF_LIST_ENTRY_STRIDE) * (uint64_t)i);
        if (!Player || Player == LocalPlayer) continue;

        uint8_t PlayerTeam = rpm<uint8_t>(Player + oxorany(OFF_PLAYER_TEAM));
        if (PlayerTeam == (uint8_t)LocalTeam) continue;

        Vector3 PlayerPos = player::position(Player);
        if (PlayerPos.x == 0.f && PlayerPos.y == 0.f && PlayerPos.z == 0.f) continue;

        int Health = player::health(Player);

        // ---- Death Particle Effect (трекаем до skip по HP) ----
        if (cfg::effects::death_particles) {
            Vector3 TrackHead(PlayerPos.x, PlayerPos.y + 0.85f, PlayerPos.z);
            ImVec2  TrackScreen;
            if (world_to_screen(TrackHead, ViewMatrix, TrackScreen)) {
                ImU32 pcol = IM_COL32(
                    static_cast<int>(cfg::effects::particle_col.x * 255),
                    static_cast<int>(cfg::effects::particle_col.y * 255),
                    static_cast<int>(cfg::effects::particle_col.z * 255),
                    255);
                death_effect::track(Player, Health, TrackScreen.x, TrackScreen.y, pcol);
                death_effect::mark_seen(Player);
            }
        }

        // Имя читаем один раз (для hitlog / dnick / enemy panel)
        std::string name_str;
        {
            read_string pn = player::name(Player);
            name_str = pn.as_utf8();
        }

        if (cfg::esp::hitlog)
            hitlog::track(Player, name_str, Health);

        if (Health <= 0) continue;

        float Distance = calculate_distance(PlayerPos, LocalPosition);
        if (Distance > 500.f) continue;

        // Заполняем данные для радара / player count / closest arrow
        if (g_enemy_count < MAX_ENEMIES) {
            EnemyEntry& e = g_enemies[g_enemy_count++];
            e.world_x  = PlayerPos.x;
            e.world_y  = PlayerPos.y;
            e.world_z  = PlayerPos.z;
            e.local_x  = LocalPosition.x;
            e.local_y  = LocalPosition.y;
            e.local_z  = LocalPosition.z;
            e.health   = Health;
            e.distance = Distance;
            e.valid    = true;
            int nlen = (int)name_str.size();
            if (nlen > 31) nlen = 31;
            for (int k = 0; k < nlen; k++) e.name[k] = name_str[k];
            e.name[nlen] = '\0';
        }

        Vector3 HeadPosition(PlayerPos.x,
                             PlayerPos.y + PLAYER_HEIGHT,
                             PlayerPos.z);

        ImVec2 ScreenHead, ScreenFoot;
        if (!world_to_screen(HeadPosition,    ViewMatrix, ScreenHead)) continue;
        if (!world_to_screen(PlayerPos,       ViewMatrix, ScreenFoot)) continue;

        float bh = fabsf(ScreenFoot.y - ScreenHead.y);
        float bw = bh * 0.25f;
        float cx = (ScreenHead.x + ScreenFoot.x) * 0.5f;

        float y_top = ScreenHead.y < ScreenFoot.y ? ScreenHead.y : ScreenFoot.y;
        float y_bot = ScreenHead.y > ScreenFoot.y ? ScreenHead.y : ScreenFoot.y;

        ImRect r(ImVec2(cx - bw, y_top), ImVec2(cx + bw, y_bot));

        // Ближайший враг — экранная позиция для стрелки
        if (Distance < closest_dist) {
            closest_dist  = Distance;
            closest_scr   = ImVec2(cx, y_top + bh * 0.5f);
            closest_valid = true;
        }

        // порядок: fill → box → детали
        dbox_fill(dl, r);

        if (cfg::esp::box) {
            if (cfg::esp::box_type == 0) dbox_full  (dl, r, 1.f);
            else                         dbox_corner(dl, r, 1.f);
        }

        dline       (dl, r);
        dhead_circle(dl, r, Player, ViewMatrix);
        ddist       (dl, r, Distance);

        if (cfg::esp::name && !name_str.empty())
            dnick(dl, r, name_str.c_str());

        dhp_bar (dl, r, Health);
        dhp_text(dl, r, Health);
        dskeleton(dl, r, Player, ViewMatrix);
        dsnapline_head(dl, r, ScreenHead);
        darmor_bar(dl, r, 0);   // armor — заглушка (OFF_PLAYER_ARMOR = 0x000)

        dhp_color_box    (dl, r, Health);
        dhp_gradient_bar (dl, r, Health);
        ddanger_zone     (dl, r, Distance);
        doffscreen       (dl, ScreenHead, g_sw, g_sh, ScreenHead);
        dskel_chams      (dl, r, Health, Distance);
        dthick_bones     (dl, r);
        ddevice_tag      (dl, r, Player);

        dshadow_esp  (dl, r);
        dchams_body  (dl, r);
        dhit_zone    (dl, r);
        footprint_push(Player, PlayerPos);

        if (cfg::esp::enemy_panel) {
            int ep_vis = player::visibility_state(Player);
            enemy_panel_push(name_str.c_str(), Health, Distance, ep_vis);
        }
    }

    // ── Overlay: после цикла ──────────────────────────────────
    hitlog::render();
    dfootprints   (dl, ViewMatrix);
    dplayer_count (dl, g_enemy_count);
    if (closest_valid)
        dclosest_arrow(dl, g_sw, g_sh, closest_scr);

    if (cfg::radar::enabled)
        draw_radar(1.f);

    denemy_panel  (dl);

    // ---- Death Particle Effect render ----
    if (cfg::effects::death_particles) {
        death_effect::end_frame();
        death_effect::update_and_render(dl);
    }
}
