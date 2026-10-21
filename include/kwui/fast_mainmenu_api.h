#ifndef KWUI_FAST_MAINMENU_API_H
#define KWUI_FAST_MAINMENU_API_H
#include <kwengine/native_status.h>
#ifdef __cplusplus
extern "C" {
#endif
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureFastMainMenu(uint32_t enabled);
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureMainMenuAnimationOff(uint32_t enabled);
/* Compatibility alias for wrappers predating the user-facing off name. */
KWENGINE_EXPORT uint32_t KWENGINE_CALL KwuiConfigureSnapMainMenu(uint32_t enabled);
#ifdef __cplusplus
}
#endif
#endif
