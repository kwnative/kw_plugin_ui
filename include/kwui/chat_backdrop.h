/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KWUI_CHAT_BACKDROP_H
#define KWUI_CHAT_BACKDROP_H

#include <stddef.h>
#include <stdint.h>

#define KWUI_CHAT_BACKDROP_ROWS 6U
#define KWUI_CHAT_BACKDROP_STRIPS 192U
#define KWUI_CHAT_BACKDROP_PADDING 3
#define KWUI_CHAT_BACKDROP_MAX_DIMENSION 32768

struct kwui_chat_backdrop_row {
    int32_t x, y, width, height, pitch;
    uint32_t alpha;
};

struct kwui_chat_backdrop_rect {
    int32_t x, y, width, height;
    uint32_t color;
};

/* Text-width strips with modest horizontal padding and the native shadow pixel.
 * The caller supplies each row's actual draw X (including right-edge anchoring).
 * Inset one pixel vertically at each edge to leave a two-pixel divider at
 * normal fonts.
 * Wrapped text gets a strip per physical line. Duplicate strips from overlapping
 * stock messages union their text bounds and use max alpha so black is never
 * blended twice at the same row.
 * The caller must discard the layout if this returns zero. No heap work. */
static inline size_t kwui_chat_backdrop_layout(const struct kwui_chat_backdrop_row *rows, size_t count, int32_t viewport_width,
                                               int32_t viewport_height, uint32_t opacity, struct kwui_chat_backdrop_rect *output)
{
    size_t used = 0, i;
    if (!rows || !output || count > KWUI_CHAT_BACKDROP_ROWS || opacity == 0 || opacity > 100 || viewport_width <= 0 || viewport_height <= 0 ||
        viewport_width > KWUI_CHAT_BACKDROP_MAX_DIMENSION || viewport_height > KWUI_CHAT_BACKDROP_MAX_DIMENSION)
        return 0;
    for (i = 0; i < count; ++i) {
        uint32_t alpha, lines, line;
        int32_t inset;
        int64_t text_right = (int64_t)rows[i].x + rows[i].width;
        int64_t left, right;
        if (rows[i].width <= 0 || rows[i].height <= 0 || rows[i].pitch <= 0 || rows[i].width > KWUI_CHAT_BACKDROP_MAX_DIMENSION ||
            rows[i].height > KWUI_CHAT_BACKDROP_MAX_DIMENSION || rows[i].pitch > KWUI_CHAT_BACKDROP_MAX_DIMENSION || rows[i].alpha == 0 ||
            rows[i].alpha > 255 || text_right <= 0 || rows[i].x >= viewport_width)
            continue;
        left = (int64_t)rows[i].x - KWUI_CHAT_BACKDROP_PADDING;
        right = text_right + 1 + KWUI_CHAT_BACKDROP_PADDING;
        if (left < 0)
            left = 0;
        if (right > viewport_width)
            right = viewport_width;
        alpha = (rows[i].alpha * opacity + 50U) / 100U;
        if (!alpha)
            continue;
        lines = (uint32_t)(rows[i].height + rows[i].pitch - 1) / (uint32_t)rows[i].pitch;
        if (lines > KWUI_CHAT_BACKDROP_STRIPS)
            return 0;
        inset = rows[i].pitch > 2 ? 1 : 0;
        for (line = 0; line < lines; ++line) {
            int64_t top = (int64_t)rows[i].y + line * (int64_t)rows[i].pitch + inset;
            int64_t bottom = top + rows[i].pitch - inset * 2;
            size_t j;
            if (top < 0)
                top = 0;
            if (bottom > viewport_height)
                bottom = viewport_height;
            if (bottom <= top)
                continue;
            for (j = 0; j < used; ++j) {
                if (output[j].y == top && output[j].height == bottom - top)
                    break;
            }
            if (j < used) {
                int64_t union_right = (int64_t)output[j].x + output[j].width;
                if (right > union_right)
                    union_right = right;
                if (left < output[j].x)
                    output[j].x = (int32_t)left;
                output[j].width = (int32_t)(union_right - output[j].x);
                if (output[j].color < (alpha << 24U))
                    output[j].color = alpha << 24U;
                continue;
            }
            if (used == KWUI_CHAT_BACKDROP_STRIPS)
                return 0;
            output[used].x = (int32_t)left;
            output[used].y = (int32_t)top;
            output[used].width = (int32_t)(right - left);
            output[used].height = (int32_t)(bottom - top);
            output[used].color = alpha << 24U;
            ++used;
        }
    }
    return used;
}

#endif
