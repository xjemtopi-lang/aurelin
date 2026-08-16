#pragma once

// cfg_system.hpp — сохранение/загрузка конфигов.
// Слоты хранятся в /data/local/tmp/magaisanware_slotN.cfg (text, key=value).

namespace cfg_system {
    bool save_id(int slot);
    bool load_id(int slot);
    bool slot_exists(int slot);
    void reset_defaults();
    const char* last_status();
}