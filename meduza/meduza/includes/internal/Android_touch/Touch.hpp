#pragma once

namespace touch {
    // Главный поток ставит true, когда меню открыто — свайпы станут скроллом
    inline volatile bool scroll_enabled = false;

    bool init(int32_t _screen_w, int32_t _screen_h, uint8_t _orientation);
    void update(int32_t _screen_w, int32_t _screen_h, uint8_t _orientation);
    void updateOrientation(uint8_t _orientation);
    void shutdown();
}

