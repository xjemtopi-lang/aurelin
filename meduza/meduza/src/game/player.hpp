#pragma once

#include "game.hpp"
#include "../other/vector3.h"
#include "../other/string.h"
#include "../protect/oxorany.hpp"
#include <cstring>
#include <string>
#include <cmath>
#include <vector>
#include <unordered_map>

namespace player {
    inline Vector3 position(uint64_t p) noexcept {
        uint64_t MovementController = rpm<uint64_t>(p + oxorany(0x98));
        if (!MovementController) return Vector3(0, 0, 0);

        uint64_t TransformData = rpm<uint64_t>(MovementController + oxorany(0xB0));
        if (!TransformData) return Vector3(0, 0, 0);

        return rpm<Vector3>(TransformData + oxorany(0x44));
    }

    inline uint64_t photon_ptr(uint64_t p) noexcept {
        return rpm<uint64_t>(p + oxorany(0x160));
    }

    template<typename T>
    inline T property(uint64_t p, const char* tag) noexcept {
        T result{};
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return result;

        uint64_t PropertiesRegistry = rpm<uint64_t>(PhotonPlayer + oxorany(0x38));
        if (!PropertiesRegistry) return result;

        int Count = rpm<int>(PropertiesRegistry + oxorany(0x20));
        uint64_t PropertiesList = rpm<uint64_t>(PropertiesRegistry + oxorany(0x18));

        for (int i = 0; i < Count; i++) {
            uint64_t Key = rpm<uint64_t>(PropertiesList + oxorany(0x28) + oxorany(0x18) * i);
            uint64_t Value = rpm<uint64_t>(PropertiesList + oxorany(0x30) + oxorany(0x18) * i);

            if (!Key) continue;

            std::string KeyString = rpm<read_string>(Key).as_utf8();
            if (strstr(KeyString.c_str(), tag)) {
                result = rpm<T>(Value + oxorany(0x10));
                break;
            }
        }

        return result;
    }

    inline int health(uint64_t p) noexcept {
        return property<int>(p, oxorany("health"));
    }

    inline read_string name(uint64_t p) noexcept {
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return {};
        return rpm<read_string>(rpm<uint64_t>(PhotonPlayer + oxorany(0x20)));
    }

    inline matrix view_matrix(uint64_t p) noexcept {
        uint64_t PlayerMainCamera = rpm<uint64_t>(p + oxorany(0xE8));
        if (!PlayerMainCamera) return {};

        uint64_t CameraTransform = rpm<uint64_t>(PlayerMainCamera + oxorany(0x20));
        if (!CameraTransform) return {};

        uint64_t CameraMatrix = rpm<uint64_t>(CameraTransform + oxorany(0x10));
        if (!CameraMatrix) return {};

        return rpm<matrix>(CameraMatrix + oxorany(0xF0));
    }
    // Читает позицию из Transform-объекта напрямую (не через player chain)
    // Используется aimbot для камеры и костей
    inline Vector3 get_transform_position(uint64_t transform) noexcept {
        if (!transform) return Vector3(0, 0, 0);
        uint64_t TransformData = rpm<uint64_t>(transform + oxorany(0xB0));
        if (!TransformData) return Vector3(0, 0, 0);
        return rpm<Vector3>(TransformData + oxorany(0x44));
    }

    // ================================================================
    //  BONES API (портировано из anuswin, оффсеты совпадают с нашими)
    // ================================================================

    struct TransformEntry {
        Vector4 position;
        Vector4 rotation;
        Vector4 scale;
    };

    inline bool likely_ptr(uint64_t p) noexcept {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    inline bool sane_world_pos(const Vector3& v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
               std::fabs(v.x) < 100000.f && std::fabs(v.y) < 100000.f && std::fabs(v.z) < 100000.f &&
               !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
    }

    inline bool sane_bone_pos(const Vector3& v, const Vector3& base) noexcept {
        if (!sane_world_pos(v)) return false;
        if (!sane_world_pos(base)) return true;

        float dx = v.x - base.x;
        float dy = v.y - base.y;
        float dz = v.z - base.z;
        return (dx * dx + dy * dy + dz * dz) <= 100.f;
    }

    inline Vector3 rotate_by_quat(const Vector3& v, const Vector4& q) noexcept {
        float x2 = q.x + q.x;
        float y2 = q.y + q.y;
        float z2 = q.z + q.z;

        float xx = q.x * x2;
        float yy = q.y * y2;
        float zz = q.z * z2;
        float xy = q.x * y2;
        float xz = q.x * z2;
        float yz = q.y * z2;
        float wx = q.w * x2;
        float wy = q.w * y2;
        float wz = q.w * z2;

        return Vector3(
            (1.f - (yy + zz)) * v.x + (xy - wz) * v.y + (xz + wy) * v.z,
            (xy + wz) * v.x + (1.f - (xx + zz)) * v.y + (yz - wx) * v.z,
            (xz - wy) * v.x + (yz + wx) * v.y + (1.f - (xx + yy)) * v.z
        );
    }

    inline uint64_t biped_map(uint64_t p) noexcept {
        uint64_t view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_1));
        if (likely_ptr(view)) {
            uint64_t map = rpm<uint64_t>(view + oxorany(OFF_VIEW_BIPED_MAP));
            if (likely_ptr(map)) return map;
        }

        view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_2));
        if (likely_ptr(view)) {
            uint64_t map = rpm<uint64_t>(view + oxorany(OFF_VIEW_BIPED_MAP));
            if (likely_ptr(map)) return map;
        }

        return 0;
    }

    inline bool is_visible(uint64_t p) noexcept {
        if (!p) return false;

        uint64_t occlusion = rpm<uint64_t>(p + oxorany(OFF_PLAYER_OCCLUSION));
        if (likely_ptr(occlusion)) {
            int visState = rpm<int>(occlusion + oxorany(OFF_OCCLUSION_CURRENT));
            int occState = rpm<int>(occlusion + oxorany(OFF_OCCLUSION_NEXT));
            return (visState == 2 && occState != 1);
        }

        uint64_t view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_1));
        if (likely_ptr(view)) {
            return rpm<bool>(view + oxorany(0x30));
        }

        return false;
    }

    inline int visibility_state(uint64_t p) noexcept {
        return is_visible(p) ? 2 : 0;
    }

    struct bones_t {
        Vector3 head;
        Vector3 neck;
        Vector3 spine;
        Vector3 spine1;
        Vector3 spine2;
        Vector3 l_shoulder;
        Vector3 l_arm;
        Vector3 l_forearm;
        Vector3 l_hand;
        Vector3 r_shoulder;
        Vector3 r_arm;
        Vector3 r_forearm;
        Vector3 r_hand;
        Vector3 pelvis;
        Vector3 l_thigh;
        Vector3 l_knee;
        Vector3 l_foot;
        Vector3 l_toe;
        Vector3 r_thigh;
        Vector3 r_knee;
        Vector3 r_foot;
        Vector3 r_toe;

        Vector3& operator[](int i) noexcept {
            return ((Vector3*)this)[i];
        }
    };

    inline void apply_default_pose(bones_t& b, const Vector3& root) noexcept {
        b.head      = root + Vector3(0,     1.75f, 0);
        b.neck      = root + Vector3(0,     1.6f,  0);
        b.spine2    = root + Vector3(0,     1.45f, 0);
        b.spine1    = root + Vector3(0,     1.25f, 0);
        b.spine     = root + Vector3(0,     1.05f, 0);
        b.pelvis    = root + Vector3(0,     0.9f,  0);
        b.l_shoulder= root + Vector3(-0.2f, 1.55f, 0);
        b.l_arm     = root + Vector3(-0.4f, 1.5f,  0);
        b.l_forearm = root + Vector3(-0.4f, 1.2f,  0);
        b.l_hand    = root + Vector3(-0.4f, 1.0f,  0);
        b.r_shoulder= root + Vector3(0.2f,  1.55f, 0);
        b.r_arm     = root + Vector3(0.4f,  1.5f,  0);
        b.r_forearm = root + Vector3(0.4f,  1.2f,  0);
        b.r_hand    = root + Vector3(0.4f,  1.0f,  0);
        b.l_thigh   = root + Vector3(-0.15f,0.9f,  0);
        b.l_knee    = root + Vector3(-0.15f,0.45f, 0);
        b.l_foot    = root + Vector3(-0.15f,0.05f, 0);
        b.l_toe     = root + Vector3(-0.15f,0.0f,  0.1f);
        b.r_thigh   = root + Vector3(0.15f, 0.9f,  0);
        b.r_knee    = root + Vector3(0.15f, 0.45f, 0);
        b.r_foot    = root + Vector3(0.15f, 0.05f, 0);
        b.r_toe     = root + Vector3(0.15f, 0.0f,  0.1f);
    }

    inline Vector3 get_bone_world_pos(
        const std::vector<TransformEntry>& matrices,
        const std::vector<int>& parents,
        uint32_t index) noexcept
    {
        uint32_t count = (uint32_t)matrices.size();
        if (index >= count) return {0,0,0};

        const TransformEntry& tm = matrices[index];
        Vector3 result = {tm.position.x, tm.position.y, tm.position.z};
        int p_idx = parents[index];
        int depth = 0;

        while (p_idx >= 0 && p_idx < (int)count && depth < 64) {
            const TransformEntry& p_tm = matrices[p_idx];
            float rx = p_tm.rotation.x, ry = p_tm.rotation.y,
                  rz = p_tm.rotation.z, rw = p_tm.rotation.w;
            float sx = result.x * p_tm.scale.x;
            float sy = result.y * p_tm.scale.y;
            float sz = result.z * p_tm.scale.z;

            result.x = p_tm.position.x + sx
                + sx * (ry*ry*-2.f - rz*rz*2.f)
                + sy * (rw*rz*-2.f - ry*rx*-2.f)
                + sz * (rz*rx*2.f  - rw*ry*-2.f);
            result.y = p_tm.position.y + sy
                + sx * (rx*ry*2.f  - rw*rz*-2.f)
                + sy * (rz*rz*-2.f - rx*rx*2.f)
                + sz * (rw*rx*-2.f - rz*ry*-2.f);
            result.z = p_tm.position.z + sz
                + sx * (rw*ry*-2.f - rx*rz*-2.f)
                + sy * (ry*rz*2.f  - rw*rx*-2.f)
                + sz * (rx*rx*-2.f - ry*ry*2.f);

            p_idx = parents[p_idx];
            depth++;
        }
        return result;
    }

    inline bool transform_position(uint64_t native, Vector3& out) noexcept {
        if (!likely_ptr(native)) return false;

        uint64_t mdata = rpm<uint64_t>(native + oxorany(OFF_TRANSFORM_MATRIX));
        if (!likely_ptr(mdata)) return false;

        uint32_t idx = rpm<uint32_t>(native + oxorany(OFF_TRANSFORM_INDEX));
        if (idx >= 10000) return false;

        uint64_t mlist = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_LIST));
        uint64_t midx  = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_INDICES));
        if (!likely_ptr(mlist) || !likely_ptr(midx)) return false;

        uint32_t count = idx + 1;
        std::vector<TransformEntry> mats(count);
        std::vector<int>            pars(count);
        if (!mem_read(mlist, mats.data(), count * sizeof(TransformEntry))) return false;
        if (!mem_read(midx,  pars.data(), count * sizeof(int)))            return false;

        Vector3 r = get_bone_world_pos(mats, pars, idx);
        if (!sane_world_pos(r)) return false;
        out = r;
        return true;
    }

    inline bool read_biped_bone(uint64_t map, int bone, const Vector3& base, Vector3& out) noexcept {
        if (!likely_ptr(map) || bone < 0 || bone >= BIPED_BONE_COUNT) return false;

        uint64_t transform = rpm<uint64_t>(map + oxorany(OFF_BIPED_START) + bone * oxorany(OFF_BIPED_STRIDE));
        if (!likely_ptr(transform)) return false;

        Vector3 pos{};
        if (!transform_position(transform, pos)) return false;
        if (!sane_bone_pos(pos, base)) return false;

        out = pos;
        return true;
    }

    inline bool bone_position(uint64_t p, int bone_mode, const Vector3& base, Vector3& out) noexcept {
        uint64_t map = biped_map(p);
        if (!likely_ptr(map)) return false;

        if (bone_mode == 1) {
            return read_biped_bone(map, BONE_HEAD, base, out);
        }

        if (bone_mode == 2) {
            return read_biped_bone(map, BONE_SPINE2, base, out) ||
                   read_biped_bone(map, BONE_SPINE1, base, out);
        }

        return read_biped_bone(map, BONE_HEAD, base, out);
    }

    inline bool bone_position(uint64_t p, int bone_mode, Vector3& out) noexcept {
        return bone_position(p, bone_mode, position(p), out);
    }

    struct bone_cache_entry_t {
        Vector3 offsets[BIPED_BONE_COUNT];
        bool valid = false;
    };
    inline std::unordered_map<uint64_t, bone_cache_entry_t> g_bone_cache;
    inline uint64_t g_bone_cache_pm = 0;

    inline bool get_bones(uint64_t p, bones_t& b) noexcept {
        uint64_t cur_pm = get_player_manager();
        if (cur_pm != g_bone_cache_pm) {
            g_bone_cache.clear();
            g_bone_cache_pm = cur_pm;
        }

        Vector3 root = position(p);
        if (root.x == 0.f && root.y == 0.f && root.z == 0.f) return false;

        auto fallback = [&]() -> bool {
            auto it = g_bone_cache.find(p);
            if (it != g_bone_cache.end() && it->second.valid) {
                for (int i = 0; i < BIPED_BONE_COUNT; i++)
                    b[i] = root + it->second.offsets[i];
                return true;
            }
            apply_default_pose(b, root);
            return true;
        };

        // Читаем кости всегда: is_visible/occlusion может лгать на разных
        // версиях клиента, а валидность позы защищена d-фильтром ниже.
        // (fallback на статичную позу остаётся при битых матрицах)

        uint64_t map = biped_map(p);
        if (!likely_ptr(map)) return fallback();

        uint64_t ptrs[BIPED_BONE_COUNT];
        if (!mem_read(map + oxorany(OFF_BIPED_START),
                      ptrs, sizeof(ptrs))) return fallback();

        uint32_t transform_indices[BIPED_BONE_COUNT];
        uint64_t matrix_list = 0, matrix_indices_ptr = 0;
        uint32_t max_index = 0;

        for (int i = 0; i < BIPED_BONE_COUNT; i++) {
            transform_indices[i] = 0xFFFFFFFF;
            if (!likely_ptr(ptrs[i])) continue;
            uint64_t native = ptrs[i];
            uint32_t idx = rpm<uint32_t>(native + oxorany(OFF_TRANSFORM_INDEX));
            transform_indices[i] = idx;
            if (idx > max_index && idx < 10000) max_index = idx;
            if (!matrix_list) {
                uint64_t mdata = rpm<uint64_t>(native + oxorany(OFF_TRANSFORM_MATRIX));
                if (likely_ptr(mdata)) {
                    matrix_list        = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_LIST));
                    matrix_indices_ptr = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_INDICES));
                }
            }
        }

        if (!matrix_list || !matrix_indices_ptr || max_index == 0) return fallback();

        uint32_t count = max_index + 1;
        if (count > 10000) return fallback();

        std::vector<TransformEntry> all_matrices(count);
        std::vector<int>            all_parents(count);
        if (!mem_read(matrix_list,        all_matrices.data(), count * sizeof(TransformEntry))) return fallback();
        if (!mem_read(matrix_indices_ptr, all_parents.data(),  count * sizeof(int)))            return fallback();

        bool any_valid = false;
        for (int i = 0; i < BIPED_BONE_COUNT; i++) {
            if (transform_indices[i] != 0xFFFFFFFF) {
                b[i] = get_bone_world_pos(all_matrices, all_parents, transform_indices[i]);
                if (b[i].x != 0.f || b[i].y != 0.f) any_valid = true;
            } else {
                b[i] = {0, 0, 0};
            }
        }

        if (any_valid) {
            float d = (b[1] - root).magnitude();
            if (d > 0.1f && d < 3.0f) {
                // Санити позы: отсекаем «перевёрнутые»/битые скелеты.
                // Unity: y — вверх. Голова должна быть выше таза, ноги — ниже.
                auto valid3 = [](const Vector3& v) {
                    return v.x != 0.f || v.y != 0.f || v.z != 0.f;
                };
                bool pose_sane = true;
                if (valid3(b[BONE_HEAD]) && valid3(b[BONE_HIP])) {
                    if (b[BONE_HEAD].y < b[BONE_HIP].y - 0.1f) pose_sane = false;
                }
                if (valid3(b[BONE_LEFT_FOOT]) && valid3(b[BONE_HIP])) {
                    if (b[BONE_LEFT_FOOT].y > b[BONE_HIP].y + 0.1f) pose_sane = false;
                }
                if (valid3(b[BONE_RIGHT_FOOT]) && valid3(b[BONE_HIP])) {
                    if (b[BONE_RIGHT_FOOT].y > b[BONE_HIP].y + 0.1f) pose_sane = false;
                }
                if (!pose_sane) return fallback();

                auto& cache = g_bone_cache[p];
                for (int i = 0; i < BIPED_BONE_COUNT; i++)
                    cache.offsets[i] = b[i] - root;
                cache.valid = true;
                return true;
            }
        }
        return fallback();
    }

    // Позиция камеры (логика: p+0x28 как прямой native transform → main_camera chain → position)
    inline Vector3 get_transform_position_full(uint64_t native) noexcept {
        if (!likely_ptr(native)) return {0,0,0};
        uint64_t mdata = rpm<uint64_t>(native + oxorany(OFF_TRANSFORM_MATRIX));
        if (!likely_ptr(mdata)) return {0,0,0};
        uint64_t mlist = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_LIST));
        uint64_t midx  = rpm<uint64_t>(mdata + oxorany(OFF_MATRIX_INDICES));
        if (!likely_ptr(mlist) || !likely_ptr(midx)) return {0,0,0};
        uint32_t idx = rpm<uint32_t>(native + oxorany(OFF_TRANSFORM_INDEX));
        if (idx >= 10000) return {0,0,0};
        uint32_t count = idx + 1;
        std::vector<TransformEntry> mats(count);
        std::vector<int>            pars(count);
        if (!mem_read(mlist, mats.data(), count * sizeof(TransformEntry))) return {0,0,0};
        if (!mem_read(midx,  pars.data(), count * sizeof(int)))            return {0,0,0};
        return get_bone_world_pos(mats, pars, idx);
    }

    inline Vector3 camera_position(uint64_t p) noexcept {
        if (!p) return {0,0,0};

        // Путь 1: p+0x28 как прямой transform-ptr
        {
            uint64_t transform = rpm<uint64_t>(p + oxorany(0x28));
            if (likely_ptr(transform)) {
                Vector3 r = get_transform_position_full(transform);
                if (sane_world_pos(r)) return r;
            }
        }

        // Путь 2: main_camera → transform chain
        {
            uint64_t cam = rpm<uint64_t>(p + oxorany(OFF_PLAYER_MAIN_CAMERA));
            if (likely_ptr(cam)) {
                uint64_t cam_transform = rpm<uint64_t>(cam + oxorany(OFF_CAM_TRANSFORM));
                if (likely_ptr(cam_transform)) {
                    Vector3 r = get_transform_position_full(cam_transform);
                    if (sane_world_pos(r)) return r;
                }
            }
        }

        return position(p);
    }

}
