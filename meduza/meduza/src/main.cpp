#include "Android_draw/draw.h"
#include "ui/theme/theme.hpp"
#include "ui/menu.hpp"
#include "ui/bar.hpp"
#include "other/memory.hpp"
#include "game/game.hpp"
#include "func/visuals.hpp"
#include "func/combat.hpp"
#include "func/wallshot.hpp"
#include "func/inf_ammo.hpp"
#include "func/inf_shop.hpp"
#include "func/fustknife.hpp"
#include "func/norecoil.hpp"
#include "func/fire_rate.hpp"
#include "func/anti_effects.hpp"
#include "func/chams.hpp"
#include "func/test_loader.hpp"
#include "func/movement.hpp"
#include "func/fov_changer.hpp"
#include "func/sigma.hpp"
#include "func/props.hpp"
#include "ui/cfg_holy.hpp"
#include "protect/oxorany.hpp"
#include <cstdio>
#include <thread>
#include <chrono>

static void print_status(const char* status) {
    printf(oxorany("\033[2J\033[H\033[1;38;2;162;144;225m[Aurelin]\033[0m \033[1;37m%s\033[0m\n"), status);
}

static void launch_standoff() {
    system(oxorany("am start -n com.standoff/com.standoff.MainActivity"));
}

int main() {
    screen_config();

    int max_size = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    int min_size = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);

    native_window_screen_x = max_size;
    native_window_screen_y = max_size;

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) return -1;

    touch::init(max_size, min_size, (uint8_t)displayInfo.orientation);

    print_status(oxorany("Game detect ✅"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    print_status(oxorany("start cheat...."));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    launch_standoff();
    
    game::init();

    static float alpha = 0.f;
    static bool prev = false;
    static bool game_started = false;

    while (true) {
        if (ui::menu::should_exit()) {
            break;  // Graceful exit
        }

        drawBegin();

        bool run = game::valid();

#if defined(__x86_64__)
        bool is_landscape = (displayInfo.orientation == 0 || displayInfo.orientation == 2);
#else
        bool is_landscape = (displayInfo.orientation == 1 || displayInfo.orientation == 3);
#endif

        if (run && !prev) {
            if (!game_started) {
                print_status(oxorany("Game detect ✅"));
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                print_status(oxorany("start cheat...."));
                game_started = true;
            } else {
                print_status(oxorany("Game detect ✅"));
            }
            prev = true;
        } else if (!run && prev) {
            print_status(oxorany("game closed"));
            prev = false;
            game_started = false;
        }

        if (is_landscape) {
            ImGuiIO& io = ImGui::GetIO();
            float dt = io.DeltaTime;
            if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

            float target = run ? 1.f : 0.f;
            float spd = run ? 4.f : 6.f;

            if (alpha < target) {
                alpha += dt * spd;
                if (alpha > target) alpha = target;
            } else if (alpha > target) {
                alpha -= dt * spd;
                if (alpha < target) alpha = target;
            }

            ui::bar::set_game_alpha(alpha);

            // Синхронизируем watermark с меню
            ui::menu::render_watermark(alpha);

            if (alpha > 0.001f) {
                ui::menu::render();
            }

            if (run && proc::lib != 0) {
                game::check_lib(get_player_manager());
                visuals::draw();

                // Combat: aimbot + triggerbot
                {
                    uint64_t pm = get_player_manager();
                    if (pm) {
                        uint64_t lp = rpm<uint64_t>(pm + 0x70);
                        if (lp) {
                            combat::tick(lp);
                            arms::run(lp);

                            // ── Перенесённые фичи из anuswin ──
                            wallshot::run();
                            inf_ammo::run();
                            inf_shop::run();
                            fustknife::run();
                            norecoil::run();
                            fire_rate::run();
                            sigma::run();
                            test_loader::run();
                            air_jump::run();
                            strafe::run();
                            bunny_hop::run();
                            crouch_speed::run();
                            anti_effects::run();
                            fov_changer::run();
                            sky_color::run();
                            props_ns::tick(pm, lp);
                        }
                    }
                }

                // FOV circle overlay
                combat::aimbot_draw_fov(
                    ImGui::GetBackgroundDrawList(), g_sw, g_sh);
            }

            // Chams tick — не зависит от proc::lib (работает через PID-скан извне),
            // поэтому обеспечиваем вызов, чтобы stop_worker() отрабатывал при тоггле OFF.
            chams::run();
        }

        bool vis = ui::bar::g_open;
        drawEnd();

        // FPS лимитер
        float target_fps = vis ? 120.f : 30.f;
        float target_us = 1000000.f / target_fps;
        usleep(static_cast<useconds_t>(target_us));
    }

    shutdown();
    return 0;
}
