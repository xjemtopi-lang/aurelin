#pragma once
// esp_preview.hpp — ESP Preview окно
// - Нет никаких надписей (name, hp text, distance убраны)
// - Head circle рисуется ДО бокса (z-order: под боксом)
// - Head circle центр совпадает с верхней точкой шеи скелета

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include <cmath>

namespace ui { namespace esp_preview {

static inline ImU32 _ecol32(const ImVec4& c, bool rgb, float bright, int force_a = -1) {
    ImVec4 v;
    if (rgb) {
        float t = (float)ImGui::GetTime() * cfg::rgb::speed + bright;
        v = ImVec4(
            sinf(t)          * 0.5f + 0.5f,
            sinf(t + 2.094f) * 0.5f + 0.5f,
            sinf(t + 4.189f) * 0.5f + 0.5f,
            1.0f);
    } else {
        v = c;
        v.x = ImClamp(v.x + bright * 0.1f, 0.f, 1.f);
        v.y = ImClamp(v.y + bright * 0.1f, 0.f, 1.f);
        v.z = ImClamp(v.z + bright * 0.1f, 0.f, 1.f);
    }
    int a = (force_a >= 0) ? force_a : 255;
    return IM_COL32((int)(v.x*255),(int)(v.y*255),(int)(v.z*255), a);
}

// Акцентная фиолетовая (наш ui::clr::accent) с альфой
static inline ImU32 _accent32(float ma) {
    return IM_COL32(162, 144, 225, (int)(255 * ma));
}

static inline void _draw_gradient_rounded(ImDrawList* dl,
                                           ImVec2 mn, ImVec2 mx,
                                           ImU32 ct, ImU32 cb, float r) {
    dl->AddRectFilled(mn, mx, ct, r);
    ImVec2 mid(mn.x, mn.y + (mx.y - mn.y) * 0.5f);
    dl->AddRectFilledMultiColor(mid, mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), cb, cb);
}

static void render(float ma) {
    if (!cfg::esp::preview_visible) return;

    ImGuiIO& io = ImGui::GetIO();

    const float PW = 280.f;
    const float PH = 380.f;
    const float PR = 10.f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   PR);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.f, 0.f, 0.f, 0.f));

    ImGui::SetNextWindowSize(ImVec2(PW, PH), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - PW - 24.f, 24.f), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::Begin("##ESPPreview_exp", nullptr,
        ImGuiWindowFlags_NoCollapse        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2      pp  = ImGui::GetWindowPos();
    ImVec2      psz = ImGui::GetWindowSize();
    ImDrawList* dl  = ImGui::GetWindowDrawList();

    ImU32 accent32 = _accent32(ma);

    // Фон окна
    dl->AddRectFilled(pp, ImVec2(pp.x+psz.x, pp.y+psz.y), IM_COL32(8,8,14,220), PR);
    dl->AddRect(ImVec2(pp.x+0.5f, pp.y+0.5f),
                ImVec2(pp.x+psz.x-0.5f, pp.y+psz.y-0.5f),
                (accent32 & 0x00FFFFFF) | 0xC0000000, PR, 0, 1.5f);
    dl->AddRect(ImVec2(pp.x+2.5f, pp.y+2.5f),
                ImVec2(pp.x+psz.x-2.5f, pp.y+psz.y-2.5f),
                (accent32 & 0x00FFFFFF) | 0x18000000, PR-2.f, 0, 1.f);

    // ── Геометрия фиктивного игрока ──────────────────────────────────────────
    const float BOX_W = 90.f;
    const float BOX_H = 210.f;
    float cx   = pp.x + psz.x * 0.5f;
    float bcx  = cx + 8.f;
    float left  = bcx - BOX_W * 0.5f;
    float right = bcx + BOX_W * 0.5f;
    float top   = pp.y + psz.y * 0.5f - BOX_H * 0.5f;
    float bot   = pp.y + psz.y * 0.5f + BOX_H * 0.5f;

    ImVec2 fmin(left, top);
    ImVec2 fmax(right, bot);
    float  bw = fmax.x - fmin.x;
    float  bh = fmax.y - fmin.y;

    ImVec2 neck (bcx, fmin.y + bh * 0.10f);
    ImVec2 waist(bcx, fmin.y + bh * 0.50f);
    ImVec2 lsh  (fmin.x + bw*0.08f, fmin.y + bh*0.18f);
    ImVec2 lhd  (fmin.x + bw*0.02f, fmin.y + bh*0.45f);
    ImVec2 rsh  (fmax.x - bw*0.08f, fmin.y + bh*0.18f);
    ImVec2 rhd  (fmax.x - bw*0.02f, fmin.y + bh*0.45f);
    ImVec2 lkn  (fmin.x + bw*0.22f, fmin.y + bh*0.72f);
    ImVec2 lft  (fmin.x + bw*0.18f, fmax.y - 2.f);
    ImVec2 rkn  (fmax.x - bw*0.22f, fmin.y + bh*0.72f);
    ImVec2 rft  (fmax.x - bw*0.18f, fmax.y - 2.f);

    float hr = (neck.y - fmin.y) * 0.85f;
    ImVec2 hc(bcx, neck.y - hr * 0.1f);

    // ── 1. Head circle ───────────────────────────────────────────────────────
    if (cfg::esp::head_circle) {
        ImU32 hcc = _ecol32(cfg::esp::head_circle_col, cfg::esp::head_circle_rgb, 0.f);
        dl->AddCircle(hc, hr, IM_COL32(0,0,0,160), 32, 2.8f);
        dl->AddCircle(hc, hr, hcc,                  32, 1.6f);
    }

    // ── 2. Box Fill ──────────────────────────────────────────────────────────
    if (cfg::esp::box_fill) {
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
            _draw_gradient_rounded(dl, fmin, fmax, cu, cl, rnd);
        } else {
            ImU32 fc = _ecol32(cfg::esp::fill_col, cfg::esp::fill_rgb, 0.f, a);
            dl->AddRectFilled(fmin, fmax, fc, rnd);
        }
    }

    // ── 3. Box ESP ───────────────────────────────────────────────────────────
    if (cfg::esp::box) {
        float rnd = cfg::esp::box_rounding;
        int   bt  = cfg::esp::box_type;
        ImU32 bc  = _ecol32(cfg::esp::box_col, cfg::esp::box_rgb, 0.f);

        if (bt == 0) {
            dl->AddRect(fmin, fmax, IM_COL32(0,0,0,160), rnd, 0, 2.8f);
            dl->AddRect(fmin, fmax, bc,                   rnd, 0, 1.6f);
        } else {
            ImU32 blk = IM_COL32(0,0,0,160);
            float cl  = fminf(bw, bh) * 0.22f;
            dl->AddLine(fmin,                   ImVec2(fmin.x+cl,fmin.y),  blk,2.8f);
            dl->AddLine(fmin,                   ImVec2(fmin.x,fmin.y+cl),  blk,2.8f);
            dl->AddLine(fmin,                   ImVec2(fmin.x+cl,fmin.y),  bc, 1.6f);
            dl->AddLine(fmin,                   ImVec2(fmin.x,fmin.y+cl),  bc, 1.6f);
            dl->AddLine(ImVec2(fmax.x,fmin.y),  ImVec2(fmax.x-cl,fmin.y), blk,2.8f);
            dl->AddLine(ImVec2(fmax.x,fmin.y),  ImVec2(fmax.x,fmin.y+cl), blk,2.8f);
            dl->AddLine(ImVec2(fmax.x,fmin.y),  ImVec2(fmax.x-cl,fmin.y), bc, 1.6f);
            dl->AddLine(ImVec2(fmax.x,fmin.y),  ImVec2(fmax.x,fmin.y+cl), bc, 1.6f);
            dl->AddLine(ImVec2(fmin.x,fmax.y),  ImVec2(fmin.x+cl,fmax.y), blk,2.8f);
            dl->AddLine(ImVec2(fmin.x,fmax.y),  ImVec2(fmin.x,fmax.y-cl), blk,2.8f);
            dl->AddLine(ImVec2(fmin.x,fmax.y),  ImVec2(fmin.x+cl,fmax.y), bc, 1.6f);
            dl->AddLine(ImVec2(fmin.x,fmax.y),  ImVec2(fmin.x,fmax.y-cl), bc, 1.6f);
            dl->AddLine(fmax,                   ImVec2(fmax.x-cl,fmax.y), blk,2.8f);
            dl->AddLine(fmax,                   ImVec2(fmax.x,fmax.y-cl), blk,2.8f);
            dl->AddLine(fmax,                   ImVec2(fmax.x-cl,fmax.y), bc, 1.6f);
            dl->AddLine(fmax,                   ImVec2(fmax.x,fmax.y-cl), bc, 1.6f);
        }
    }

    // ── 4. Skeleton ──────────────────────────────────────────────────────────
    if (cfg::esp::skeleton) {
        ImU32 sc = _ecol32(cfg::esp::skeleton_col, cfg::esp::skeleton_rgb, 0.f);
        ImU32 sb = IM_COL32(0,0,0,140);
        dl->AddLine(neck,  waist, sb, 2.8f); dl->AddLine(neck,  waist, sc, 1.4f);
        dl->AddLine(neck,  lsh,   sb, 2.8f); dl->AddLine(neck,  lsh,   sc, 1.4f);
        dl->AddLine(lsh,   lhd,   sb, 2.8f); dl->AddLine(lsh,   lhd,   sc, 1.4f);
        dl->AddLine(neck,  rsh,   sb, 2.8f); dl->AddLine(neck,  rsh,   sc, 1.4f);
        dl->AddLine(rsh,   rhd,   sb, 2.8f); dl->AddLine(rsh,   rhd,   sc, 1.4f);
        dl->AddLine(waist, lkn,   sb, 2.8f); dl->AddLine(waist, lkn,   sc, 1.4f);
        dl->AddLine(lkn,   lft,   sb, 2.8f); dl->AddLine(lkn,   lft,   sc, 1.4f);
        dl->AddLine(waist, rkn,   sb, 2.8f); dl->AddLine(waist, rkn,   sc, 1.4f);
        dl->AddLine(rkn,   rft,   sb, 2.8f); dl->AddLine(rkn,   rft,   sc, 1.4f);
    }

    // ── 5. Health Bar ─────────────────────────────────────────────────────────
    if (cfg::esp::health) {
        const float HBW = 4.f;
        float hbx = fmin.x - HBW - 4.f;
        float hby = fmin.y;
        dl->AddRectFilled(ImVec2(hbx,hby), ImVec2(hbx+HBW,hby+bh), IM_COL32(0,0,0,200));
        const float PCT = 0.72f;
        float hpH = bh * PCT;
        float hpY = hby + (bh - hpH);
        ImU32 hc  = _ecol32(cfg::esp::health_col, cfg::esp::health_rgb, 0.f);
        dl->AddRectFilled(ImVec2(hbx,hpY), ImVec2(hbx+HBW,hby+bh), hc);
        dl->AddRect      (ImVec2(hbx,hby), ImVec2(hbx+HBW,hby+bh), IM_COL32(0,0,0,255), 0, 0, 1.f);
    }

    // ── 6. Nickname ──────────────────────────────────────────────────────────
    if (cfg::esp::name) {
        const char* ntxt = "Enemy";
        ImVec2 tsz = ImGui::CalcTextSize(ntxt);
        float  tx  = bcx - tsz.x * 0.5f;
        float  ty  = fmin.y - tsz.y - 4.f;
        ImU32  nc  = _ecol32(cfg::esp::name_col, cfg::esp::name_rgb, 0.6f);
        dl->AddText(ImVec2(tx+1,ty+1), IM_COL32(0,0,0,255), ntxt);
        dl->AddText(ImVec2(tx,  ty),   nc,                   ntxt);
    }

    // ── 7. Health Text ───────────────────────────────────────────────────────
    if (cfg::esp::health_text) {
        const char* htxt = "72 HP";
        ImVec2 tsz = ImGui::CalcTextSize(htxt);
        float  tx  = bcx - tsz.x * 0.5f;
        float  ty  = fmax.y + 4.f;
        ImU32  hc  = _ecol32(cfg::esp::hptext_col, cfg::esp::hptext_rgb, 0.f);
        dl->AddText(ImVec2(tx+1,ty+1), IM_COL32(0,0,0,255), htxt);
        dl->AddText(ImVec2(tx,  ty),   hc,                   htxt);
    }

    // ── 8. Distance ──────────────────────────────────────────────────────────
    if (cfg::esp::distance) {
        const char* dtxt = "38m";
        ImVec2 tsz = ImGui::CalcTextSize(dtxt);
        float  tx  = fmax.x + 4.f;
        float  ty  = (fmin.y + fmax.y) * 0.5f - tsz.y * 0.5f;
        ImU32  dc  = _ecol32(cfg::esp::distance_col, cfg::esp::distance_rgb, 0.f);
        dl->AddText(ImVec2(tx+1,ty+1), IM_COL32(0,0,0,255), dtxt);
        dl->AddText(ImVec2(tx,  ty),   dc,                   dtxt);
    }

    // ── 9. Snap Line ─────────────────────────────────────────────────────────
    if (cfg::esp::line) {
        ImDrawList* fg2 = ImGui::GetForegroundDrawList();
        ImU32 lc = _ecol32(cfg::esp::line_col, cfg::esp::line_rgb, 1.5f);
        fg2->AddLine(ImVec2(pp.x+psz.x*0.5f, pp.y+psz.y-1.f),
                     ImVec2(bcx, fmax.y),
                     IM_COL32(0,0,0,120), 2.2f);
        fg2->AddLine(ImVec2(pp.x+psz.x*0.5f, pp.y+psz.y-1.f),
                     ImVec2(bcx, fmax.y),
                     lc, 1.2f);
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

}} // namespace ui::esp_preview
