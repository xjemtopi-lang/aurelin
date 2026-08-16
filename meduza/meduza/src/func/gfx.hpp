#pragma once
#include <stdint.h>

// cfg::gfx объявлен в src/ui/cfg.hpp
// Этот хедер — только публичный интерфейс модуля

namespace gfx {
    void low_gfx_on();
    void low_gfx_off();
    void texture_on();
    void texture_off();
} // namespace gfx
