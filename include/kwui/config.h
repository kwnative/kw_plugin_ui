#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
enum kwui_menu_animation { KWUI_MENU_STOCK, KWUI_MENU_FAST, KWUI_MENU_OFF };
enum kwui_graphics_mode { KWUI_GRAPHICS_STOCK, KWUI_GRAPHICS_AUTO, KWUI_GRAPHICS_FULL };
struct kwui_config {
    uint32_t match_chat_background;
    uint32_t match_chat_background_opacity;
    uint32_t match_chat_outline;
    uint32_t match_chat_font_scale_percent;
    uint32_t match_chat_outline_light_rgb;
    uint32_t match_chat_outline_dark_rgb;
    uint32_t menu_title_resolution_scaling;
    uint32_t mainmenu_animation;
    uint32_t map_list_high_resolution_fix;
    uint32_t map_list_capacity;
    uint32_t ingame_menu_animation;
    uint32_t antialiasing_modes;
    uint32_t video_playback;
};
int kwui_config_parse(const void *, size_t, void *, int, size_t *);
#ifdef __cplusplus
}
#endif
