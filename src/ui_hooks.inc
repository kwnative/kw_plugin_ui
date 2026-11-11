/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Rendered-only native UI runtime. The shared engine component owns discovery;
 * the framework owns every modified code/vtable range. No AI DLL is required. */
#define KWUI_PLUGIN_RUNTIME
#define KWUI_THISCALL __attribute__((thiscall))
#define KWUI_FASTCALL __attribute__((fastcall))

#include <kwui/chat_backdrop_api.h>
#include <kwui/fast_mainmenu_api.h>
static LONG ui_initialization_state;
static const LONG ui_render_flags;
static struct kwengine_native_status ui_hook_status;
static HMODULE ui_log_module(void)
{
    return (HMODULE)ui_context->loader_module;
}
#include <kwengine/detail/native_reads.inc>
static int ui_write_slot(uintptr_t slot, uintptr_t original, uintptr_t replacement)
{
    if ((slot & 3U) || !kwengine_is_data(slot, 4) || !kwengine_is_code(original))
        return 0;
    return ui_context->host->write(ui_context->owner, slot, &original, &replacement, 4) == KWF_OK;
}
static void *install_function_trampoline_proven(unsigned char *entry, size_t length, uintptr_t target, const unsigned char *proof)
{
    unsigned char expected[64], replacement[64], *copy;
    DWORD previous;
    uint32_t relative;
    if (length < 5 || length > sizeof(expected) || !target ||
        !(proof ? kwengine_code_span((uintptr_t)entry, length) : kwengine_patch_site_valid((uintptr_t)entry, length)) ||
        !read_process_raw(entry, expected, length))
        return NULL;
    if (proof && memcmp(expected, proof, length))
        return NULL;
    copy = VirtualAlloc(NULL, length + 5, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!copy)
        return NULL;
    memcpy(copy, expected, length);
    copy[length] = 0xe9;
    relative = (uint32_t)(uintptr_t)(entry + length) - (uint32_t)(uintptr_t)(copy + length + 5);
    memcpy(copy + length + 1, &relative, 4);
    if (!VirtualProtect(copy, length + 5, PAGE_EXECUTE_READ, &previous))
        goto fail;
    FlushInstructionCache(GetCurrentProcess(), copy, length + 5);
    memset(replacement, 0x90, length);
    replacement[0] = 0xe9;
    relative = (uint32_t)target - (uint32_t)(uintptr_t)(entry + 5);
    memcpy(replacement + 1, &relative, 4);
    if (ui_context->host->write(ui_context->owner, (uintptr_t)entry, expected, replacement, (uint32_t)length) != KWF_OK)
        goto fail;
    /* Process lifetime matches the plugin and its callbacks. Failed activation
     * rolls back owned entry bytes; successful hooks retain their native copy. */
    return copy;
fail:
    VirtualFree(copy, 0, MEM_RELEASE);
    return NULL;
}
static void *install_function_trampoline(unsigned char *entry, size_t length, uintptr_t target)
{
    return install_function_trampoline_proven(entry, length, target, NULL);
}
/* Same complete x87/MXCSR preservation as the presentation probe, without
 * linking its observer, queue, writer or AI match-update callbacks. */
#include <kwengine/hook_context.h>
#define memcmp kwengine_memcmp
#include "hooks/chat_backdrop.inc"
#define KWUI_TITLE_BRIDGE
#include "hooks/menu_title.inc"
#undef KWUI_TITLE_BRIDGE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#include "hooks/fast_mainmenu.inc"
#pragma GCC diagnostic pop
#undef memcmp
#include "list_scroll.inc"
#include "graphics.inc"
static uint32_t ui_native_prepare(const struct kwf_context *context)
{
    struct kwengine_resolution resolution;
    const int style = ui_config.match_chat_outline || ui_config.match_chat_font_scale_percent != 100;
    const struct {
        uintptr_t reference;
        uint32_t length;
        int enabled;
    } sites[] = {{0x7e1839, 6, style || (ui_config.match_chat_background && ui_config.match_chat_background_opacity)},
                 {0x7e598c, 6, style},
                 {0x7dfb1d, 6, style},
                 {0x7c1158, 7, ui_config.menu_title_resolution_scaling},
                 {0x85ec81, 6, ui_config.mainmenu_animation != KWUI_MENU_STOCK || ui_config.ingame_menu_animation != KWUI_MENU_STOCK}};
    if (context->role != KWF_ROLE_RENDERED || !context->game_image)
        return KWF_UNSUPPORTED;
    int resolved = kwengine_resolve((void *)context->game_image, context->game_image_size, &resolution);
    kwengine_log_event(ui_log_module(), "ui-engine-resolve", resolved ? 0 : KWENGINE_NATIVE_ERROR_SIGNATURE, resolution.resolved, resolution.expected,
                       resolution.missing_reference, resolution.image_size);
    if (!resolved)
        return KWF_UNSUPPORTED;
    for (size_t i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i)
        if (sites[i].enabled) {
            uintptr_t address = kwengine_va(sites[i].reference);
            if (!kwengine_patch_site_valid(address, sites[i].length)) {
                kwengine_log_event(ui_log_module(), "ui-prepare-site", KWENGINE_NATIVE_ERROR_HOOK, (uint32_t)sites[i].reference, (uint32_t)address,
                                   sites[i].length, 0);
                return KWF_UNSUPPORTED;
            }
            uint32_t result = context->host->reserve(context->owner, address, sites[i].length);
            if (result != KWF_OK) {
                kwengine_log_event(ui_log_module(), "ui-prepare-reserve", result, (uint32_t)sites[i].reference, (uint32_t)address, sites[i].length,
                                   0);
                return result;
            }
        }
    if (ui_config.menu_title_resolution_scaling) {
        uintptr_t slot = kwengine_va(0xa9a3dc) + 0x38, method;
        if (!kwengine_is_data(slot, 4))
            return KWF_UNSUPPORTED;
        memcpy(&method, (void *)slot, 4);
        if (method != kwengine_va(0x7b9a27))
            return KWF_UNSUPPORTED;
        uint32_t result = context->host->reserve(context->owner, slot, 4);
        if (result != KWF_OK)
            return result;
    }
    ui_hook_status.host_image_base = (uint32_t)context->game_image;
    ui_hook_status.host_image_size = context->game_image_size;
    ui_hook_status.state = KWENGINE_NATIVE_STATE_PASSIVE_READY;
    ui_hook_status.result = KWENGINE_NATIVE_OK;
    title_module = ui_log_module();
    fast_menu_module = ui_log_module();
    uint32_t result = list_scroll_prepare(context);
    return result == KWF_OK ? graphics_prepare(context) : result;
}
