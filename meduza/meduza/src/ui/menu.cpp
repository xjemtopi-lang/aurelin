#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "bar.hpp"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "../func/combat.hpp"
#include "../func/gfx.hpp"
#include "cfg_holy.hpp"
#include "cfg_holy_bridge.hpp"
#include "../func/props.hpp"
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cmath>

extern ImFont* fontBold;
extern ImFont* fontMedium;
extern ImFont* fontDesc;
extern ImFont* espFont;
extern ImGuiWindow* g_window;

namespace ui::menu {
    static bool g_style_init = false;
    static int g_tab = 1;
    static float g_alpha = 0.f;
    static ImFont* g_font_body = nullptr;
    static ImFont* g_font_tabs = nullptr;
    static ImFont* g_font_title = nullptr;
    static bool g_should_exit = false;
    static bool g_menu_open = false;

    // Константы размеров
    static const float TITLE_HEIGHT  = 52.f;
    static const float TABS_HEIGHT   = 70.f;
    static const float CONTENT_OFFSET = TITLE_HEIGHT + TABS_HEIGHT + 30.f;
    static const float WIN_WIDTH     = 1400.f;
    static const float WIN_HEIGHT    = 850.f;

    // Watermark
    static const float WM_PAD    = 12.f;
    static const float WM_HEIGHT = 38.f;   // нормальный размер

    static float lerp_float(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static void init_style() {
        if (g_style_init) return;

        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();

        style.WindowBorderSize    = 1.f;
        style.ChildBorderSize     = 1.f;
        style.FrameBorderSize     = 1.f;
        style.WindowRounding      = 0.f;
        style.ChildRounding       = 0.f;
        style.FrameRounding       = 0.f;
        style.PopupRounding       = 0.f;
        style.ScrollbarRounding   = 0.f;
        style.GrabRounding        = 0.f;

        style.Colors[ImGuiCol_Text]                = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled]        = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        style.Colors[ImGuiCol_WindowBg]            = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_ChildBg]             = ImVec4(0.098f, 0.086f, 0.208f, 1.00f);
        style.Colors[ImGuiCol_PopupBg]             = ImVec4(0.066f, 0.059f, 0.141f, 0.98f);
        style.Colors[ImGuiCol_Border]              = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_BorderShadow]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_FrameBg]             = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]      = ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive]       = ImVec4(0.106f, 0.078f, 0.220f, 1.00f);
        style.Colors[ImGuiCol_TitleBg]             = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive]       = ImVec4(0.761f, 0.090f, 0.314f, 1.00f);
        style.Colors[ImGuiCol_MenuBarBg]           = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg]         = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]       = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.106f, 0.078f, 0.220f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]           = ImVec4(0.820f, 0.290f, 0.490f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]          = ImVec4(0.820f, 0.290f, 0.490f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive]    = ImVec4(0.761f, 0.090f, 0.314f, 1.00f);
        style.Colors[ImGuiCol_Button]              = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered]       = ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive]        = ImVec4(0.106f, 0.078f, 0.220f, 1.00f);
        style.Colors[ImGuiCol_Header]              = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered]       = ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive]        = ImVec4(0.106f, 0.078f, 0.220f, 1.00f);
        style.Colors[ImGuiCol_Separator]           = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip]          = ImVec4(0.118f, 0.098f, 0.243f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripHovered]   = ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripActive]    = ImVec4(0.761f, 0.090f, 0.314f, 1.00f);
        style.Colors[ImGuiCol_TextSelectedBg]      = ImVec4(0.761f, 0.090f, 0.314f, 0.35f);
        style.Colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.05f, 0.05f, 0.05f, 0.65f);
        style.Colors[ImGuiCol_Tab]                 = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_TabHovered]          = ImVec4(0.157f, 0.125f, 0.294f, 1.00f);
        style.Colors[ImGuiCol_TabSelected]         = ImVec4(0.761f, 0.090f, 0.314f, 1.00f);
        style.Colors[ImGuiCol_TabDimmed]           = ImVec4(0.066f, 0.059f, 0.141f, 1.00f);
        style.Colors[ImGuiCol_TabDimmedSelected]   = ImVec4(0.761f, 0.090f, 0.314f, 1.00f);

        // Аккуратные паддинги — чуть плотнее чем было
        style.WindowPadding = ImVec2(16.f, 16.f);
        style.FramePadding  = ImVec2(8.f, 4.f);
        style.ItemSpacing   = ImVec2(12.f, 10.f);  // было 15/15 — слишком разреженно
        style.ScrollbarSize = 20.f;

        // Шрифтовая иерархия:
        // fontBold   → заголовок меню + ватермарк (жирный)
        // fontMedium → табы
        // fontDesc   → тело (основной контент)
        g_font_title = fontBold   ? fontBold   : ImGui::GetFont();
        g_font_tabs  = fontMedium ? fontMedium : ImGui::GetFont();
        g_font_body  = fontDesc   ? fontDesc   : ImGui::GetFont();

        g_style_init = true;
    }

    static bool begin_window() {
        ImVec2 win_size(WIN_WIDTH, WIN_HEIGHT);
        ImVec2 min_size(850.f, 500.f);
        ImGui::SetNextWindowSizeConstraints(min_size, win_size);
        ImGui::SetNextWindowSize(win_size, ImGuiCond_Once);
        ImGui::SetNextWindowPos(
            ImVec2((g_sw - win_size.x) * 0.5f, (g_sh - win_size.y) * 0.5f),
            ImGuiCond_Once);

        if (!ImGui::Begin("picadff", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse))
            return false;

        g_window = ImGui::GetCurrentWindow();

        // Многослойная рамка — идентично оригиналу
        for (int i = 0; i < 8; ++i) {
            ImColor bc(0, 0, 0, 255);
            if      (i == 1 || i == 7) bc = ImColor(55, 55, 55, 255);
            else if (i == 0)            bc = ImColor(0,  0,  0,  255);
            else                        bc = ImColor(35, 35, 35, 255);

            ImGui::GetWindowDrawList()->AddRect(
                ImVec2(ImGui::GetWindowPos().x + (float)i, ImGui::GetWindowPos().y + (float)i),
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - (float)i,
                       ImGui::GetWindowPos().y + ImGui::GetWindowSize().y - (float)i),
                bc);
        }
        return true;
    }

    static void end_window() { ImGui::End(); }

    static bool begin_section(const char* label, const ImVec2& size) {
        if (!ImGui::BeginChild(label, size, ImGuiChildFlags_Border,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            return false;

        const ImVec2 pos      = ImGui::GetWindowPos();
        const ImVec2 sz       = ImGui::GetWindowSize();
        const ImVec2 lsz      = ImGui::CalcTextSize(label);
        const ImVec2 lpos(pos.x + 15.f, pos.y - (lsz.y * 0.5f));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(pos, ImVec2(pos.x + sz.x, pos.y + sz.y), ImColor(0,0,0,255));
        dl->AddRect(
            ImVec2(pos.x + 1.f, pos.y + 1.f),
            ImVec2(pos.x + sz.x - 1.f, pos.y + sz.y - 1.f),
            ImColor(40, 40, 40, 255));
        dl->AddRect(
            ImVec2(pos.x + 2.f, pos.y + 2.f),
            ImVec2(pos.x + sz.x - 2.f, pos.y + sz.y - 2.f),
            ImColor(25, 25, 25, 255));

        // Подложка под лейбл секции + сам лейбл
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(lpos.x - 4.f, lpos.y),
            ImVec2(lpos.x + lsz.x + 4.f, lpos.y + lsz.y),
            ImColor(15, 15, 15, 255));
        ImGui::GetForegroundDrawList()->AddText(lpos, ImColor(255, 255, 255, 255), label);

        return true;
    }

    static void end_section() { ImGui::EndChild(); }

    static void section_placeholder(const char* text) {
        ImGui::PushFont(g_font_body);
        ImGui::Spacing();
        ImGui::TextDisabled("%s", text);
        ImGui::PopFont();
    }

    static void begin_body_font() {
        if (g_font_body) ImGui::PushFont(g_font_body);
    }
    static void end_body_font() {
        if (g_font_body) ImGui::PopFont();
    }

    static void draw_separator() { ImGui::Separator(); }

    static bool combo_from_vector(const char* label, int* current_item,
                                  const std::vector<const char*>& items) {
        const char* preview = "";
        if (*current_item >= 0 && *current_item < (int)items.size())
            preview = items[*current_item];

        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            for (int i = 0; i < (int)items.size(); ++i) {
                const bool sel = (*current_item == i);
                if (ImGui::Selectable(items[i], sel)) {
                    *current_item = i;
                    changed = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    static void color_control(const char* label, ImVec4* value) {
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(-1.f);
        std::string id = std::string("##") + label;
        ImGui::ColorEdit4(id.c_str(), &value->x,
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf);
    }

    static ImVec2 content_half_size() {
        return ImVec2(
            (ImGui::GetWindowSize().x -
             (ImGui::GetStyle().WindowPadding.x * 2.f + ImGui::GetStyle().ItemSpacing.x)) * 0.5f,
            ImGui::GetWindowSize().y - CONTENT_OFFSET -
             (ImGui::GetStyle().WindowPadding.y * 2.f + ImGui::GetStyle().ItemSpacing.y * 2.f));
    }

    // =============================== TABS ================================

    // rage_tab:
    //   left  — combat panel (wallshot / inf ammo / damage) [unchanged]
    //   right — aimbot + triggerbot panel (new)
    static void rage_tab() {
        const ImVec2 child_size = content_half_size();

        if (begin_section("combat", child_size)) {
            begin_body_font();

            // ── Wallshot ──
            ImGui::Checkbox("Wallshot", &cfg_holy::wallshot::enabled);
            if (cfg_holy::wallshot::enabled) {
                ImGui::SliderInt("Penetration", &cfg_holy::wallshot::value, 150, 2147483646);
            }
            draw_separator();

            // ── Infinite Ammo ──
            ImGui::Checkbox("Inf Ammo", &cfg_holy::inf_ammo::enabled);
            if (cfg_holy::inf_ammo::enabled) {
                ImGui::SliderInt("Ammo amount", &cfg_holy::inf_ammo::value, 100, 30000);
            }
            draw_separator();

            // ── Damage Hack (Sigma) ──
            ImGui::Checkbox("Damage Hack", &cfg_holy::sigma::enabled);
            if (cfg_holy::sigma::enabled) {
                ImGui::SliderInt("Damage", &cfg_holy::sigma::damage, 1, 1000);
            }

            end_body_font();
        }
        end_section();

        ImGui::SameLine();

        // ── AIMBOT + TRIGGERBOT (полностью биндится к cfg::combat::*) ──
        if (begin_section("aimbot & triggerbot", child_size)) {
            begin_body_font();

            // ---------- AIMBOT ----------
            // cfg::combat::aimbot — главный флаг; читается в combat.cpp:114,301
            ImGui::Checkbox("Enable Aimbot", &cfg::combat::aimbot);
            if (cfg::combat::aimbot) {
                // cfg::combat::aimbot_hitbox — combat.cpp:379,402,439; 0=Head, 1=Bone
                combo_from_vector("Target bone", &cfg::combat::aimbot_hitbox,
                    std::vector<const char*>{"Head", "Chest"});

                // cfg::combat::aimbot_visible — combat.cpp:363 (проверка видимости кости)
                ImGui::Checkbox("Visible only", &cfg::combat::aimbot_visible);

                // cfg::combat::aimbot_allow_fallback — запасной вариант цели
                ImGui::Checkbox("Fallback to body", &cfg::combat::aimbot_allow_fallback);

                // cfg::combat::aimbot_fov — combat.cpp:347,495; fov_radius_px()
                ImGui::SliderFloat("FOV (deg)", &cfg::combat::aimbot_fov, 5.f, 180.f, "%.0f");

                // cfg::combat::aimbot_smooth — combat.cpp:439
                ImGui::SliderFloat("Smooth", &cfg::combat::aimbot_smooth, 1.f, 20.f, "%.1f");

                // cfg::combat::aimbot_max_dist — combat.cpp:374
                ImGui::SliderFloat("Max distance", &cfg::combat::aimbot_max_dist, 10.f, 500.f, "%.0f m");

                draw_separator();

                // cfg::combat::aimbot_fov_draw — combat.cpp:489,495 (круг FOV)
                ImGui::Checkbox("Show FOV circle", &cfg::combat::aimbot_fov_draw);
                // cfg::combat::aimbot_lock_line — combat.cpp:408 (линия к цели)
                ImGui::Checkbox("Show lock line", &cfg::combat::aimbot_lock_line);
                // cfg::combat::aimbot_lock_dot — combat.cpp:414 (точка на цели)
                ImGui::Checkbox("Show lock dot", &cfg::combat::aimbot_lock_dot);
            }

            draw_separator();

            // ---------- TRIGGERBOT ----------
            // cfg::combat::triggerbot — combat.cpp:166
            ImGui::Checkbox("Enable Triggerbot", &cfg::combat::triggerbot);
            if (cfg::combat::triggerbot) {
                // cfg::combat::trigger_range — combat.cpp:211
                ImGui::SliderFloat("Trigger range", &cfg::combat::trigger_range, 5.f, 80.f, "%.0f m");
                // cfg::combat::trigger_delay — combat.cpp:238
                ImGui::SliderFloat("Trigger delay", &cfg::combat::trigger_delay, 0.f, 0.5f, "%.2f s");
                // cfg::combat::trigger_visible_only — combat.cpp:200
                ImGui::Checkbox("Visible only", &cfg::combat::trigger_visible_only);
            }

            draw_separator();

            // ---------- AIM STATUS (read-only, из combat namespace) ─────────
            ImGui::TextDisabled("Aimbot state");
            {
                const char* status = combat::aimbot_status_label();
                const char* target = combat::aimbot_selected_label();
                const char* bone   = combat::aimbot_resolved_label();
                bool        has_t  = combat::aimbot_has_target();
                float       dist   = combat::aimbot_target_distance();

                ImVec4 col = has_t ? ImVec4(0.65f, 0.95f, 0.65f, 1.f)
                                   : ImVec4(0.65f, 0.65f, 0.65f, 1.f);
                ImGui::TextColored(col, "Status: %s", status ? status : "Idle");
                if (has_t) {
                    ImGui::Text("Target: %s", target ? target : "?");
                    ImGui::Text("Bone:   %s", bone   ? bone   : "?");
                    ImGui::Text("Dist:   %.0f m", dist);
                }
            }

            end_body_font();
        }
        end_section();
    }

    static void visuals_tab() {
        const ImVec2 child_size = content_half_size();

        if (begin_section("esp", child_size)) {
            begin_body_font();
            ImGui::Checkbox("Box 2D",          &cfg::esp::box);
            ImGui::Checkbox("Name",            &cfg::esp::name);
            ImGui::Checkbox("Health bar",      &cfg::esp::health);
            ImGui::Checkbox("Distance",        &cfg::esp::distance);
            ImGui::Checkbox("Snapline head",   &cfg::esp::snapline_head);
            ImGui::Checkbox("Hit log",         &cfg::esp::hitlog);
            draw_separator();
            combo_from_vector("Box type", &cfg::esp::box_type,
                std::vector<const char*>{"Full", "Corner"});
            ImGui::SliderFloat("Box rounding", &cfg::esp::box_rounding, 0.f, 10.f, "%.0f");
            end_body_font();
        }
        end_section();

        ImGui::SameLine();

        if (begin_section("colors & effects", child_size)) {
            begin_body_font();
            color_control("Box 2D color",       &cfg::esp::box_col);
            color_control("Name color",         &cfg::esp::name_col);
            color_control("Health color",       &cfg::esp::health_col);
            color_control("Distance color",     &cfg::esp::distance_col);
            color_control("Snapline head col",  &cfg::esp::snapline_head_col);
            draw_separator();
            ImGui::Checkbox("Death Particles",  &cfg::effects::death_particles);
            if (cfg::effects::death_particles) {
                color_control("Particle color", &cfg::effects::particle_col);
            }
            end_body_font();
        }
        end_section();
    }

    static void exploits_tab() {
        const ImVec2 child_size = content_half_size();

        // ── LOW GFX ──────────────────────────────────────────────────────────
        if (begin_section("low gfx", child_size)) {
            begin_body_font();

            ImGui::TextDisabled("Sniffs QWORD pattern, patches gfx quality value.");
            ImGui::TextDisabled("Enable once per session.");
            draw_separator();

            if (ImGui::Checkbox("Low GFX", &cfg::gfx::low_gfx)) {
                if (cfg::gfx::low_gfx)
                    gfx::low_gfx_on();
                else
                    gfx::low_gfx_off();
            }

            end_body_font();
        }
        end_section();

        ImGui::SameLine();

        // ── TEXTURE POTATO ───────────────────────────────────────────────────
        if (begin_section("texture potato", child_size)) {
            begin_body_font();

            ImGui::TextDisabled("Scans DWORD 1073741890, writes 1086324736.");
            ImGui::TextDisabled("Up to 6666 patches. Backup + restore.");
            draw_separator();

            if (ImGui::Checkbox("Texture Potato", &cfg::gfx::texture_potato)) {
                if (cfg::gfx::texture_potato)
                    gfx::texture_on();
                else
                    gfx::texture_off();
            }

            end_body_font();
        }
        end_section();
    }

    // misc_tab:
    //   left  — movement (existing) + radar (new, binds cfg::radar::*)
    //   right — props (existing) + info panel (new, binds cfg::info_panel::enabled)
    static void misc_tab() {
        const ImVec2 child_size = content_half_size();

        // ───── LEFT: MOVEMENT + RADAR ─────
        if (begin_section("movement & radar", child_size)) {
            begin_body_font();

            ImGui::TextDisabled("movement");
            ImGui::Checkbox("Bunny Hop",    &cfg_holy::bunny_hop::enabled);
            ImGui::Checkbox("Strafe",       &cfg_holy::strafe::enabled);
            ImGui::Checkbox("Air Jump",     &cfg_holy::air_jump::enabled);
            ImGui::Checkbox("Crouch Speed", &cfg_holy::crouch_speed::enabled);

            draw_separator();

            // ── FOV Changer ──
            ImGui::Checkbox("FOV Changer", &cfg_holy::fov_changer::enabled);
            if (cfg_holy::fov_changer::enabled) {
                ImGui::SliderFloat("FOV", &cfg_holy::fov_changer::value, 40.f, 90.f, "%.0f");
            }

            draw_separator();

            // ─── RADAR ───
            // visuals.cpp:223 — draw_radar() вызывается только если enabled
            // visuals.cpp:475..587 использует: size, range, bg_col, dot_col, self_col
            ImGui::TextDisabled("radar");
            ImGui::Checkbox("Enable Radar", &cfg::radar::enabled);

            begin_body_font();
            ImGui::BeginDisabled(!cfg::radar::enabled);
            // visuals.cpp:475 — float R = cfg::radar::size * 0.5f
            ImGui::SliderFloat("Radar size",    &cfg::radar::size,   100.f, 300.f, "%.0f px");
            // visuals.cpp:526 — if (dist2d > cfg::radar::range) continue;
            ImGui::SliderFloat("Radar range",   &cfg::radar::range,   20.f, 250.f, "%.0f m");
            draw_separator();

            // cfg::radar::bg_col / dot_col / self_col — visuals.cpp:484..520
            color_control("Radar background", &cfg::radar::bg_col);
            color_control("Radar enemy dot",  &cfg::radar::dot_col);
            color_control("Radar self dot",   &cfg::radar::self_col);
            ImGui::EndDisabled();

            end_body_font();
        }
        end_section();

        ImGui::SameLine();

        // ───── RIGHT: PROPS + INFO PANEL ─────
        if (begin_section("props & info panel", child_size)) {
            begin_body_font();

            ImGui::TextDisabled("props");
            // Score
            ImGui::Checkbox("Set Score",     &cfg_holy::props::set_score);
            if (cfg_holy::props::set_score)
                ImGui::SliderInt("Score",    &cfg_holy::props::score_val, 0, 9999);

            ImGui::Checkbox("Set Kills",     &cfg_holy::props::set_kills);
            if (cfg_holy::props::set_kills)
                ImGui::SliderInt("Kills",    &cfg_holy::props::kills_val, 0, 9999);

            ImGui::Checkbox("Set Deaths",    &cfg_holy::props::set_death);
            if (cfg_holy::props::set_death)
                ImGui::SliderInt("Deaths",   &cfg_holy::props::death_val, 0, 9999);

            ImGui::Checkbox("Set Assists",   &cfg_holy::props::set_assists);
            if (cfg_holy::props::set_assists)
                ImGui::SliderInt("Assists",  &cfg_holy::props::assists_val, 0, 9999);

            draw_separator();
            ImGui::Checkbox("Set Ping",      &cfg_holy::props::set_ping);
            if (cfg_holy::props::set_ping)
                ImGui::SliderInt("Ping",     &cfg_holy::props::ping_val, 0, 999);

            ImGui::Checkbox("Set MVP",       &cfg_holy::props::set_mvp);
            ImGui::Checkbox("Hide ID",       &cfg_holy::props::hide_id);
            ImGui::Checkbox("Hide Clan",     &cfg_holy::props::hide_clan);
            ImGui::Checkbox("Fake Medal",    &cfg_holy::props::fake_medal);

            draw_separator();

            // ─── INFO PANEL ───
            // visuals.cpp:226..227 — draw_enemy_info_panel(1.f) → enabled
            // Содержимое берётся из visuals::g_enemies / g_enemy_count (visuals.cpp:22)
            ImGui::TextDisabled("enemy info panel");
            ImGui::Checkbox("Enable Info Panel", &cfg::info_panel::enabled);

            ImGui::BeginDisabled(!cfg::info_panel::enabled);
            ImGui::TextWrapped("Shows up to 8 nearest enemies sorted by distance with nick, HP bar and range. Data is fed by visuals::draw(). Position: top-right, below watermark.");
            ImGui::EndDisabled();

            end_body_font();
        }
        end_section();
    }

    static void reset_visuals_cfg() {
        cfg::esp::box          = false;
        cfg::esp::name         = false;
        cfg::esp::health       = false;
        cfg::esp::distance     = false;
        cfg::esp::box_type     = 0;
        cfg::esp::box_rounding = 0.f;
        cfg::esp::box_col      = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
        cfg::esp::name_col     = ImVec4(1.f, 1.f, 1.f, 1.f);
        cfg::esp::health_col   = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
        cfg::esp::distance_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);

        cfg::radar::enabled    = false;
        cfg::radar::size       = 180.f;
        cfg::radar::range      = 100.f;
        cfg::info_panel::enabled = false;

        cfg::combat::aimbot           = false;
        cfg::combat::aimbot_visible   = false;
        cfg::combat::aimbot_fov       = 70.f;
        cfg::combat::aimbot_smooth    = 5.f;
        cfg::combat::aimbot_fov_draw  = true;
        cfg::combat::aimbot_lock_line = true;
        cfg::combat::aimbot_lock_dot  = true;
        cfg::combat::aimbot_max_dist  = 250.f;
        cfg::combat::aimbot_hitbox    = 0;
        cfg::combat::aimbot_allow_fallback = true;

        cfg::combat::triggerbot           = false;
        cfg::combat::trigger_delay        = 0.05f;
        cfg::combat::trigger_range        = 30.f;
        cfg::combat::trigger_visible_only = true;
    }

    static void config_tab() {
        const ImVec2 child_size = content_half_size();

        if (begin_section("configs", child_size)) {
            begin_body_font();
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "Screen: %.0f x %.0f", g_sw, g_sh);
            ImGui::TextUnformatted(buffer);
            std::snprintf(buffer, sizeof(buffer), "FPS: %.0f", ImGui::GetIO().Framerate);
            ImGui::TextUnformatted(buffer);
            draw_separator();
            ImGui::TextDisabled("Конфиг-система в этом сорсе отсутствует,");
            ImGui::TextDisabled("поэтому оставил инфо-блок и reset под visuals.");
            end_body_font();
        }
        end_section();

        ImGui::SameLine();

        if (begin_section("action", child_size)) {
            begin_body_font();
            if (ImGui::Button("Reset visuals", ImVec2(-1.f, 0.f)))
                reset_visuals_cfg();
            if (ImGui::Button("Exit", ImVec2(-1.f, 0.f)))
                g_should_exit = true;
            end_body_font();
        }
        end_section();
    }

    static void render_current_tab() {
        switch (g_tab) {
            case 0: rage_tab();     break;
            case 1: visuals_tab();  break;
            case 2: exploits_tab(); break;
            case 3: misc_tab();     break;
            case 4: config_tab();   break;
            default: visuals_tab(); break;
        }
    }

    static void render_bottom_tabs() {
        const char* names[5] = {"rage", "visuals", "exploits", "misc", "config"};

        if (!begin_section("  ",
            ImVec2(ImGui::GetWindowSize().x - (ImGui::GetStyle().WindowPadding.x * 2.f),
                   TABS_HEIGHT)))
            return;

        // fontBold для табов — чуть жирнее чем medium
        if (g_font_tabs) ImGui::PushFont(g_font_tabs);

        for (int i = 0; i < 5; ++i) {
            char tab_name[32];
            std::snprintf(tab_name, sizeof(tab_name), "%s##%d", names[i], i);

            if (g_tab == i)
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 160, 160, 255));

            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

            if (ImGui::Selectable(
                    tab_name, g_tab == i, 0,
                    ImVec2((ImGui::GetWindowSize().x / 5.f) -
                           (ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().WindowPadding.x),
                           0.f))) {
                g_tab = i;
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            if (i != 4) ImGui::SameLine();
        }

        if (g_font_tabs) ImGui::PopFont();

        end_section();
    }

    // ============================================================
    // WATERMARK — ЛЕВАЯ СТОРОНА, ЧЁРНЫЙ ФОН, ЖИРНЫЙ ШРИФТ
    // ============================================================

    void render_watermark(float alpha) {
        if (alpha < 0.001f) return;

        // Средний шрифт: fontMedium если есть, иначе espFont
        ImFont* wm_font = fontMedium ? fontMedium : (espFont ? espFont : nullptr);
        if (!wm_font) return;

        const float FONT_SIZE = 15.f;   // средний размер — не слишком крупно, не мелко
        const float PAD_X     = 30.f;
        const float PAD_Y     = 10.f;
        const float BG_H      = 50.f;   // высота плашки

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        char wm_text[64];
        std::snprintf(wm_text, sizeof(wm_text), "tenmi.cc  |  %.0f fps", ImGui::GetIO().Framerate);

        ImVec2 text_size = wm_font->CalcTextSizeA(FONT_SIZE, FLT_MAX, 0.f, wm_text);

        float bg_w = text_size.x + PAD_X * 2.f + 6.f;  // +6 под акцент-полосу

        // Верхний левый угол экрана с небольшим отступом
        const float OFF = 10.f;
        ImVec2 bg_min(OFF, OFF);
        ImVec2 bg_max(OFF + bg_w, OFF + BG_H);

        // Фон
        dl->AddRectFilled(bg_min, bg_max, IM_COL32(8, 8, 8, (int)(230 * alpha)), 4.f);

        // Левая акцентная полоска 3px
        dl->AddRectFilled(
            ImVec2(bg_min.x + 1.f, bg_min.y + 4.f),
            ImVec2(bg_min.x + 4.f, bg_max.y - 4.f),
            IM_COL32(162, 144, 225, (int)(255 * alpha)), 2.f);

        // Внешняя рамка
        dl->AddRect(bg_min, bg_max,
            IM_COL32(35, 35, 35, (int)(220 * alpha)), 4.f, 0, 1.f);

        // Позиция текста: вертикально по центру, горизонтально после полоски
        ImVec2 text_pos(
            bg_min.x + PAD_X + 4.f,
            bg_min.y + (BG_H - text_size.y) * 0.5f);

        // Тень
        dl->AddText(wm_font, FONT_SIZE,
            ImVec2(text_pos.x + 1.f, text_pos.y + 1.f),
            IM_COL32(0, 0, 0, (int)(120 * alpha)), wm_text);

        // Текст: имя белое, FPS немного приглушён
        // Рисуем одной строкой — красить части отдельно не нужно
        dl->AddText(wm_font, FONT_SIZE, text_pos,
            IM_COL32(225, 225, 225, (int)(255 * alpha)), wm_text);

        // --- Кликабельная зона ---
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.f);
        ImGui::SetNextWindowPos(bg_min);
        ImGui::SetNextWindowSize(ImVec2(bg_w, BG_H));
        ImGui::Begin("##wm_click", nullptr,
            ImGuiWindowFlags_NoTitleBar     |
            ImGuiWindowFlags_NoDecoration   |
            ImGuiWindowFlags_NoBackground   |
            ImGuiWindowFlags_NoMove         |
            ImGuiWindowFlags_NoResize       |
            ImGuiWindowFlags_NoScrollbar);

        if (ImGui::InvisibleButton("##wm_btn", ImVec2(bg_w, BG_H))) {
            g_menu_open     = !g_menu_open;
            ui::bar::g_open = g_menu_open;
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool should_exit() { return g_should_exit; }

    void set_menu_open(bool open) {
        g_menu_open     = open;
        ui::bar::g_open = open;
    }

    bool is_menu_open() { return g_menu_open; }

    // ============================================================
    // MAIN RENDER
    // ============================================================

    void render() {
        if (g_should_exit) return;

        bar::render();
        init_style();

        const float dt = ImClamp(ImGui::GetIO().DeltaTime * 10.f, 0.f, 1.f);
        g_alpha = lerp_float(g_alpha, bar::g_open ? 1.f : 0.f, dt);

        if (g_alpha <= 0.01f) return;

        // Затемнение фона при открытом меню
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(g_sw, g_sh),
            IM_COL32(0, 0, 0, (int)(110.f * g_alpha)));

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_alpha);

        if (begin_window()) {
            // --- Заголовок --- fontBold + чуть крупнее + жирный вид
            if (begin_section(" ",
                ImVec2(ImGui::GetWindowSize().x -
                       (ImGui::GetStyle().WindowPadding.x * 2.f), TITLE_HEIGHT))) {

                const char* label = "tenmi.cc | written by swesws";

                // Используем fontBold для заголовка
                if (g_font_title) ImGui::PushFont(g_font_title);

                const ImVec2 ts = ImGui::CalcTextSize(label);
                const ImVec2 lpos(
                    (ImGui::GetWindowPos().x + ImGui::GetWindowSize().x * 0.5f) - (ts.x * 0.5f),
                    (ImGui::GetWindowPos().y + ImGui::GetWindowSize().y * 0.5f) - (ts.y * 0.5f));

                // Тень заголовка — глубина
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(lpos.x + 1.f, lpos.y + 1.f),
                    ImColor(0, 0, 0, (int)(180.f * g_alpha)), label);

                // Основной заголовок
                ImGui::GetWindowDrawList()->AddText(
                    lpos, ImColor(255, 255, 255, (int)(255.f * g_alpha)), label);

                if (g_font_title) ImGui::PopFont();
            }
            end_section();

            render_current_tab();
            render_bottom_tabs();
            end_window();
        }

        ImGui::PopStyleVar();
    }
}
