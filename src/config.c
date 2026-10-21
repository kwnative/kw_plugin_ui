#include <kwui/config.h>
#include <kwengine/config.h>
#include <string.h>
int kwui_config_parse(const void *data, size_t size, void *output, int legacy, size_t *line)
{
    struct kwui_config config = {0};
    config.match_chat_background_opacity = 75U;
    config.match_chat_font_scale_percent = 135U;
    config.match_chat_outline_light_rgb = 10526880U;
    config.match_chat_outline_dark_rgb = 1973790U;
    config.mainmenu_animation = 1U;
    config.map_list_high_resolution_fix = 1U;
    config.map_list_capacity = 4096U;
    config.ingame_menu_animation = 1U;
    static const struct kwengine_config_field fields[] = {
        {"match_chat_background", offsetof(struct kwui_config, match_chat_background), KWENGINE_CONFIG_BOOL, 0U, 1U, NULL},
        {"match_chat_background_opacity", offsetof(struct kwui_config, match_chat_background_opacity), KWENGINE_CONFIG_U32, 0U, 100U, NULL},
        {"match_chat_outline", offsetof(struct kwui_config, match_chat_outline), KWENGINE_CONFIG_BOOL, 0U, 1U, NULL},
        {"match_chat_font_scale", offsetof(struct kwui_config, match_chat_font_scale_percent), KWENGINE_CONFIG_SCALE, 50U, 200U, NULL},
        {"match_chat_outline_light_rgb", offsetof(struct kwui_config, match_chat_outline_light_rgb), KWENGINE_CONFIG_RGB, 0U, 16777215U, NULL},
        {"match_chat_outline_dark_rgb", offsetof(struct kwui_config, match_chat_outline_dark_rgb), KWENGINE_CONFIG_RGB, 0U, 16777215U, NULL},
        {"font_resolution_scaling", offsetof(struct kwui_config, menu_title_resolution_scaling), KWENGINE_CONFIG_BOOL, 0U, 1U, NULL},
        {"menu_title_resolution_scaling", offsetof(struct kwui_config, menu_title_resolution_scaling), KWENGINE_CONFIG_BOOL, 0, 1, NULL},
        {"mainmenu_animation", offsetof(struct kwui_config, mainmenu_animation), KWENGINE_CONFIG_ENUM, 0U, 2U, "stock|fast|off"},
        {"map_list_high_resolution_fix", offsetof(struct kwui_config, map_list_high_resolution_fix), KWENGINE_CONFIG_BOOL, 0U, 1U, NULL},
        {"map_list_capacity", offsetof(struct kwui_config, map_list_capacity), KWENGINE_CONFIG_U32, 999U, 32767U, NULL},
        {"ingame_menu_animation", offsetof(struct kwui_config, ingame_menu_animation), KWENGINE_CONFIG_ENUM, 0U, 2U, "stock|fast|off"},
        {"antialiasing_modes", offsetof(struct kwui_config, antialiasing_modes), KWENGINE_CONFIG_ENUM, 0U, 2U, "stock|auto|full"},
        {"video_playback", offsetof(struct kwui_config, video_playback), KWENGINE_CONFIG_ENUM, 0U, 2U, "stock|auto|full"},
    };
    if (!output || !kwengine_config_parse(data, size, &config, sizeof(config), fields, sizeof(fields) / sizeof(fields[0]), legacy, line))
        return 0;
    *(struct kwui_config *)output = config;
    return 1;
}
