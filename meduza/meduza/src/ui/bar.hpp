#pragma once

namespace ui::bar {
    extern bool g_open;
    extern float g_alpha;
    extern float g_game_alpha;

    void set_game_alpha(float a);
    float game_alpha();
    void render();
}
