#pragma once

#include <stdint.h>

namespace cfg_holy {
namespace props {
    inline bool set_score        = false;
    inline int  score_val        = 0;

    inline bool set_score_all    = false;
    inline int  score_all_val    = 0;

    inline bool set_death        = false;
    inline int  death_val        = 0;

    inline bool set_death_all    = false;
    inline int  death_all_val    = 0;

    inline bool set_kills        = false;
    inline int  kills_val        = 0;

    inline bool set_kills_all    = false;
    inline int  kills_all_val    = 0;

    inline bool set_assists      = false;
    inline int  assists_val      = 0;

    inline bool set_money_all    = false;
    inline int  money_all_val    = 0;

    inline bool set_ping         = false;
    inline int  ping_val         = 0;

    inline bool set_ping_all     = false;
    inline int  ping_all_val     = 0;

    inline bool set_mvp          = false;
    inline bool kick_players     = false;
    inline bool hide_id          = false;
    inline bool hide_id_all      = false;
    inline bool hide_clan        = false;
    inline bool hide_clan_all    = false;
    inline bool fake_avatar      = false;
    inline bool fake_avatar_all  = false;
    inline bool fake_medal       = false;
    inline bool fake_medal_all   = false;
}
}

namespace props_ns {
    void tick(uint64_t player_manager, uint64_t local_player);
}
