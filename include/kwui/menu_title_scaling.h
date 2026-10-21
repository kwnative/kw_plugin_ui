#ifndef KWUI_MENU_TITLE_SCALING_H
#define KWUI_MENU_TITLE_SCALING_H
#include <stdint.h>

/* Stock ShellContent TextStr: RussellSquare, red, centered, 36 points,
 * wide title box. Narrow matching excludes profile/chat and red ruler numbers. */
struct kwui_menu_title_metrics {
    float left, top, right, bottom, font_height;
    uint32_t color, alignment;
};
static inline int kwui_is_shared_menu_title(const struct kwui_menu_title_metrics *m)
{
    float width = m->right - m->left, height = m->bottom - m->top;
    return (m->color & 0xffffffU) == 0xff0000U && m->alignment == 2U && m->font_height >= 35.99f && m->font_height <= 36.01f && width >= 600.0f &&
           width <= 900.0f && height > 30.0f && height <= 128.0f;
}
#endif
