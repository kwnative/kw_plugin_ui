/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Stock KW 1.02 MessageDisplay composition, immediately before stock text.
 * Included after the bridge's validated trampoline and memory helpers.
 * Native addresses/ABIs and reproduction: MATCH_CHAT_BACKDROP_1_02.md. */
#include <kwengine/engine_address.h>
#include <kwui/chat_backdrop.h>
#include <kwui/chat_backdrop_api.h>
#include <kwengine/detail/native_log.h>

static volatile LONG chat_backdrop_noted;
static void chat_backdrop_note(unsigned reason, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    const LONG bit = (LONG)(1U << reason);
    if ((InterlockedOr(&chat_backdrop_noted, bit) & bit) != 0)
        return;
#ifdef _WIN32
#ifdef KWUI_PLUGIN_RUNTIME
    kwengine_log_event(ui_log_module(), "draw-status",
#else
    kwengine_log_event(NULL, "draw-status",
#endif
                       reason, a, b, c, d);
#else
    (void)a;
    (void)b;
    (void)c;
    (void)d;
#endif
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
typedef void(KWUI_THISCALL *chat_backdrop_stock_draw_fn)(void *, const float *);
/* KW's rectangle arguments are FLOATS, unlike the older Generals source. */
typedef void(KWUI_THISCALL *chat_backdrop_fill_fn)(void *, float, float, float, float, uint32_t);
typedef void(KWUI_THISCALL *chat_backdrop_clip_fn)(void *, uint32_t);
typedef void(KWUI_FASTCALL *chat_backdrop_hook_fn)(void *, void *, const float *);

static chat_backdrop_stock_draw_fn chat_backdrop_original_draw;
static volatile LONG chat_backdrop_opacity;

static size_t chat_backdrop_frame_layout(void *self, const float *position, uintptr_t image, void **display_output,
                                         struct kwui_chat_backdrop_rect *rects, uint32_t opacity)
{
    const unsigned char *messages = self;
    struct kwui_chat_backdrop_row rows[KWUI_CHAT_BACKDROP_ROWS] = {{0}};
    void *display;
    uintptr_t display_vtable;
    int32_t viewport_width, viewport_height, x, y;
    unsigned i;
    size_t count = 0;
    size_t strips;

    if (!readable_memory(messages, 0xdcU)) {
        chat_backdrop_note(1, (uint32_t)(uintptr_t)messages, 0, 0, 0);
        return 0;
    }
    if (!messages[0x78]) {
        chat_backdrop_note(2, 0, 0, 0, 0);
        return 0;
    }
    if (!readable_memory(position, sizeof(float) * 2U) || !(position[0] >= -65536.0f && position[0] <= 65536.0f) ||
        !(position[1] >= -65536.0f && position[1] <= 65536.0f)) {
        chat_backdrop_note(3, (uint32_t)(uintptr_t)position, 0, 0, 0);
        return 0;
    }
    if (!readable_memory((void *)(image + kwengine_rva(0x7dd600U)), sizeof(display)))
        return 0;
    CopyMemory(&display, (void *)(image + kwengine_rva(0x7dd600U)), sizeof(display));
    if (!readable_memory(display, 0x179U)) {
        chat_backdrop_note(4, (uint32_t)(uintptr_t)display, 0, 0, 0);
        return 0;
    }
    CopyMemory(&display_vtable, display, sizeof(display_vtable));
    if (display_vtable != image + kwengine_rva(0x69b274U) || !readable_memory((void *)display_vtable, 0xecU) ||
        *(uintptr_t *)(display_vtable + 0xbcU) != image + kwengine_rva(0x36c524U) ||
        *(uintptr_t *)(display_vtable + 0xd0U) != image + kwengine_rva(0x36c90eU) ||
        *(uintptr_t *)(display_vtable + 0xe8U) != image + kwengine_rva(0x36c52fU) ||
        *(uintptr_t *)(display_vtable + 0x9cU) != image + kwengine_rva(0x38c0d0U)) {
        chat_backdrop_note(5, (uint32_t)display_vtable, (uint32_t)image, 0, 0);
        return 0;
    }
    CopyMemory(&viewport_width, (unsigned char *)display + 0x68U, sizeof(viewport_width));
    CopyMemory(&viewport_height, (unsigned char *)display + 0x6cU, sizeof(viewport_height));
    if (viewport_width <= 0 || viewport_height <= 0 || viewport_width > KWUI_CHAT_BACKDROP_MAX_DIMENSION ||
        viewport_height > KWUI_CHAT_BACKDROP_MAX_DIMENSION) {
        chat_backdrop_note(6, (uint32_t)viewport_width, (uint32_t)viewport_height, 0, 0);
        return 0;
    }

    /* Same add-0.5 then truncate as stock cvttss2si, including negative X's
     * right-edge anchoring. Stock advances by FONT height, not wrapped height. */
    x = (int32_t)(position[0] + 0.5f);
    y = (int32_t)(position[1] + 0.5f);
    for (i = KWUI_CHAT_BACKDROP_ROWS; i-- > 0;) {
        const unsigned char *string;
        uintptr_t string_vtable, font;
        uintptr_t text;
        uint16_t text_length = 0;
        int32_t width, height, font_height;
        uint32_t color;
        CopyMemory(&string, messages + 0x80U + i * 0x10U, sizeof(string));
        if (!string)
            continue;
        if (!readable_memory(string, 0x1e4U)) {
            chat_backdrop_note(7, (uint32_t)(uintptr_t)string, i, 0, 0);
            return 0;
        }
        CopyMemory(&string_vtable, string, sizeof(string_vtable));
        if (string_vtable != image + kwengine_rva(0x6a210cU)) {
            chat_backdrop_note(8, (uint32_t)string_vtable, (uint32_t)image, i, 0);
            return 0;
        }
        /* These are exactly the reads in getSize (+44) and getFont (+1c).
         * No native calls, layout recomputation, text copy or retained object. */
        CopyMemory(&width, string + 0x1dcU, sizeof(width));
        CopyMemory(&height, string + 0x1e0U, sizeof(height));
        if (width < 0 || height < 0 || width > KWUI_CHAT_BACKDROP_MAX_DIMENSION || height > KWUI_CHAT_BACKDROP_MAX_DIMENSION) {
            chat_backdrop_note(9, (uint32_t)width, (uint32_t)height, i, 0);
            return 0;
        }
        CopyMemory(&font, string + 0x08U, sizeof(font));
        if (!readable_memory((void *)font, 0x14U)) {
            chat_backdrop_note(10, (uint32_t)font, i, 0, 0);
            return 0;
        }
        CopyMemory(&font_height, (void *)(font + 0x10U), sizeof(font_height));
        if (font_height <= 0 || font_height > KWUI_CHAT_BACKDROP_MAX_DIMENSION) {
            chat_backdrop_note(11, (uint32_t)font_height, i, 0, 0);
            return 0;
        }
        /* Only nonempty text with a measured width gets a backdrop. Font pitch
         * supplies row height when needed. Length is the exact getLength (+0c)
         * read: Unicode data pointer at +4, unsigned 16-bit length at data-4. */
        CopyMemory(&text, string + 0x04U, sizeof(text));
        if (text >= 4 && readable_memory((void *)(text - 4U), sizeof(text_length)))
            CopyMemory(&text_length, (void *)(text - 4U), sizeof(text_length));
        if (text_length && (!width || !height)) {
            chat_backdrop_note(12, (uint32_t)width, (uint32_t)height, (uint32_t)font_height, text_length);
            if (!height)
                height = font_height;
        }
        CopyMemory(&color, messages + 0x88U + i * 0x10U, sizeof(color));
        rows[count].x = x < 0 ? viewport_width + x - width : x;
        rows[count].y = y;
        rows[count].width = width;
        rows[count].height = height;
        rows[count].pitch = font_height;
        rows[count].alpha = text_length ? color >> 24U : 0;
        ++count;
        y += font_height;
    }
    strips = kwui_chat_backdrop_layout(rows, count, viewport_width, viewport_height, opacity, rects);
    if (!strips) {
        chat_backdrop_note(13, (uint32_t)count, (uint32_t)x, (uint32_t)y, opacity);
        return 0;
    }
    chat_backdrop_note(0, (uint32_t)strips, (uint32_t)viewport_width, (uint32_t)viewport_height, rects[0].color);
    chat_backdrop_note(22, (uint32_t)rects[0].x, (uint32_t)rects[0].y, (uint32_t)rects[0].width, (uint32_t)rects[0].height);
    *display_output = display;
    return strips;
}

#if defined(__GNUC__)
__attribute__((noinline))
#else
__declspec(noinline)
#endif
static void chat_backdrop_render_frame(void *self, const float *position, uint32_t opacity)
{
    struct kwui_chat_backdrop_rect rects[KWUI_CHAT_BACKDROP_STRIPS];
    void *display;
    uintptr_t image, address;
    chat_backdrop_fill_fn fill = NULL;
    chat_backdrop_clip_fn set_clip = NULL;
    uintptr_t renderer;
    uint32_t saved_clip;
    size_t count, i;
    image = ui_hook_status.host_image_base;
    if (!image)
        return;
    count = chat_backdrop_frame_layout(self, position, image, &display, rects, opacity);
    if (!count)
        return;
    CopyMemory(&renderer, (unsigned char *)display + 0x164U, sizeof(renderer));
    if (!readable_memory((void *)renderer, 0x94U))
        return;
    saved_clip = ((unsigned char *)display)[0x178U];
    chat_backdrop_note(20, saved_clip, *(unsigned char *)(image + kwengine_rva(0x7bc734U)), *(uint32_t *)(renderer + 0x1cU),
                       *(uint32_t *)(renderer + 0x2cU));
    chat_backdrop_note(21, *(uint32_t *)((unsigned char *)display + 0x168U), *(uint32_t *)((unsigned char *)display + 0x16cU),
                       *(uint32_t *)((unsigned char *)display + 0x170U), *(uint32_t *)((unsigned char *)display + 0x174U));
    /* Stock immediate rectangle wrapper: Begin (+bc), Fill (+d0), Submit (+e8).
     * The raw Fill only queues geometry; Render2D::Render submits and resets it.
     * Use the same complete operation as stock UI callers, before native text. */
    address = image + kwengine_rva(0x00d5e9U);
    CopyMemory(&fill, &address, sizeof(fill));
    address = image + kwengine_rva(0x38c0d0U);
    CopyMemory(&set_clip, &address, sizeof(set_clip));
    /* Our strips were already clipped to the viewport. Do not inherit a
     * previous widget's CPU clip rectangle; restore its exact enable byte. */
    set_clip(display, 0);
    for (i = 0; i < count; ++i)
        fill(display, (float)rects[i].x, (float)rects[i].y, (float)rects[i].width, (float)rects[i].height, rects[i].color);
    set_clip(display, saved_clip);
    chat_backdrop_note(19, (uint32_t)count, rects[0].color, *(uint32_t *)(renderer + 0x1cU), *(uint32_t *)(renderer + 0x2cU));
}

static void chat_backdrop_draw(void *self, const float *position)
{
    uint32_t opacity;
    /* Keep the frame function separate so even its strip array is absent on
     * headless/disabled paths. No UI reads, device or resource lookup here. */
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U)
        return;
    opacity = (uint32_t)InterlockedCompareExchange(&chat_backdrop_opacity, 0, 0);
    if (!opacity)
        return;
    chat_backdrop_render_frame(self, position, opacity);
}

#include "chat_style.inc"

static void KWUI_FASTCALL chat_backdrop_draw_hook(void *self, void *unused, const float *position)
{
    float adjusted[2];
    const unsigned char *saved_messages = chat_style_active_messages;
    (void)unused;
    position = chat_style_position(self, position, adjusted);
    if (chat_style_outline && ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) == 0U && readable_memory(self, 0xdcU) &&
        ((unsigned char *)self)[0x78])
        chat_style_active_messages = self;
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) == 0U && chat_backdrop_opacity)
        chat_backdrop_note(14, (uint32_t)(uintptr_t)self, (uint32_t)(uintptr_t)position, 0, 0);
    chat_backdrop_draw(self, position);
    chat_backdrop_original_draw(self, position);
    chat_style_active_messages = saved_messages;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int chat_backdrop_install(void)
{
    static const struct {
        uint32_t rva;
        size_t size;
        unsigned char bytes[16];
    } signatures[] = {{0x3e1839U, 16U, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x80, 0x79, 0x78, 0x00, 0x0f, 0x84, 0xc4, 0x00, 0x00, 0x00}},
                      {0x36c90eU, 9U, {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x64, 0x01, 0x00, 0x00}},
                      {0x00d5e9U, 14U, {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x90, 0xbc, 0x00, 0x00, 0x00}},
                      {0x36c524U, 11U, {0x8b, 0x89, 0x64, 0x01, 0x00, 0x00, 0xe9, 0xb8, 0xa9, 0x12, 0x00}},
                      {0x36c52fU, 11U, {0x8b, 0x89, 0x64, 0x01, 0x00, 0x00, 0xe9, 0x05, 0xaa, 0x12, 0x00}},
                      {0x38c0d0U, 13U, {0x8a, 0x44, 0x24, 0x04, 0x88, 0x81, 0x78, 0x01, 0x00, 0x00, 0xc2, 0x04, 0x00}},
                      {0x07e279U, 4U, {0x8b, 0x41, 0x68, 0xc3}},
                      {0x07e27dU, 4U, {0x8b, 0x41, 0x6c, 0xc3}},
                      {0x22bf82U, 4U, {0x8b, 0x41, 0x08, 0xc3}},
                      {0x3df721U, 16U, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x74, 0x08, 0x8b, 0x91, 0xdc, 0x01, 0x00, 0x00, 0x89, 0x10}},
                      {0x3df739U, 11U, {0x8b, 0x89, 0xe0, 0x01, 0x00, 0x00, 0x89, 0x08, 0xc2, 0x08, 0x00}}};
    unsigned char *image = (unsigned char *)(uintptr_t)ui_hook_status.host_image_base;
    chat_backdrop_hook_fn hook = chat_backdrop_draw_hook;
    uintptr_t address = 0;
    void *trampoline;
    size_t i;
    if (chat_backdrop_original_draw)
        return 1;
    if (!image || ui_hook_status.host_image_size < 0x7dd604U)
        return 0;
    for (i = 0; i < sizeof(signatures) / sizeof(signatures[0]); ++i) {
        if (!readable_memory(image + kwengine_rva(signatures[i].rva), signatures[i].size) ||
            memcmp(image + kwengine_rva(signatures[i].rva), signatures[i].bytes, signatures[i].size)) {
            chat_backdrop_note(15, signatures[i].rva, (uint32_t)i, 0, 0);
            return 0;
        }
    }
    if (!readable_memory(image + kwengine_rva(0x69b274U), 0xecU) || !readable_memory(image + kwengine_rva(0x6a210cU), 0x48U) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b2b4U)) != (uintptr_t)image + kwengine_rva(0x07e279U) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b2b8U)) != (uintptr_t)image + kwengine_rva(0x07e27dU) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b344U)) != (uintptr_t)image + kwengine_rva(0x36c90eU) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b330U)) != (uintptr_t)image + kwengine_rva(0x36c524U) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b35cU)) != (uintptr_t)image + kwengine_rva(0x36c52fU) ||
        *(uintptr_t *)(image + kwengine_rva(0x69b310U)) != (uintptr_t)image + kwengine_rva(0x38c0d0U) ||
        *(uintptr_t *)(image + kwengine_rva(0x6a2128U)) != (uintptr_t)image + kwengine_rva(0x22bf82U) ||
        *(uintptr_t *)(image + kwengine_rva(0x6a2150U)) != (uintptr_t)image + kwengine_rva(0x3df721U)) {
        chat_backdrop_note(16, 0, 0, 0, 0);
        return 0;
    }
    CopyMemory(&address, &hook, sizeof(address));
    /* Three complete instructions, six bytes (push/mov/sub), no relative operands. */
    trampoline = install_function_trampoline(image + kwengine_rva(0x3e1839U), 6U, address);
    if (!trampoline) {
        chat_backdrop_note(17, 0, 0, 0, 0);
        return 0;
    }
    CopyMemory(&chat_backdrop_original_draw, &trampoline, sizeof(trampoline));
    chat_backdrop_note(18, (uint32_t)(uintptr_t)image, 0x3e1839U, 0, 0);
    return 1;
}

uint32_t KWENGINE_CALL KwuiConfigureChatBackdrop(uint32_t enabled, uint32_t opacity)
{
    if (enabled > 1U || opacity > 100U)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    /* Headless is a successful no-op even before image/UI initialization. */
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U || !enabled || !opacity) {
        InterlockedExchange(&chat_backdrop_opacity, 0);
        return KWENGINE_NATIVE_OK;
    }
    if (InterlockedCompareExchange(&ui_initialization_state, 0, 0) != 2 || ui_hook_status.result != KWENGINE_NATIVE_OK ||
        (ui_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && ui_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!chat_backdrop_install())
        return KWENGINE_NATIVE_ERROR_HOOK;
    InterlockedExchange(&chat_backdrop_opacity, (LONG)opacity);
    return KWENGINE_NATIVE_OK;
}
