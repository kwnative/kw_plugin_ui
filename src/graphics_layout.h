/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KW_UI_GRAPHICS_LAYOUT_H
#define KW_UI_GRAPHICS_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
struct kw_graphics_signature {
    const char *name, *text;
};
#include "graphics_patterns.inc"
enum { KG_COUNT, KG_MAP, KG_GET, KG_SET, KG_LABEL, KG_HUD, KG_RESET, KG_VIDEO, KG_ADAPTER, KG_SELECTION, KG_DEVICE, KG_VIDEO_FLAG, KG_SITES };
struct kw_graphics_layout {
    uintptr_t site[KG_SITES], d3d, device, parameters, windowed, adapter, selection;
    uintptr_t ascii_ctor, ascii_dtor, wide_format, pref_get, pref_set, option_vtable, user_vtable;
    uintptr_t hud_original, label_owner, label_set, wide_dtor;
};
struct kw_graphics_pe {
    const unsigned char *b;
    size_t size;
    uint32_t base, table;
    uint16_t sections;
};
static inline uint32_t kg_u32(const void *p)
{
    uint32_t n;
    memcpy(&n, p, 4);
    return n;
}
static inline uint16_t kg_u16(const void *p)
{
    uint16_t n;
    memcpy(&n, p, 2);
    return n;
}
static inline int kg_range(const struct kw_graphics_pe *p, uintptr_t a, size_t n)
{
    return a >= (uintptr_t)p->b && a - (uintptr_t)p->b <= p->size && n <= p->size - (a - (uintptr_t)p->b);
}
static inline int kg_code(const struct kw_graphics_pe *p, uintptr_t a, size_t n)
{
    if (!kg_range(p, a, n))
        return 0;
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), bytes = kg_u32(s + 8), raw = kg_u32(s + 16);
        if (raw > bytes)
            bytes = raw;
        if ((kg_u32(s + 36) & 0x20000000U) && a >= (uintptr_t)p->b + at && a - ((uintptr_t)p->b + at) <= bytes &&
            n <= bytes - (a - ((uintptr_t)p->b + at)))
            return 1;
    }
    return 0;
}
static inline int kg_writable(const struct kw_graphics_pe *p, uintptr_t a, size_t n)
{
    if (!kg_range(p, a, n))
        return 0;
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), bytes = kg_u32(s + 8), raw = kg_u32(s + 16);
        if (raw > bytes)
            bytes = raw;
        if ((kg_u32(s + 36) & 0xe0000000U) == 0xc0000000U && a >= (uintptr_t)p->b + at && a - ((uintptr_t)p->b + at) <= bytes &&
            n <= bytes - (a - ((uintptr_t)p->b + at)))
            return 1;
    }
    return 0;
}
static inline uintptr_t kg_va(const struct kw_graphics_pe *p, uint32_t va)
{
    return va >= p->base && va - p->base < p->size ? (uintptr_t)p->b + va - p->base : 0;
}
static inline uintptr_t kg_rel(const struct kw_graphics_pe *p, uintptr_t call)
{
    if (!kg_code(p, call, 5) || *(const unsigned char *)call != 0xe8)
        return 0;
    uintptr_t target = (uintptr_t)((uint32_t)call + 5U + kg_u32((const void *)(call + 1)));
    return kg_code(p, target, 1) ? target : 0;
}
static inline unsigned kg_hex(char c)
{
    return c >= '0' && c <= '9' ? (unsigned)(c - '0') : (unsigned)(c - 'a' + 10);
}
static inline uintptr_t kg_find(const struct kw_graphics_pe *p, const char *text)
{
    unsigned char bytes[160], mask[160];
    unsigned n = 0;
    uintptr_t result = 0;
    while (*text) {
        if (*text == ' ') {
            ++text;
            continue;
        }
        if (n == sizeof(bytes) || !text[1])
            return 0;
        mask[n] = *text == '?' ? 0 : 255;
        bytes[n] = mask[n] ? (unsigned char)(kg_hex(text[0]) * 16 + kg_hex(text[1])) : 0;
        ++n;
        text += 2;
    }
    if (!n)
        return 0;
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), length = kg_u32(s + 8), raw = kg_u32(s + 16);
        if ((kg_u32(s + 36) & 0x60000000U) != 0x60000000U)
            continue;
        if (raw > length)
            length = raw;
        if (at > p->size || length > p->size - at)
            return 0;
        if (length < n)
            continue;
        const unsigned char *first = p->b + at, *cursor = first, *end = first + length - n + 1;
        while (cursor < end) {
            cursor = memchr(cursor, bytes[0], (size_t)(end - cursor));
            if (!cursor)
                break;
            unsigned j = 0;
            while (j < n && (!mask[j] || cursor[j] == bytes[j]))
                ++j;
            if (j == n) {
                if (result)
                    return 0;
                result = (uintptr_t)cursor;
            }
            ++cursor;
        }
    }
    return result;
}
static inline uintptr_t kg_rtti(const struct kw_graphics_pe *p, const char *name)
{
    uintptr_t descriptor = 0, locator = 0, vtable = 0;
    size_t n = strlen(name) + 1;
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), length = kg_u32(s + 8), raw = kg_u32(s + 16);
        if (raw > length)
            length = raw;
        if (!(kg_u32(s + 36) & 0x40000000U) || (kg_u32(s + 36) & 0x20000000U) || length < n + 8)
            continue;
        if (at > p->size || length > p->size - at)
            return 0;
        for (uint32_t j = 8; j + n <= length; ++j)
            if (p->b[at + j] == '.' && !memcmp(p->b + at + j, name, n)) {
                if (descriptor)
                    return 0;
                descriptor = (uintptr_t)p->b + at + j - 8;
            }
    }
    if (!descriptor)
        return 0;
    uint32_t descriptor_va = p->base + (uint32_t)(descriptor - (uintptr_t)p->b);
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), length = kg_u32(s + 8), raw = kg_u32(s + 16);
        if (raw > length)
            length = raw;
        if ((kg_u32(s + 36) & 0xe0000000U) != 0x40000000U)
            continue;
        for (uint32_t j = 0; j + 20 <= length; j += 4) {
            const unsigned char *v = p->b + at + j;
            if (!kg_u32(v) && !kg_u32(v + 4) && !kg_u32(v + 8) && kg_u32(v + 12) == descriptor_va && kg_va(p, kg_u32(v + 16))) {
                if (locator)
                    return 0;
                locator = (uintptr_t)v;
            }
        }
    }
    if (!locator)
        return 0;
    uint32_t locator_va = p->base + (uint32_t)(locator - (uintptr_t)p->b);
    for (unsigned i = 0; i < p->sections; ++i) {
        const unsigned char *s = p->b + p->table + 40 * i;
        uint32_t at = kg_u32(s + 12), length = kg_u32(s + 8), raw = kg_u32(s + 16);
        if (raw > length)
            length = raw;
        if ((kg_u32(s + 36) & 0xe0000000U) != 0x40000000U)
            continue;
        for (uint32_t j = 0; j + 8 <= length; j += 4)
            if (kg_u32(p->b + at + j) == locator_va && kg_code(p, kg_va(p, kg_u32(p->b + at + j + 4)), 1)) {
                if (vtable)
                    return 0;
                vtable = (uintptr_t)p->b + at + j + 4;
            }
    }
    return vtable;
}
static inline int kw_graphics_discover(const void *image, size_t size, struct kw_graphics_layout *out)
{
    struct kw_graphics_layout l;
    struct kw_graphics_pe p = {(const unsigned char *)image, size, 0, 0, 0};
    memset(out, 0, sizeof(*out));
    memset(&l, 0, sizeof(l));
    if (!image || size < 256 || kg_u16(image) != 0x5a4d)
        return 0;
    uint32_t nt = kg_u32(p.b + 60);
    if (nt > size || size - nt < 120 || kg_u32(p.b + nt) != 0x4550 || kg_u16(p.b + nt + 4) != 0x14c || kg_u16(p.b + nt + 24) != 0x10b)
        return 0;
    uint16_t optional = kg_u16(p.b + nt + 20);
    p.sections = kg_u16(p.b + nt + 6);
    p.base = kg_u32(p.b + nt + 52);
    p.table = nt + 24 + optional;
    if (optional < 96 || !p.sections || p.sections > 96 || (uint64_t)p.table + 40U * p.sections > size || kg_u32(p.b + nt + 80) > size)
        return 0;
    for (unsigned i = 0; i < KG_SITES; ++i)
        if (!(l.site[i] = kg_find(&p, kw_graphics_signatures[i].text)))
            return 0;
    l.d3d = kg_va(&p, kg_u32((void *)(l.site[KG_COUNT] + 5)));
    l.windowed = kg_va(&p, kg_u32((void *)(l.site[KG_COUNT] + 22)));
    l.adapter = kg_va(&p, kg_u32((void *)(l.site[KG_ADAPTER] + 27)));
    l.device = kg_va(&p, kg_u32((void *)(l.site[KG_DEVICE] + 8)));
    l.parameters = kg_va(&p, kg_u32((void *)(l.site[KG_DEVICE] + 14)));
    l.selection = kg_va(&p, kg_u32((void *)(l.site[KG_SELECTION] + 77)));
    if (!kg_writable(&p, l.d3d, 4) || !kg_writable(&p, l.windowed, 1) || !kg_writable(&p, l.adapter, 4) || !kg_writable(&p, l.device, 4) ||
        !kg_writable(&p, l.parameters, 56) || !kg_writable(&p, l.selection, 4) ||
        kg_va(&p, kg_u32((void *)(l.site[KG_COUNT] + 31))) != l.parameters + 8 || kg_va(&p, kg_u32((void *)(l.site[KG_DEVICE] + 1))) != l.d3d ||
        kg_va(&p, kg_u32((void *)(l.site[KG_ADAPTER] + 4))) != l.d3d)
        return 0;
    uintptr_t key = kg_rel(&p, l.site[KG_GET] + 34);
    if (!kg_code(&p, key, 27) || memcmp((void *)key, "\x8b\x44\x24\x08\x8b\x4c\x24\x04\x6b\xc0\x0c", 11) ||
        (*(const unsigned char *)(key + 11) != 0xff || (*(const unsigned char *)(key + 12) != 0xb0 && *(const unsigned char *)(key + 12) != 0x25)) ||
        memcmp((void *)(key + 22), "\x8b\x44\x24\x04\xc3", 5))
        return 0;
    l.ascii_ctor = kg_rel(&p, key + 17);
    l.ascii_dtor = kg_rel(&p, l.site[KG_GET] + 51);
    l.wide_format = kg_rel(&p, l.site[KG_LABEL] + 8);
    l.hud_original = kg_rel(&p, l.site[KG_HUD] + 7);
    l.label_owner = kg_va(&p, kg_u32((void *)(l.site[KG_LABEL] + 15)));
    l.label_set = kg_rel(&p, l.site[KG_LABEL] + 31);
    l.wide_dtor = kg_rel(&p, l.site[KG_LABEL] + 39);
    l.option_vtable = kg_rtti(&p, ".?AVOptionPreferences@@");
    l.user_vtable = kg_rtti(&p, ".?AVUserPreferences@@");
    if (!l.ascii_ctor || !l.ascii_dtor || !l.wide_format || !l.hud_original || !l.label_set || !l.wide_dtor || !kg_writable(&p, l.label_owner, 4) ||
        !kg_code(&p, l.site[KG_LABEL] + 36, 8) || memcmp((void *)(l.site[KG_LABEL] + 36), "\x8d\x4d\xe0", 3) || !kg_range(&p, l.option_vtable, 52) ||
        !kg_range(&p, l.user_vtable, 52))
        return 0;
    l.pref_get = kg_va(&p, kg_u32((void *)(l.option_vtable + 0x18)));
    l.pref_set = kg_va(&p, kg_u32((void *)(l.option_vtable + 0x2c)));
    if (!kg_code(&p, l.pref_get, 12) || !kg_code(&p, l.pref_set, 12) || kg_va(&p, kg_u32((void *)(l.user_vtable + 0x18))) != l.pref_get ||
        kg_va(&p, kg_u32((void *)(l.user_vtable + 0x2c))) != l.pref_set)
        return 0;
    *out = l;
    return 1;
}
#endif
