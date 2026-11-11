/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Native list coordinates occupy four-byte aligned slots. Retail uses only
 * the low half of viewport_height (+3c) and scroll_position (+44); the high
 * halves are padding, before the existing fields at +40/+48. Widen every
 * producer, consumer and intermediate together; preserve the object ABI. */
#ifndef KWUI_UI_LIST_SCROLL_H
#define KWUI_UI_LIST_SCROLL_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
struct kw_list_pattern {
    const char *label;
    const unsigned char *bytes, *mask, *before, *after;
    uint16_t size, offset, length, parent;
};
#include "list_scroll_patterns.inc"
#define KW_LIST_PATCH_COUNT (sizeof(kw_list_patterns) / sizeof(kw_list_patterns[0]))
struct kw_list_scroll_plan {
    uintptr_t sites[KW_LIST_PATCH_COUNT];
    unsigned count, missing;
};
static uint16_t kw_list_u16(const unsigned char *p)
{
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}
static uint32_t kw_list_u32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
static unsigned kw_list_bucket(uint32_t key)
{
    return ((key ^ (key >> 16)) * 2654435761U) >> 20;
}
static int kw_list_scroll_plan_build(const void *image, size_t size, struct kw_list_scroll_plan *plan)
{
    const unsigned char *b = image;
    uint32_t nt, table;
    uint16_t sections, optional;
    uint16_t heads[4096] = {0}, next[KW_LIST_PATCH_COUNT] = {0}, anchors[KW_LIST_PATCH_COUNT] = {0};
    uint32_t bloom[2048] = {0}, keys[KW_LIST_PATCH_COUNT] = {0};
    memset(plan, 0, sizeof(*plan));
    if (!b || size < 64 || kw_list_u16(b) != 0x5a4d)
        return 0;
    nt = kw_list_u32(b + 60);
    if (nt > size || size - nt < 24 || kw_list_u32(b + nt) != 0x4550 || kw_list_u16(b + nt + 4) != 0x14c)
        return 0;
    sections = kw_list_u16(b + nt + 6);
    optional = kw_list_u16(b + nt + 20);
    if (optional < 96 || optional > size - nt - 24 || kw_list_u16(b + nt + 24) != 0x10b || sections > 96)
        return 0;
    table = nt + 24 + optional;
    if ((uint64_t)table + sections * 40U > size || kw_list_u32(b + nt + 80) > size)
        return 0;
    /* Index exact four-byte anchors, then walk executable sections once. A
     * separate whole-image scan per instruction adds seconds on protected
     * images with large code sections. Full masks still prove each match. */
    for (unsigned n = 0; n < KW_LIST_PATCH_COUNT; ++n) {
        const struct kw_list_pattern *p = &kw_list_patterns[n];
        if (p->offset + p->length > p->size)
            return 0;
        if (p->parent)
            continue;
        unsigned anchor = 0;
        for (; anchor + 4 <= p->size; ++anchor)
            if (kw_list_u32(p->mask + anchor) == UINT32_MAX)
                break;
        if (anchor + 4 > p->size)
            return 0;
        anchors[n] = (uint16_t)anchor;
        keys[n] = kw_list_u32(p->bytes + anchor);
        unsigned low = keys[n] & 65535U, bucket = kw_list_bucket(keys[n]);
        bloom[low >> 5] |= 1U << (low & 31U);
        next[n] = heads[bucket];
        heads[bucket] = (uint16_t)(n + 1);
    }
    for (unsigned s = 0; s < sections; ++s) {
        const unsigned char *section = b + table + s * 40U;
        uint32_t at = kw_list_u32(section + 12), length = kw_list_u32(section + 8), raw = kw_list_u32(section + 16);
        if ((kw_list_u32(section + 36) & 0x60000000U) != 0x60000000U)
            continue;
        if (raw > length)
            length = raw;
        if (at > size || length > size - at)
            return 0;
        if (length < 4)
            continue;
        for (size_t cursor = 0; cursor <= length - 4; ++cursor) {
            unsigned low = kw_list_u16(b + at + cursor);
            if (!(bloom[low >> 5] & (1U << (low & 31U))))
                continue;
            uint32_t key = kw_list_u32(b + at + cursor);
            for (unsigned link = heads[kw_list_bucket(key)]; link; link = next[link - 1]) {
                unsigned n = link - 1;
                const struct kw_list_pattern *p = &kw_list_patterns[n];
                if (keys[n] != key || cursor < anchors[n])
                    continue;
                size_t start = cursor - anchors[n];
                if (p->size > length - start)
                    continue;
                unsigned i = 0;
                for (; i < p->size; ++i)
                    if ((b[at + start + i] & p->mask[i]) != (p->bytes[i] & p->mask[i]))
                        break;
                if (i != p->size)
                    continue;
                if (plan->sites[n]) {
                    plan->missing = n + 1;
                    return 0;
                }
                plan->sites[n] = (uintptr_t)(b + at + start + p->offset);
                ++plan->count;
            }
        }
    }
    for (unsigned n = 0; n < KW_LIST_PATCH_COUNT; ++n) {
        const struct kw_list_pattern *p = &kw_list_patterns[n];
        uintptr_t found = plan->sites[n];
        if (p->parent) {
            /* Protection can replace push-immediate with push-[constant]. Find
             * the subsequent store by opcode, not a fixed instruction offset. */
            if (p->parent > n)
                return 0;
            uintptr_t parent = plan->sites[p->parent - 1] + kw_list_patterns[p->parent - 1].length;
            if (parent < (uintptr_t)b || parent - (uintptr_t)b > size || size - (parent - (uintptr_t)b) < 32)
                return 0;
            for (unsigned j = 0; j + p->size <= 32; ++j) {
                if (!memcmp((const void *)(parent + j), p->bytes, p->size)) {
                    if (found) {
                        plan->missing = n + 1;
                        return 0;
                    }
                    found = parent + j + p->offset;
                }
            }
            if (found)
                ++plan->count;
        }
        if (!found || memcmp((const void *)found, p->before, p->length)) {
            plan->missing = n + 1;
            return 0;
        }
        for (unsigned i = 0; i < n; ++i) {
            uintptr_t a = plan->sites[i];
            unsigned length = kw_list_patterns[i].length;
            if (found < a + length && a < found + p->length) {
                plan->missing = n + 1;
                return 0;
            }
        }
        plan->sites[n] = found;
    }
    return plan->count == KW_LIST_PATCH_COUNT;
}
#endif
