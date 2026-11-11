/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Native presentation plugin; AI/controller services are optional. */
#define KWENGINE_USE_RUNTIME_RESOLVER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <wchar.h>
#include <kwf/plugin.h>
#include <kwengine/native_status.h>
#include <kwengine/engine_address.h>
#include <kwui/config.h>
#include <kwengine/detail/native_log.h>
static HMODULE loader_module;
static struct kwui_config ui_config;
static const struct kwf_context *ui_context;
#include "engine_client.inc"
#include "ui_hooks.inc"
#include <kwengine/detail/config_file.inc>
static uint32_t KWF_CALL prepare(const struct kwf_context *context)
{
    if (context->role != KWF_ROLE_RENDERED)
        return KWF_UNSUPPORTED;
    ui_context = context;
    loader_module = (HMODULE)context->loader_module;
    if (!kwengine_config_load(context, L"\\plugins\\kw_ui.ini", L"\\kwbot_host.ini", &ui_config, kwui_config_parse))
        return KWF_ERROR;
    if (!kwengine_client_bind(context)) {
        context->host->log(context->owner, "UI engine component or required resolver API unavailable");
        return KWF_UNSUPPORTED;
    }
    return ui_native_prepare(context);
}
static int feature(const char *stage, uint32_t result, uint32_t a, uint32_t b)
{
    kwengine_log_event(loader_module, stage, result, a, b, 0, 0);
    if (result != KWENGINE_NATIVE_OK)
        ui_context->host->log(ui_context->owner, stage);
    return result == KWENGINE_NATIVE_OK;
}
static uint32_t KWF_CALL activate(void)
{
    InterlockedExchange(&ui_initialization_state, 2);
    uint32_t list_result = list_scroll_activate();
    if (list_result != KWF_OK)
        return list_result;
    uint32_t graphics_result = graphics_activate();
    if (graphics_result != KWF_OK)
        return graphics_result;
    /* Core presentation writes are owned by ui and roll back as a unit. */
    if (!feature("menu-title-configure", KwuiConfigureMenuTitleScaling((uint32_t)ui_config.menu_title_resolution_scaling),
                 (uint32_t)ui_config.menu_title_resolution_scaling, 0))
        return KWF_ERROR;
    if (!feature("chat-outline-colors-configure",
                 KwuiConfigureChatOutlineColors(ui_config.match_chat_outline_light_rgb, ui_config.match_chat_outline_dark_rgb),
                 ui_config.match_chat_outline_light_rgb, ui_config.match_chat_outline_dark_rgb))
        return KWF_ERROR;
    if (!feature("chat-style-configure", KwuiConfigureChatStyle((uint32_t)ui_config.match_chat_outline, ui_config.match_chat_font_scale_percent),
                 (uint32_t)ui_config.match_chat_outline, ui_config.match_chat_font_scale_percent))
        return KWF_ERROR;
    if (!feature("backdrop-configure", KwuiConfigureChatBackdrop((uint32_t)ui_config.match_chat_background, ui_config.match_chat_background_opacity),
                 ui_config.match_chat_background_opacity, 0))
        return KWF_ERROR;
    uint32_t result = KWENGINE_NATIVE_OK;
    if (ui_config.mainmenu_animation == KWUI_MENU_FAST)
        result = KwuiConfigureFastMainMenu(1);
    else if (ui_config.mainmenu_animation == KWUI_MENU_OFF)
        result = KwuiConfigureMainMenuAnimationOff(1);
    if (!feature("fast-mainmenu-configure", result, (uint32_t)ui_config.mainmenu_animation, 0))
        return KWF_ERROR;
    if (!feature("ingame-menu-configure", ingame_menu_configure((uint32_t)ui_config.ingame_menu_animation), (uint32_t)ui_config.ingame_menu_animation,
                 0))
        return KWF_ERROR;
    ui_context->host->log(ui_context->owner, "native GUI hooks active in UI plugin; framework owns presentation writes");
    return KWF_OK;
}
KWF_EXPORT const struct kwf_plugin *KWF_CALL KwPluginQuery(void)
{
    static const struct kwf_plugin plugin = {sizeof(plugin),  KWF_ABI_MAJOR, KWF_ABI_MINOR, KWF_ROLE_RENDERED,
                                             KWF_CAP_PATCHES, "ui",          prepare,       activate};
    return &plugin;
}
