#pragma once

#include "imgui.h"
#include "../ui/theme/theme.hpp"  // g_sw, g_sh
#include <string>
#include <vector>
#include <stdint.h>
#include <map>

// espFont объявлен в main.cpp
extern ImFont* espFont;

namespace hitlog {
    // Одна запись лога
    struct Entry {
        std::string name;   // ник врага
        int         damage; // нанесённый урон
        float       born;   // время создания (ImGui::GetTime())
    };

    // Максимум записей на экране
    static const int   MAX_ENTRIES  = 8;
    // Сколько секунд запись живёт
    static const float LIFETIME     = 4.f;
    // Время fade-out (последние N секунд жизни)
    static const float FADE_TIME    = 1.f;

    // Хранилище предыдущего hp по ptr врага
    // Нюанс: ptr может переиспользоваться при реконнекте игрока.
    // Решение: при hp > prev_hp (хил/рекон) — просто обновляем без записи.
    inline std::map<uint64_t, int> prev_hp;

    // Кольцевой буфер записей — vector, чистим по времени в render()
    inline std::vector<Entry> entries;

    // Вызывается каждый кадр из draw() для каждого живого врага
    inline void track(uint64_t player_ptr, const std::string& name, int hp) {
        if (hp <= 0) {
            // Убит — удаляем из трекера чтобы не мусорить
            prev_hp.erase(player_ptr);
            return;
        }

        auto it = prev_hp.find(player_ptr);
        if (it == prev_hp.end()) {
            // Первый раз видим этого игрока — запоминаем hp, лог не пишем
            prev_hp[player_ptr] = hp;
            return;
        }

        int prev = it->second;
        int delta = prev - hp;

        if (delta > 0) {
            // Урон нанесён
            Entry e;
            e.name   = name.empty() ? "???" : name;
            e.damage = delta;
            e.born   = ImGui::GetTime();

            // Ограничиваем размер буфера
            if ((int)entries.size() >= MAX_ENTRIES)
                entries.erase(entries.begin());

            entries.push_back(e);
        }
        // Обновляем hp (и при хиле тоже — иначе после хила посчитается урон неверно)
        it->second = hp;
    }

    // Удаляем запись при смерти/дисконнекте игрока
    inline void remove(uint64_t player_ptr) {
        prev_hp.erase(player_ptr);
    }

    // Рендер — вызывается каждый кадр из visuals::draw() после цикла игроков
    inline void render() {
        if (entries.empty() || !espFont) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        const float font_sz  = 13.f;
        const float line_h   = font_sz + 6.f;  // высота строки
        const float pad_x    = 10.f;
        const float pad_y    = 6.f;
        const float now      = ImGui::GetTime();

        // Чистим протухшие записи
        for (int i = (int)entries.size() - 1; i >= 0; --i) {
            if (now - entries[i].born >= LIFETIME)
                entries.erase(entries.begin() + i);
        }

        if (entries.empty()) return;

        // Рассчитываем размер блока
        int   count  = (int)entries.size();
        float blk_w  = 180.f;
        float blk_h  = pad_y * 2.f + line_h * (float)count;

        // Позиция: нижний левый угол экрана, над баром
        float blk_x = 16.f;
        float blk_y = g_sh - blk_h - 60.f; // 60px от низа — не перекрываем bar

        // Фон блока
        dl->AddRectFilled(
            ImVec2(blk_x, blk_y),
            ImVec2(blk_x + blk_w, blk_y + blk_h),
            IM_COL32(6, 6, 6, 210), 3.f);

        // Левая акцентная полоса (синхронно с ватермарком)
        dl->AddRectFilled(
            ImVec2(blk_x, blk_y + 3.f),
            ImVec2(blk_x + 2.f, blk_y + blk_h - 3.f),
            IM_COL32(162, 144, 225, 220), 1.f);

        // Рамка
        dl->AddRect(
            ImVec2(blk_x, blk_y),
            ImVec2(blk_x + blk_w, blk_y + blk_h),
            IM_COL32(28, 28, 28, 200), 3.f, 0, 1.f);

        // Строки (последние сверху — новые вверх)
        for (int i = 0; i < count; ++i) {
            // Рисуем снизу вверх: последний hit = верхняя строка
            const Entry& e = entries[count - 1 - i];

            float age   = now - e.born;
            float alpha = 1.f;
            if (age > (LIFETIME - FADE_TIME)) {
                alpha = 1.f - (age - (LIFETIME - FADE_TIME)) / FADE_TIME;
                if (alpha < 0.f) alpha = 0.f;
            }

            float row_y = blk_y + pad_y + (float)i * line_h;
            float tx    = blk_x + pad_x + 4.f; // +4 чтобы не залезать на полосу

            // Формируем строку: "NickName  -42"
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%-14.14s -%d", e.name.c_str(), e.damage);

            // Тень
            dl->AddText(espFont, font_sz,
                ImVec2(tx + 1.f, row_y + 1.f),
                IM_COL32(0, 0, 0, (int)(120 * alpha)), buf);

            // Цвет ника — белый
            // Рисуем ник и урон отдельно чтобы урон был акцентным цветом
            char nick_buf[32];
            std::snprintf(nick_buf, sizeof(nick_buf), "%-14.14s", e.name.c_str());

            dl->AddText(espFont, font_sz,
                ImVec2(tx, row_y),
                IM_COL32(220, 220, 220, (int)(255 * alpha)), nick_buf);

            // Урон — красноватый акцент
            char dmg_buf[16];
            std::snprintf(dmg_buf, sizeof(dmg_buf), "-%d", e.damage);

            ImVec2 nick_sz = espFont->CalcTextSizeA(font_sz, FLT_MAX, 0.f, nick_buf);
            dl->AddText(espFont, font_sz,
                ImVec2(tx + nick_sz.x + 2.f, row_y),
                IM_COL32(225, 100, 100, (int)(255 * alpha)), dmg_buf);
        }
    }

} // namespace hitlog
