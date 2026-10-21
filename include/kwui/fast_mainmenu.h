#ifndef KWUI_FAST_MAINMENU_H
#define KWUI_FAST_MAINMENU_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* ResourceManagerFileStream returns a single binary asset, including its 12-byte
 * inline data header. Validate raw retail MainMenu APT before relocation. The
 * patches below select its interval and retime the named reveal/menu clips. */
#define KWUI_MAINMENU_APT_TYPE UINT32_C(0x6c41e6dc)
#define KWUI_MAINMENU_APT_ID UINT32_C(0x5b2421ce)
#define KWUI_MAINMENU_FAST_MS 33U
#define KWUI_MAINMENU_TICK_MS 33U
#define KWUI_MAINMENU_MENU_TICKS 15U
#define KWUI_MAINMENU_BG_FRAMES 9U
#define KWUI_MAINMENU_GLOBE_FRAMES 9U
/* Globe starts at 0; BgReveal resumes at its relocated label, frame 1.
 * Menu.Reveal retains label 9 and groups its 40 updates into 15 ticks. */
#define KWUI_MAINMENU_REVEAL_MS ((KWUI_MAINMENU_GLOBE_FRAMES - 1U + KWUI_MAINMENU_BG_FRAMES - 2U) * KWUI_MAINMENU_TICK_MS)
#define KWUI_MAINMENU_ENTRIES_MS (KWUI_MAINMENU_MENU_TICKS * KWUI_MAINMENU_TICK_MS)
static inline uint32_t kwui_menu_word(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
/* Locate a unique recognized root without changing bytes. */
static inline int kwui_menu_root(unsigned char *data, size_t size, unsigned char **out, uint32_t *length, size_t *root)
{
    static const unsigned char magic[] = "Apt Data:74\x1a";
    if (!data || size < 76 || kwui_menu_word(data) != 0 || kwui_menu_word(data + 4) != 12)
        return 0;
    uint32_t bytes = kwui_menu_word(data + 8);
    if (bytes > size - 12 || bytes < 64 || memcmp(data + 12, magic, sizeof(magic) - 1))
        return 0;
    unsigned char *apt = data + 12;
    size_t candidate = 0;
    unsigned matches = 0;
    for (size_t at = 16; at + 60 <= bytes; at += 4) {
        if (kwui_menu_word(apt + at) != 9 || kwui_menu_word(apt + at + 4) != 0x09876543U)
            continue;
        /* Retail movie shape. Refuse replaced/custom main-menu assets rather
         * than treating a coincidental integer sequence as a movie header. */
        uint32_t frames = kwui_menu_word(apt + at + 12), characters = kwui_menu_word(apt + at + 24);
        if (kwui_menu_word(apt + at + 8) != 1 || kwui_menu_word(apt + at + 20) != 415 || kwui_menu_word(apt + at + 28) != 1024 ||
            kwui_menu_word(apt + at + 32) != 768 || (uint64_t)frames + 8 > bytes || (uint64_t)characters + 415U * 4U > bytes)
            continue;
        uint32_t ms = kwui_menu_word(apt + at + 36);
        if (ms != 33 && ms != 13 && ms != 4 && ms != 1)
            continue;
        candidate = at + 36;
        ++matches;
    }
    if (matches != 1)
        return 0;
    *out = apt;
    *length = bytes;
    *root = candidate - 36;
    return 1;
}
static inline void kwui_menu_put(unsigned char *p, uint32_t v)
{
    for (unsigned j = 0; j < 4; ++j)
        p[j] = (unsigned char)(v >> (8 * j));
}
/* 1 patched, 2 already fast, 0 unfamiliar/truncated data (unchanged). */
static inline int kwui_fast_mainmenu_asset_mode(unsigned char *data, size_t size, size_t *changed, unsigned animations_off)
{
    unsigned char *apt;
    uint32_t bytes;
    size_t root;
    if (!kwui_menu_root(data, size, &apt, &bytes, &root))
        return 0;
    size_t candidate = root + 36;
    (void)bytes;
    if (changed)
        *changed = candidate + 12;
    /* Both modes retain stock cadence; visual frames select their behavior. */
    (void)animations_off;
    uint32_t ms = KWUI_MAINMENU_FAST_MS;
    if (kwui_menu_word(apt + candidate) == ms)
        return 2;
    kwui_menu_put(apt + candidate, ms);
    return 1;
}
static inline int kwui_fast_mainmenu_asset(unsigned char *data, size_t size, size_t *changed)
{
    return kwui_fast_mainmenu_asset_mode(data, size, changed, 0);
}
/* A shorter interval may still be limited by native presentation scheduling.
 * Sample the two reveal clips' absolute transform frames while retaining their
 * setup, label, endpoints and callbacks, to target half a second per phase. All
 * offsets below are read from bounded raw APT tables before engine relocation. */
static inline int kwui_menu_range(uint32_t bytes, uint32_t at, uint32_t n)
{
    return at <= bytes && n <= bytes - at;
}
static inline int kwui_menu_string(const unsigned char *apt, uint32_t bytes, uint32_t at, const char *text)
{
    size_t n = strlen(text) + 1;
    return n <= bytes && kwui_menu_range(bytes, at, (uint32_t)n) && !memcmp(apt + at, text, n);
}
struct kwui_menu_reveal_plan {
    uint32_t sprite, table, count;
};
/* Return a frame's bounded control-pointer array. */
static inline int kwui_menu_frame(const unsigned char *apt, uint32_t bytes, uint32_t table, uint32_t f, uint32_t *count, uint32_t *items)
{
    if (f > 4096 || !kwui_menu_range(bytes, table, 8 * (f + 1)))
        return 0;
    *count = kwui_menu_word(apt + table + 8 * f);
    *items = kwui_menu_word(apt + table + 8 * f + 4);
    return *count <= 4096 && kwui_menu_range(bytes, *items, *count * 4);
}
static inline int kwui_menu_callback(const unsigned char *apt, uint32_t bytes, uint32_t action, const char *target)
{
    if (!kwui_menu_range(bytes, action, 27))
        return 0;
    return apt[action] == 0x59 && apt[action + 1] == 0xa4 && apt[action + 8] == 0xa5 && apt[action + 16] == 0xa1 && apt[action + 24] == 0x5d &&
           apt[action + 25] == 7 && apt[action + 26] == 0 && kwui_menu_string(apt, bytes, kwui_menu_word(apt + action + 4), "_parent") &&
           kwui_menu_string(apt, bytes, kwui_menu_word(apt + action + 12), target) &&
           kwui_menu_string(apt, bytes, kwui_menu_word(apt + action + 20), "Reveal");
}
static inline int kwui_menu_plan(const unsigned char *apt, uint32_t bytes, uint32_t sprite, unsigned which, uint32_t wanted,
                                 struct kwui_menu_reveal_plan *plan)
{
    if (!kwui_menu_range(bytes, sprite, 16) || kwui_menu_word(apt + sprite) != 5 || kwui_menu_word(apt + sprite + 4) != 0x09876543U)
        return 0;
    uint32_t n = kwui_menu_word(apt + sprite + 8), table = kwui_menu_word(apt + sprite + 12);
    uint32_t stock = which ? 36 : 26;
    if (n != stock && n != wanted)
        return 0;
    if (!kwui_menu_range(bytes, table, n * 8))
        return 0;
    /* Idempotence checks the shortened clip with the same structural rules. */
    for (uint32_t f = 0; f < n; ++f) {
        uint32_t count, items;
        if (!kwui_menu_frame(apt, bytes, table, f, &count, &items))
            return 0;
        uint32_t label = n == stock ? 4 : 1, final = n - 1;
        uint32_t expected = f == 0 ? 2 : f == label ? 1 : f == final ? (which ? 3 : 2) : f < label ? 0 : (which ? 2 : 1);
        if (count != expected)
            return 0;
        for (uint32_t j = 0; j < count; ++j) {
            uint32_t item = kwui_menu_word(apt + items + j * 4);
            if (!kwui_menu_range(bytes, item, 8))
                return 0;
            uint32_t type = kwui_menu_word(apt + item);
            if (f == label) {
                if (type != 2 || !kwui_menu_string(apt, bytes, kwui_menu_word(apt + item + 4), "_reveal"))
                    return 0;
            } else if (f == final) {
                if (j == count - 1) {
                    if (type != 1 || !kwui_menu_callback(apt, bytes, kwui_menu_word(apt + item + 4), which ? "BgReveal" : "Menu"))
                        return 0;
                } else if (type != 4 || kwui_menu_word(apt + item + 4) != (j ? 3U : 1U))
                    return 0;
            } else if (f == 0 && !which && j == 0) {
                uint32_t action = kwui_menu_word(apt + item + 4);
                if (type != 1 || !kwui_menu_range(bytes, action, 39) || apt[action] != 0x9b ||
                    !kwui_menu_string(apt, bytes, kwui_menu_word(apt + action + 4), "Reveal") || kwui_menu_word(apt + action + 8) != 0 ||
                    kwui_menu_word(apt + action + 16) != 9 || apt[action + 28] != 0x8c ||
                    !kwui_menu_string(apt, bytes, kwui_menu_word(apt + action + 32), "_reveal") || apt[action + 36] != 6 || apt[action + 37] != 7 ||
                    apt[action + 38] != 0)
                    return 0;
            } else {
                if (type != 3 || !kwui_menu_range(bytes, item, 64) || kwui_menu_word(apt + item + 4) != (f ? 5U : 6U) ||
                    kwui_menu_word(apt + item + 8) != (which && j ? 3U : 1U))
                    return 0;
            }
        }
    }
    plan->sprite = sprite;
    plan->table = table;
    plan->count = n;
    return 1;
}
/* Short-lived objects collapsed by off mode must contain no ActionScript,
 * clip events or nested timelines: just static shape wrappers. */
static inline int kwui_menu_static_visual(const unsigned char *apt, uint32_t bytes, uint32_t chars, uint32_t ch, unsigned depth)
{
    if (ch >= 415 || depth > 8)
        return 0;
    uint32_t sprite = kwui_menu_word(apt + chars + ch * 4);
    if (!kwui_menu_range(bytes, sprite, 16) || kwui_menu_word(apt + sprite + 4) != 0x09876543U)
        return 0;
    uint32_t type = kwui_menu_word(apt + sprite);
    if (type == 1)
        return 1; /* shape geometry has no movie actions */
    if (type != 5 || kwui_menu_word(apt + sprite + 8) != 1)
        return 0;
    uint32_t count, items;
    if (!kwui_menu_frame(apt, bytes, kwui_menu_word(apt + sprite + 12), 0, &count, &items) || count != 1)
        return 0;
    uint32_t item = kwui_menu_word(apt + items);
    if (!kwui_menu_range(bytes, item, 64) || kwui_menu_word(apt + item) != 3 || kwui_menu_word(apt + item + 4) != 6)
        return 0;
    return kwui_menu_static_visual(apt, bytes, chars, kwui_menu_word(apt + item + 12), depth + 1);
}
/* The Menu clip uses contiguous control-pointer lists. Group consecutive frames
 * without dropping/reordering a single control, creation, removal or action. */
static inline int kwui_menu_entries_plan(const unsigned char *apt, uint32_t bytes, uint32_t sprite, uint32_t chars, unsigned animations_off,
                                         struct kwui_menu_reveal_plan *plan)
{
    if (!kwui_menu_range(bytes, sprite, 16) || kwui_menu_word(apt + sprite) != 5 || kwui_menu_word(apt + sprite + 4) != 0x09876543U)
        return 0;
    uint32_t n = kwui_menu_word(apt + sprite + 8), table = kwui_menu_word(apt + sprite + 12);
    uint32_t target = animations_off ? 10U : 10U + KWUI_MAINMENU_MENU_TICKS;
    if ((n != 50 && n != target) || !kwui_menu_range(bytes, table, n * 8))
        return 0;
    uint32_t next = 0, total = 0, definitions = 0, stops = 0, labels = 0;
    for (uint32_t f = 0; f < n; ++f) {
        uint32_t count, items;
        if (!kwui_menu_frame(apt, bytes, table, f, &count, &items))
            return 0;
        if ((f == 0 && count != 6) || (f > 0 && f < 9 && count) || (f == 9 && count != (animations_off && n == 10 ? 112U : 5U)))
            return 0;
        if (count) {
            if (next && items != next)
                return 0;
            next = items + count * 4;
        }
        total += count;
        if (total > 118)
            return 0;
        for (uint32_t j = 0; j < count; ++j) {
            uint32_t item = kwui_menu_word(apt + items + 4 * j);
            if (!kwui_menu_range(bytes, item, 8))
                return 0;
            uint32_t type = kwui_menu_word(apt + item), value = kwui_menu_word(apt + item + 4);
            if (type == 1) {
                if (f == 0 && j == 0) {
                    if (!kwui_menu_range(bytes, value, 39) || apt[value] != 0x9b ||
                        !kwui_menu_string(apt, bytes, kwui_menu_word(apt + value + 4), "Reveal") || kwui_menu_word(apt + value + 8) != 0 ||
                        kwui_menu_word(apt + value + 16) != 9 || apt[value + 28] != 0x8c ||
                        !kwui_menu_string(apt, bytes, kwui_menu_word(apt + value + 32), "_reveal") || apt[value + 36] != 6 || apt[value + 37] != 7 ||
                        apt[value + 38] != 0)
                        return 0;
                    ++definitions;
                } else {
                    if (f != n - 1 || !kwui_menu_range(bytes, value, 2) || apt[value] != 7 || apt[value + 1] != 0)
                        return 0;
                    ++stops;
                }
            } else if (type == 2) {
                if (f != 9 || j != 1 || !kwui_menu_string(apt, bytes, value, "_reveal"))
                    return 0;
                ++labels;
            } else if (type == 3) {
                if (!kwui_menu_range(bytes, item, 64) || (value & ~0x7fU) || ((value & 2) && kwui_menu_word(apt + item + 12) >= 415))
                    return 0;
                uint32_t at_depth = kwui_menu_word(apt + item + 8);
                if (animations_off && f >= 9 && (value & 2) && !(value & 1) &&
                    (at_depth == 243 || at_depth == 245 || at_depth == 249 || at_depth == 251) &&
                    !kwui_menu_static_visual(apt, bytes, chars, kwui_menu_word(apt + item + 12), 0))
                    return 0;
            } else if (type == 4) {
                if (f < 9 || (value != 243 && value != 245 && value != 249 && value != 251))
                    return 0;
            } else
                return 0;
        }
    }
    if (total != 118 || definitions != 1 || stops != 1 || labels != 1)
        return 0;
    plan->sprite = sprite;
    plan->table = table;
    plan->count = n;
    return 1;
}
static inline void kwui_menu_entries_apply(unsigned char *apt, const struct kwui_menu_reveal_plan *plan, unsigned animations_off)
{
    if (animations_off) {
        uint32_t total = 0, items = kwui_menu_word(apt + plan->table + 9 * 8 + 4);
        for (uint32_t f = 9; f < 50; ++f)
            total += kwui_menu_word(apt + plan->table + f * 8);
        kwui_menu_put(apt + plan->table + 9 * 8, total);
        kwui_menu_put(apt + plan->table + 9 * 8 + 4, items);
        kwui_menu_put(apt + plan->sprite + 8, 10);
        return;
    }
    unsigned char frames[(10 + KWUI_MAINMENU_MENU_TICKS) * 8];
    memcpy(frames, apt + plan->table, 10 * 8);
    for (uint32_t step = 0; step < KWUI_MAINMENU_MENU_TICKS; ++step) {
        uint32_t begin = 10 + step * 40 / KWUI_MAINMENU_MENU_TICKS;
        uint32_t end = 10 + (step + 1) * 40 / KWUI_MAINMENU_MENU_TICKS, total = 0;
        for (uint32_t f = begin; f < end; ++f)
            total += kwui_menu_word(apt + plan->table + f * 8);
        kwui_menu_put(frames + (step + 10) * 8, total);
        kwui_menu_put(frames + (step + 10) * 8 + 4, kwui_menu_word(apt + plan->table + begin * 8 + 4));
    }
    memcpy(apt + plan->table, frames, sizeof(frames));
    kwui_menu_put(apt + plan->sprite + 8, 10 + KWUI_MAINMENU_MENU_TICKS);
}
/* 1 shortened, 2 already short, 0 unfamiliar; validate all clips before writing. */
static inline int kwui_fast_mainmenu_reveals_mode(unsigned char *data, size_t size, unsigned animations_off)
{
    unsigned char *apt;
    uint32_t bytes;
    size_t root;
    if (!kwui_menu_root(data, size, &apt, &bytes, &root))
        return 0;
    uint32_t chars = kwui_menu_word(apt + root + 24), found[3] = {0, 0, 0}, work = 0;
    unsigned hits[3] = {0, 0, 0}, parents[3] = {0, 0, 0};
    /* Discover the clips from their named placements, not character numbers. */
    for (unsigned c = 0; c < 415; ++c) {
        uint32_t p = kwui_menu_word(apt + chars + 4 * c);
        if (!p)
            continue;
        if (!kwui_menu_range(bytes, p, 16))
            return 0;
        if (kwui_menu_word(apt + p) != 5)
            continue;
        uint32_t n = kwui_menu_word(apt + p + 8), table = kwui_menu_word(apt + p + 12);
        if (n > 4096 || n > 200000 - work)
            return 0;
        work += n;
        for (uint32_t f = 0; f < n; ++f) {
            uint32_t count, items;
            if (!kwui_menu_frame(apt, bytes, table, f, &count, &items))
                return 0;
            if (count > 200000 - work)
                return 0;
            work += count;
            for (uint32_t j = 0; j < count; ++j) {
                uint32_t item = kwui_menu_word(apt + items + 4 * j);
                if (!kwui_menu_range(bytes, item, 8))
                    return 0;
                if (kwui_menu_word(apt + item) != 3 || !(kwui_menu_word(apt + item + 4) & 32))
                    continue;
                if (!kwui_menu_range(bytes, item, 64))
                    return 0;
                uint32_t name = kwui_menu_word(apt + item + 52), ch = kwui_menu_word(apt + item + 12);
                for (unsigned k = 0; k < 3; ++k)
                    if (kwui_menu_string(apt, bytes, name, k == 0 ? "BgReveal" : k == 1 ? "GlobeReveal" : "Menu")) {
                        if (ch >= 415)
                            return 0;
                        found[k] = kwui_menu_word(apt + chars + 4 * ch);
                        parents[k] = c;
                        ++hits[k];
                    }
            }
        }
    }
    uint32_t target[2] = {animations_off ? 3U : KWUI_MAINMENU_BG_FRAMES, animations_off ? 3U : KWUI_MAINMENU_GLOBE_FRAMES};
    struct kwui_menu_reveal_plan plan[3];
    if (hits[0] != 1 || hits[1] != 1 || parents[0] != parents[1] || found[0] == found[1] ||
        !kwui_menu_plan(apt, bytes, found[0], 0, target[0], &plan[0]) || !kwui_menu_plan(apt, bytes, found[1], 1, target[1], &plan[1]))
        return 0;
    if (hits[2] != 1 || parents[2] != parents[0] || found[2] == found[0] || found[2] == found[1] ||
        !kwui_menu_entries_plan(apt, bytes, found[2], chars, animations_off, &plan[2]))
        return 0;
    /* Reject overlapping frame tables before writing any clip. */
    for (unsigned k = 0; k < 3U; ++k)
        for (unsigned j = 0; j < k; ++j)
            if (plan[k].table < plan[j].table + plan[j].count * 8 && plan[j].table < plan[k].table + plan[k].count * 8)
                return 0;
    int changed = 0;
    for (unsigned k = 0; k < 2; ++k) {
        uint32_t wanted = target[k];
        if (plan[k].count == wanted)
            continue;
        unsigned char frames[KWUI_MAINMENU_GLOBE_FRAMES * 8];
        for (uint32_t f = 0; f < wanted; ++f) {
            /* Uniformly sample the original tween, including both endpoints.
             * No action or object lifetime operation is skipped. */
            uint32_t source = f == 0            ? 0
                              : f == 1          ? 4
                              : f == wanted - 1 ? plan[k].count - 1
                              : wanted == 4     ? plan[k].count - 2
                                                : 5 + (f - 2) * (plan[k].count - 7) / (wanted - 4);
            memcpy(frames + 8 * f, apt + plan[k].table + 8 * source, 8);
        }
        memcpy(apt + plan[k].table, frames, wanted * 8);
        kwui_menu_put(apt + plan[k].sprite + 8, wanted);
        changed = 1;
    }
    if (plan[2].count == 50) {
        kwui_menu_entries_apply(apt, &plan[2], animations_off);
        changed = 1;
    }
    return changed ? 1 : 2;
}
static inline int kwui_fast_mainmenu_reveals(unsigned char *data, size_t size)
{
    return kwui_fast_mainmenu_reveals_mode(data, size, 0);
}
#endif
