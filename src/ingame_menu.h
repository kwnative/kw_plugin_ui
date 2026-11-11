/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KW_UI_INGAME_MENU_H
#define KW_UI_INGAME_MENU_H
#include <kwui/fast_mainmenu.h>
/* Raw TacticalPauseDialog APT, before engine relocation. No global clock edits. */
#define KW_INGAME_MENU_APT_ID UINT32_C(0x8efa7dda)
#define KW_INGAME_MENU_BOX_TICKS 3U
#define KW_INGAME_MENU_BUTTON_TICKS 12U
#define KW_INGAME_MENU_TOTAL_MS ((KW_INGAME_MENU_BOX_TICKS + KW_INGAME_MENU_BUTTON_TICKS) * 33U)
struct kw_ingame_span {
    uint16_t end, fast, off;
};
struct kw_ingame_mark {
    uint16_t frame, count;
};
struct kw_ingame_label {
    uint16_t frame;
    const char *name;
};
struct kw_ingame_clip {
    uint32_t sprite, table, frames, target;
    const struct kw_ingame_span *spans;
    size_t span_count;
    const struct kw_ingame_mark *marks;
    size_t mark_count;
    const struct kw_ingame_label *labels;
    size_t label_count;
    uint32_t counts[107], items[107], label_items[9];
};
static const struct kw_ingame_span kw_ingame_spans[][11] = {
    /* Box/stage setup finishes at tick 3. Button placements occupy 3..10;
     * each button reaches its opening stop after five more ticks, last at 15.
     * Zero-length spans merge their controls into the following frame. */
    /* Closing must outlive the selected button's deferred execute action.
     * Fast preserves stock press/close cadence; off keeps distinct input,
     * stopDown-guard and execute ticks before either OnClosed notification. */
    {{1, 1, 1}, {18, 2, 0}, {23, 0, 0}, {39, 8, 0}, {46, 1, 1}, {59, 13, 6}},
    {{27, 1, 1}, {39, 12, 5}},
    /* Hover keeps every stock tick. Fast also keeps all press/out ticks.
     * Off compresses press/out but never merges the frame-53 stopDown guard
     * with frame-49 entry or frame-58 execution. Numeric goto(20) targets an
     * unchanged nested bar clip. */
    {{16, 2, 1}, {25, 4, 1}, {49, 24, 24}, {53, 4, 1}, {54, 1, 1}, {59, 5, 1}, {69, 10, 2}, {77, 3, 2}, {86, 3, 2}, {100, 5, 2}, {107, 3, 2}},
    {{1, 1, 1}, {10, 3, 1}, {20, 3, 1}}};
static const struct kw_ingame_mark kw_ingame_root_marks[] = {{0, 3},  {18, 1}, {23, 1}, {26, 1}, {29, 1}, {32, 1},
                                                             {35, 1}, {38, 1}, {41, 1}, {45, 2}, {46, 1}, {58, 1}};
static const struct kw_ingame_mark kw_ingame_frame_marks[] = {{0, 3},  {26, 1}, {27, 2}, {28, 2}, {29, 2}, {30, 2}, {31, 2},
                                                              {32, 2}, {33, 2}, {34, 2}, {35, 1}, {36, 1}, {37, 1}, {38, 2}};
static const struct kw_ingame_mark kw_ingame_button_marks[] = {
    {0, 3},  {16, 1}, {24, 1}, {25, 4}, {48, 1}, {49, 3}, {53, 3}, {58, 1}, {59, 2},  {68, 1},  {69, 4},  {70, 1},  {71, 1},  {72, 1},  {73, 1},
    {74, 1}, {75, 1}, {76, 2}, {77, 3}, {78, 1}, {79, 1}, {80, 1}, {81, 1}, {82, 1},  {83, 1},  {84, 1},  {85, 2},  {86, 2},  {87, 1},  {88, 1},
    {89, 1}, {90, 1}, {91, 1}, {92, 1}, {93, 1}, {94, 1}, {95, 1}, {99, 1}, {100, 1}, {101, 1}, {102, 1}, {103, 1}, {104, 1}, {105, 1}, {106, 2}};
static const struct kw_ingame_mark kw_ingame_backdrop_marks[] = {{0, 2},  {1, 2},  {2, 1},  {3, 1},  {4, 1},  {5, 1},  {6, 1},
                                                                 {7, 1},  {8, 1},  {9, 2},  {10, 2}, {11, 1}, {12, 1}, {13, 1},
                                                                 {14, 1}, {15, 1}, {16, 1}, {17, 1}, {18, 1}, {19, 2}};
static const struct kw_ingame_label kw_ingame_root_labels[] = {{46, "_close"}};
static const struct kw_ingame_label kw_ingame_frame_labels[] = {{0, "_in"}, {27, "_out"}};
static const struct kw_ingame_label kw_ingame_button_labels[] = {
    {0, "__fadein"},          {16, "_up"}, {25, "_over"}, {49, "_down"}, {59, "_out"}, {69, "_fadeout"}, {77, "_fadeout1"}, {86, "_fadeinDisabled"},
    {100, "_fadeoutDisabled"}};
static const struct kw_ingame_label kw_ingame_backdrop_labels[] = {{1, "_open"}, {10, "_close"}};
static inline uint32_t kw_ingame_destination(const struct kw_ingame_clip *p, uint32_t frame, unsigned off)
{
    uint32_t start = 0, dest = 0;
    for (size_t i = 0; i < p->span_count; ++i) {
        const struct kw_ingame_span *s = &p->spans[i];
        uint32_t n = off ? s->off : s->fast;
        if (frame < s->end)
            return dest + (frame - start) * n / (s->end - start);
        dest += n;
        start = s->end;
    }
    return UINT32_MAX;
}
static inline int kw_ingame_string_valid(const unsigned char *apt, uint32_t bytes, uint32_t at)
{
    if (at >= bytes)
        return 0;
    for (uint32_t i = 0; i < 128 && i < bytes - at; ++i)
        if (!apt[at + i])
            return 1;
    return 0;
}
static inline int kw_ingame_overlap(uint32_t a, uint32_t n, uint32_t b, uint32_t m)
{
    return (uint64_t)a + n > b && (uint64_t)b + m > a;
}
/* The four authored frame tables fit in their existing allocation. Validate
 * every frame and control before writing any table. Every action, placement,
 * clip event and removal survives, in the same order, with identical action/placement payload. Label frame metadata follows its new tick. */
static inline int kw_ingame_plan(const unsigned char *apt, uint32_t bytes, struct kw_ingame_clip *p, unsigned off, int already)
{
    uint32_t stock = p->spans[p->span_count - 1].end;
    p->target = kw_ingame_destination(p, stock - 1, off) + 1;
    p->frames = kwui_menu_word(apt + p->sprite + 8);
    p->table = kwui_menu_word(apt + p->sprite + 12);
    if (p->frames != (already ? p->target : stock) || !kwui_menu_range(bytes, p->table, stock * 8))
        return 0;
    uint32_t expected[107] = {0}, next = 0, labels = 0, work = 0;
    for (size_t j = 0; j < p->mark_count; ++j) {
        uint32_t f = already ? kw_ingame_destination(p, p->marks[j].frame, off) : p->marks[j].frame;
        expected[f] += p->marks[j].count;
    }
    memset(p->counts, 0, sizeof(p->counts));
    memset(p->items, 0, sizeof(p->items));
    for (uint32_t f = 0; f < p->frames; ++f) {
        uint32_t count, items;
        if (!kwui_menu_frame(apt, bytes, p->table, f, &count, &items) || count != expected[f])
            return 0;
        if (count) {
            if (next && next != items)
                return 0;
            next = items + 4 * count;
        }
        uint32_t dest = already ? f : kw_ingame_destination(p, f, off);
        if (!p->counts[dest] && count)
            p->items[dest] = items;
        p->counts[dest] += count;
        for (uint32_t j = 0; j < count; ++j) {
            if (++work > 256)
                return 0;
            uint32_t item = kwui_menu_word(apt + items + 4 * j);
            if (!kwui_menu_range(bytes, item, 8))
                return 0;
            uint32_t type = kwui_menu_word(apt + item), value = kwui_menu_word(apt + item + 4);
            if (type == 1) {
                if (!kwui_menu_range(bytes, value, 2))
                    return 0;
            } else if (type == 2) {
                size_t k;
                for (k = 0; k < p->label_count; ++k)
                    if (kwui_menu_string(apt, bytes, value, p->labels[k].name))
                        break;
                if (!kwui_menu_range(bytes, item, 16) || kwui_menu_word(apt + item + 12) != f || k == p->label_count ||
                    f != (already ? kw_ingame_destination(p, p->labels[k].frame, off) : p->labels[k].frame) || (labels & (1U << k)))
                    return 0;
                labels |= 1U << k;
                p->label_items[k] = item;
            } else if (type == 3) {
                if (!kwui_menu_range(bytes, item, 64) || (value & ~255U) || ((value & 2U) && kwui_menu_word(apt + item + 12) >= 48) ||
                    ((value & 32U) && !kw_ingame_string_valid(apt, bytes, kwui_menu_word(apt + item + 52))))
                    return 0;
                if (value & 128U) {
                    uint32_t block = kwui_menu_word(apt + item + 60);
                    if (!kwui_menu_range(bytes, block, 8))
                        return 0;
                    uint32_t n = kwui_menu_word(apt + block), events = kwui_menu_word(apt + block + 4);
                    if (!n || n > 16 || !kwui_menu_range(bytes, events, n * 12))
                        return 0;
                    for (uint32_t e = 0; e < n; ++e)
                        if (!kwui_menu_range(bytes, kwui_menu_word(apt + events + 12 * e + 8), 2))
                            return 0;
                }
            } else if (type != 4 && type != 5)
                return 0;
        }
    }
    return labels == ((1U << p->label_count) - 1U);
}
/* 1 shortened, 2 already in the requested mode, 0 unfamiliar/truncated asset.
 * Identity is also filtered by the resource hook; no archive offsets are used. */
static inline int kw_ingame_menu_asset(unsigned char *data, size_t size, unsigned off)
{
    static const char magic[] = "Apt Data:74\x1a";
    if (!data || size < 76 || kwui_menu_word(data) || kwui_menu_word(data + 4) != 12)
        return 0;
    uint32_t bytes = kwui_menu_word(data + 8);
    if (bytes > size - 12 || bytes < 64 || memcmp(data + 12, magic, sizeof(magic) - 1))
        return 0;
    unsigned char *apt = data + 12;
    uint32_t root = 0, hits = 0;
    for (uint32_t at = 16; at <= bytes - 60; at += 4) {
        if (kwui_menu_word(apt + at) == 9 && kwui_menu_word(apt + at + 4) == 0x09876543U && kwui_menu_word(apt + at + 20) == 48 &&
            kwui_menu_word(apt + at + 28) == 1024 && kwui_menu_word(apt + at + 32) == 768 && kwui_menu_word(apt + at + 36) == 33) {
            root = at;
            ++hits;
        }
    }
    if (hits != 1)
        return 0;
    uint32_t chars = kwui_menu_word(apt + root + 24), n = kwui_menu_word(apt + root + 8), table = kwui_menu_word(apt + root + 12);
    if (!kwui_menu_range(bytes, chars, 48 * 4) || n > 107 || !kwui_menu_range(bytes, table, n * 8))
        return 0;
    uint32_t sprites[4] = {root, 0, 0, 0}, named[8] = {0};
    static const char *const names[] = {"Frame", "Options", "Save", "Load", "Restart", "ExitMission", "Resume", "backdrop"};
    for (uint32_t f = 0; f < n; ++f) {
        uint32_t count, items;
        if (!kwui_menu_frame(apt, bytes, table, f, &count, &items))
            return 0;
        for (uint32_t j = 0; j < count; ++j) {
            uint32_t item = kwui_menu_word(apt + items + 4 * j);
            if (!kwui_menu_range(bytes, item, 8))
                return 0;
            if (kwui_menu_word(apt + item) != 3 || !(kwui_menu_word(apt + item + 4) & 32U))
                continue;
            if (!kwui_menu_range(bytes, item, 64))
                return 0;
            uint32_t name = kwui_menu_word(apt + item + 52), ch = kwui_menu_word(apt + item + 12);
            for (unsigned k = 0; k < 8; ++k)
                if (kwui_menu_string(apt, bytes, name, names[k])) {
                    if (named[k] || ch >= 48)
                        return 0;
                    named[k] = kwui_menu_word(apt + chars + ch * 4);
                }
        }
    }
    for (unsigned k = 0; k < 8; ++k)
        if (!named[k])
            return 0;
    for (unsigned k = 2; k < 7; ++k)
        if (named[k] != named[1])
            return 0;
    sprites[1] = named[0];
    sprites[2] = named[1];
    sprites[3] = named[7];
    struct kw_ingame_clip p[4];
    memset(p, 0, sizeof(p));
#define KW_INGAME_CONTRACT(i, tag)                                                                                                                   \
    p[i].marks = kw_ingame_##tag##_marks;                                                                                                            \
    p[i].mark_count = sizeof(kw_ingame_##tag##_marks) / sizeof(kw_ingame_##tag##_marks[0]);                                                          \
    p[i].labels = kw_ingame_##tag##_labels;                                                                                                          \
    p[i].label_count = sizeof(kw_ingame_##tag##_labels) / sizeof(kw_ingame_##tag##_labels[0])
    KW_INGAME_CONTRACT(0, root);
    KW_INGAME_CONTRACT(1, frame);
    KW_INGAME_CONTRACT(2, button);
    KW_INGAME_CONTRACT(3, backdrop);
#undef KW_INGAME_CONTRACT
    int already = n != 59;
    static const unsigned span_counts[] = {6, 2, 11, 3};
    for (unsigned i = 0; i < 4; ++i) {
        p[i].sprite = sprites[i];
        p[i].spans = kw_ingame_spans[i];
        p[i].span_count = span_counts[i];
        if (!kwui_menu_range(bytes, sprites[i], i ? 16 : 60) || kwui_menu_word(apt + sprites[i]) != (i ? 5U : 9U) ||
            kwui_menu_word(apt + sprites[i] + 4) != 0x09876543U || !kw_ingame_plan(apt, bytes, &p[i], off, already))
            return 0;
        for (unsigned j = 0; j < i; ++j)
            if (sprites[i] == sprites[j])
                return 0;
    }
    /* Cross-clip dependency, not merely per-clip control preservation. Native
     * OnClosed resumes the match unless Options/Save/Load ran first. Allow
     * two ticks for parent/child advancement order and callback dispatch. */
    uint32_t down = kw_ingame_destination(&p[2], 49, off), guard = kw_ingame_destination(&p[2], 53, off);
    uint32_t execute = kw_ingame_destination(&p[2], 58, off);
    uint32_t frame_close = kw_ingame_destination(&p[1], 38, off) - kw_ingame_destination(&p[1], 27, off);
    uint32_t root_close = kw_ingame_destination(&p[0], 58, off) - kw_ingame_destination(&p[0], 46, off);
    if (guard <= down || execute <= guard || frame_close < execute - down + 2 || root_close < execute - down + 2)
        return 0;
    /* Protect all data referenced by a table from overlapping our writes. */
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j) {
            uint32_t capacity = p[i].spans[p[i].span_count - 1].end * 8;
            if (kw_ingame_overlap(p[i].table, capacity, p[j].sprite, 16) ||
                (i != j && kw_ingame_overlap(p[i].table, capacity, p[j].table, p[j].spans[p[j].span_count - 1].end * 8)))
                return 0;
            for (uint32_t f = 0; f < p[j].frames; ++f) {
                uint32_t count = kwui_menu_word(apt + p[j].table + 8 * f), items = kwui_menu_word(apt + p[j].table + 8 * f + 4);
                if (count && kw_ingame_overlap(p[i].table, capacity, items, count * 4))
                    return 0;
                for (uint32_t k = 0; k < count; ++k) {
                    uint32_t item = kwui_menu_word(apt + items + 4 * k), type = kwui_menu_word(apt + item),
                             length = type == 3   ? 64
                                      : type == 2 ? 16
                                                  : 8;
                    if (kw_ingame_overlap(p[i].table, capacity, item, length))
                        return 0;
                }
            }
        }
    if (already)
        return 2;
    for (unsigned i = 0; i < 4; ++i) {
        for (uint32_t f = 0; f < p[i].target; ++f) {
            kwui_menu_put(apt + p[i].table + 8 * f, p[i].counts[f]);
            kwui_menu_put(apt + p[i].table + 8 * f + 4, p[i].items[f]);
        }
        kwui_menu_put(apt + p[i].sprite + 8, p[i].target);
        for (size_t j = 0; j < p[i].label_count; ++j)
            kwui_menu_put(apt + p[i].label_items[j] + 12, kw_ingame_destination(&p[i], p[i].labels[j].frame, off));
    }
    return 1;
}
#endif
