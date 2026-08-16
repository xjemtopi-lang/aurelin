#pragma once

namespace ui::menu {
    void render();
    void render_watermark(float alpha);
    bool should_exit();
    void set_menu_open(bool open);
    bool is_menu_open();
}
