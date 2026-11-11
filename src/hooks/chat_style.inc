/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Match-only font scaling and outline. Shared native draw/string routines stay
 * stock for lobby, HUD, and other users. Included before the match draw hook. */
#include <kwengine/engine_address.h>
#include <kwui/chat_style.h>
static int chat_backdrop_install(void);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
typedef void *(KWUI_THISCALL *chat_style_font_fn)(void *, float);
typedef void(KWUI_THISCALL *chat_style_text_fn)(void *, int32_t, int32_t, int32_t, int32_t);
typedef void(KWUI_THISCALL *chat_style_colors_fn)(void *, uint32_t, uint32_t);
static chat_style_font_fn chat_style_original_font;
static chat_style_text_fn chat_style_original_text;
/* Rendering stays disabled until startup configuration is applied. */
static volatile LONG chat_style_scale = 100, chat_style_outline;
static volatile LONG chat_style_light_rgb = KWUI_CHAT_OUTLINE_LIGHT_DEFAULT_RGB;
static volatile LONG chat_style_dark_rgb = KWUI_CHAT_OUTLINE_DARK_DEFAULT_RGB;
/* Owned only by the current synchronous match draw; no retained UI objects. */
static const unsigned char *chat_style_active_messages;

static void *KWUI_FASTCALL chat_style_font_hook(void *self, void *unused, float scale)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    (void)unused;
    /* This return site is the MessageDisplay insertion's font assignment. */
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) == 0U && caller == (uintptr_t)ui_hook_status.host_image_base + kwengine_rva(0x3e7567U))
        scale *= (float)chat_style_scale / 100.0f;
    return chat_style_original_font(self, scale);
}

static void KWUI_FASTCALL chat_style_text_hook(void *self, void *unused, int32_t x, int32_t y, int32_t x_drop, int32_t y_drop)
{
    const unsigned char *messages = chat_style_active_messages;
    uintptr_t image = ui_hook_status.host_image_base, address;
    chat_style_colors_fn colors = NULL;
    uint32_t foreground, shadow, outline;
    unsigned i;
    int dx, dy;
    (void)unused;
    if (!messages || !chat_style_outline || ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U)
        goto stock;
    for (i = 0; i < KWUI_CHAT_BACKDROP_ROWS; ++i)
        if (*(void **)(messages + 0x80U + i * 0x10U) == self)
            break;
    if (i == KWUI_CHAT_BACKDROP_ROWS || !readable_memory(self, 0x1e4U) || *(uintptr_t *)self != image + kwengine_rva(0x6a210cU))
        goto stock;
    /* Native setter writes four uniform foreground/shadow ARGB corners. */
    CopyMemory(&foreground, (unsigned char *)self + 0x1b8U, sizeof(foreground));
    CopyMemory(&shadow, (unsigned char *)self + 0x1c8U, sizeof(shadow));
    if (!(foreground >> 24U))
        goto stock;
    outline = kwui_chat_outline_color(foreground, (uint32_t)chat_style_light_rgb, (uint32_t)chat_style_dark_rgb);
    address = image + kwengine_rva(0x3df668U);
    CopyMemory(&colors, &address, sizeof(colors));
    colors(self, outline, 0);
    /* Offset copies form the surrounding one-pixel contour. Native text is
     * rendered last at its original position and keeps the player's raw RGB. */
    for (dy = -1; dy <= 1; ++dy)
        for (dx = -1; dx <= 1; ++dx)
            if (dx || dy)
                chat_style_original_text(self, x + dx, y + dy, 0, 0);
    colors(self, foreground, shadow);
    chat_style_original_text(self, x, y, 0, 0); /* No black shadow over the contour. */
    chat_backdrop_note(23, foreground, outline, (uint32_t)x, (uint32_t)y);
    return;
stock:
    chat_style_original_text(self, x, y, x_drop, y_drop);
}

/* Keep the stack's bottom edge where stock put it when a larger/smaller font
 * changes pitch. Native draw still computes every individual message origin. */
static const float *chat_style_position(void *self, const float *position, float adjusted[2])
{
    uintptr_t image = ui_hook_status.host_image_base, settings;
    const unsigned char *messages = self;
    unsigned i;
    int32_t difference = 0, stock_pitch;
    float native_scale;
    void *font;
    if (chat_style_scale == 100 || ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U || !readable_memory(messages, 0xe0U) ||
        !messages[0x78] || !readable_memory(position, 2U * sizeof(float)) || !(position[0] >= -65536.0f && position[0] <= 65536.0f) ||
        !(position[1] >= -65536.0f && position[1] <= 65536.0f))
        return position;
    CopyMemory(&settings, (void *)(image + kwengine_rva(0x7f46bcU)), sizeof(settings));
    if (!readable_memory((void *)settings, 0x18U))
        return position;
    CopyMemory(&native_scale, messages + 0xdcU, sizeof(native_scale));
    if (!(native_scale > 0.0f && native_scale <= 16.0f))
        return position;
    font = chat_style_original_font((void *)settings, native_scale);
    if (!readable_memory(font, 0x14U))
        return position;
    CopyMemory(&stock_pitch, (unsigned char *)font + 0x10U, sizeof(stock_pitch));
    if (stock_pitch <= 0 || stock_pitch > 256)
        return position;
    for (i = 0; i < KWUI_CHAT_BACKDROP_ROWS; ++i) {
        const unsigned char *string = *(const unsigned char **)(messages + 0x80U + i * 0x10U);
        uintptr_t row_font;
        int32_t pitch;
        if (!string)
            continue;
        if (!readable_memory(string, 0x1e4U) || *(uintptr_t *)string != image + kwengine_rva(0x6a210cU))
            return position;
        CopyMemory(&row_font, string + 8U, sizeof(row_font));
        if (!readable_memory((void *)row_font, 0x14U))
            return position;
        CopyMemory(&pitch, (void *)(row_font + 0x10U), sizeof(pitch));
        if (pitch <= 0 || pitch > 256)
            return position;
        difference += pitch - stock_pitch;
    }
    adjusted[0] = position[0];
    adjusted[1] = position[1] - (float)difference;
    chat_backdrop_note(24, (uint32_t)chat_style_scale, (uint32_t)stock_pitch, (uint32_t)difference, 0);
    return adjusted;
}

static int chat_style_install(void)
{
    static const unsigned char font_signature[] = {0x8b, 0x41, 0x0c, 0x85, 0xc0, 0x56};
    static const unsigned char text_signature[] = {0x55, 0x8b, 0xec, 0x51, 0x51, 0x56};
    static const unsigned char insertion_call[] = {0xe8, 0x25, 0xe4, 0xff, 0xff};
    static const unsigned char color_signature[] = {0x8b, 0x54, 0x24, 0x04, 0x56, 0x6a, 0x04};
    unsigned char *image = (unsigned char *)(uintptr_t)ui_hook_status.host_image_base;
    void *trampoline;
    uintptr_t address;
    typedef void *(KWUI_FASTCALL * font_hook_fn)(void *, void *, float);
    typedef void(KWUI_FASTCALL * text_hook_fn)(void *, void *, int32_t, int32_t, int32_t, int32_t);
    font_hook_fn font_hook = chat_style_font_hook;
    text_hook_fn text_hook = chat_style_text_hook;
    if (chat_style_original_font && chat_style_original_text)
        return 1;
    if (!image || ui_hook_status.host_image_size < 0x7f46c0U || memcmp(image + kwengine_rva(0x3e598cU), font_signature, sizeof(font_signature)) ||
        memcmp(image + kwengine_rva(0x3dfb1dU), text_signature, sizeof(text_signature)) ||
        memcmp(image + kwengine_rva(0x3df668U), color_signature, sizeof(color_signature)) ||
        memcmp(image + kwengine_rva(0x3e7562U), insertion_call, sizeof(insertion_call)) ||
        *(uintptr_t *)(image + kwengine_rva(0x6a2124U)) != (uintptr_t)image + kwengine_rva(0x3dfdb9U) ||
        *(uintptr_t *)(image + kwengine_rva(0x6a2134U)) != (uintptr_t)image + kwengine_rva(0x3df668U) ||
        *(uintptr_t *)(image + kwengine_rva(0x6a214cU)) != (uintptr_t)image + kwengine_rva(0x3dfb1dU) || !chat_backdrop_install())
        return 0;
    CopyMemory(&address, &font_hook, sizeof(address));
    trampoline = install_function_trampoline(image + kwengine_rva(0x3e598cU), 6U, address);
    if (!trampoline)
        return 0;
    CopyMemory(&chat_style_original_font, &trampoline, sizeof(trampoline));
    CopyMemory(&address, &text_hook, sizeof(address));
    trampoline = install_function_trampoline(image + kwengine_rva(0x3dfb1dU), 6U, address);
    if (!trampoline)
        return 0;
    CopyMemory(&chat_style_original_text, &trampoline, sizeof(trampoline));
    return 1;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

uint32_t KWENGINE_CALL KwuiConfigureChatStyle(uint32_t outline, uint32_t scale_percent)
{
    if (outline > 1U || scale_percent < 50U || scale_percent > 200U)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U || (!outline && scale_percent == 100U)) {
        InterlockedExchange(&chat_style_outline, 0);
        InterlockedExchange(&chat_style_scale, 100);
        return KWENGINE_NATIVE_OK;
    }
    if (InterlockedCompareExchange(&ui_initialization_state, 0, 0) != 2 || ui_hook_status.result != KWENGINE_NATIVE_OK ||
        (ui_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && ui_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!chat_style_install())
        return KWENGINE_NATIVE_ERROR_HOOK;
    InterlockedExchange(&chat_style_scale, (LONG)scale_percent);
    InterlockedExchange(&chat_style_outline, (LONG)outline);
    return KWENGINE_NATIVE_OK;
}

/* Startup-only color state: no hooks, font resources or UI reads. */
uint32_t KWENGINE_CALL KwuiConfigureChatOutlineColors(uint32_t light_rgb, uint32_t dark_rgb)
{
    if (light_rgb > 0xffffffU || dark_rgb > 0xffffffU)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U)
        return KWENGINE_NATIVE_OK;
    if (InterlockedCompareExchange(&ui_initialization_state, 0, 0) != 2 || ui_hook_status.result != KWENGINE_NATIVE_OK ||
        (ui_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && ui_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    InterlockedExchange(&chat_style_light_rgb, (LONG)light_rgb);
    InterlockedExchange(&chat_style_dark_rgb, (LONG)dark_rgb);
    return KWENGINE_NATIVE_OK;
}
