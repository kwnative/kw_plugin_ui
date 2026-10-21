#ifndef KWUI_CHAT_BACKDROP_API_H
#define KWUI_CHAT_BACKDROP_API_H

#include <kwengine/native_status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional, separate export: existing bridge/telemetry ABIs are unchanged.
 * Call after successful Initialize at startup, before rendering can execute.
 * Opacity is an integer percentage: 0 transparent, 100 opaque black.
 * Disabled/zero/headless settings succeed without installing a render hook.
 * Once installed, disabling only switches off backdrop work; stock text stays.
 */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureChatBackdrop(uint32_t enabled, uint32_t opacity);

/* Startup-only presentation settings. Scale is integer hundredths internally:
 * 115 means the INI multiplier 1.15. Range 50..200; outline is 0 or 1. */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureChatStyle(uint32_t outline, uint32_t scale_percent);

/* Startup-only RGB pairs packed as 0xRRGGBB; no alpha channel. */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureChatOutlineColors(uint32_t light_rgb, uint32_t dark_rgb);

/* Startup-only shared menu heading native-font path test; default off. */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureMenuTitleScaling(uint32_t enabled);

#ifdef __cplusplus
}
#endif
#endif
