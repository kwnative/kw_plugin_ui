/* Optional asset-data adjustment; stock archives and global clocks are untouched.
 * A resource-read observer makes the startup INI switch automatic. */
#include <kwui/fast_mainmenu.h>
#include <kwui/fast_mainmenu_api.h>
typedef unsigned char(KWUI_THISCALL *fast_menu_read_fn)(void *, uint32_t, const uint32_t *, void *);
#ifdef KWUI_PLUGIN_RUNTIME
#include "../ingame_menu.h"
static volatile LONG ingame_menu_enabled;
#endif
static fast_menu_read_fn fast_menu_original_read;
static volatile LONG fast_menu_enabled, fast_menu_patched;
static HMODULE fast_menu_module;
static int fast_menu_hook_installed;
static unsigned char KWUI_FASTCALL fast_menu_read(void *self, void *unused, uint32_t index, const uint32_t *entry, void *request)
{
    (void)unused;
    unsigned char result = fast_menu_original_read(self, index, entry, request);
    if (!result)
        return result;
    LONG mode = InterlockedCompareExchange(&fast_menu_enabled, 0, 0);
    int ingame = 0;
#ifdef KWUI_PLUGIN_RUNTIME
    LONG ingame_mode = InterlockedCompareExchange(&ingame_menu_enabled, 0, 0);
    ingame = ingame_mode && entry[1] == KW_INGAME_MENU_APT_ID;
#endif
    if (entry[0] != KWUI_MAINMENU_APT_TYPE || (!ingame && (!mode || entry[1] != KWUI_MAINMENU_APT_ID)))
        return result;
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    unsigned char *data = NULL;
    size_t changed = 0;
    int outcome = 0, reveals = 0;
    if (entry[8] >= 76 && entry[8] <= 2U * 1024U * 1024U && read_process_value((unsigned char *)request + 4, &data, 4) && data) {
        /* The resource reader has just filled this allocation with entry[8]
         * bytes; no borrowed gameplay objects or post-free input are involved. */
#ifdef KWUI_PLUGIN_RUNTIME
        if (ingame)
            outcome = kw_ingame_menu_asset(data, entry[8], ingame_mode == 2);
        else
#endif
        {
            outcome = kwui_fast_mainmenu_asset_mode(data, entry[8], &changed, mode == 2);
            if (outcome)
                reveals = kwui_fast_mainmenu_reveals_mode(data, entry[8], mode == 2);
        }
    }
#ifdef KWUI_PLUGIN_RUNTIME
    if (ingame) {
        kwengine_log_event(fast_menu_module, "ingame-menu-asset", (uint32_t)outcome, (uint32_t)ingame_mode, 33, 0, entry[8]);
        kwengine_hook_context_restore(&guard);
        return result;
    }
#endif
    if (outcome)
        InterlockedIncrement(&fast_menu_patched);
    kwengine_log_event(fast_menu_module, "fast-mainmenu-asset", (uint32_t)outcome, 33, KWUI_MAINMENU_FAST_MS, (uint32_t)changed, entry[8]);
    kwengine_log_event(fast_menu_module, "fast-mainmenu-reveals", (uint32_t)reveals, mode == 2 ? 3U : KWUI_MAINMENU_BG_FRAMES,
                       mode == 2 ? 3U : KWUI_MAINMENU_GLOBE_FRAMES, KWUI_MAINMENU_FAST_MS, entry[8]);
    kwengine_hook_context_restore(&guard);
    return result;
}
static uint32_t fast_menu_install(void)
{
    if (InterlockedCompareExchange(&ui_initialization_state, 0, 0) != 2 || ui_hook_status.result != KWENGINE_NATIVE_OK ||
        (ui_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && ui_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!fast_menu_module) {
        uintptr_t address = 0;
        __typeof__(&KwuiConfigureFastMainMenu) self = KwuiConfigureFastMainMenu;
        memcpy(&address, &self, 4);
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)address,
                                &fast_menu_module))
            return KWENGINE_NATIVE_ERROR_IO;
    }
    if (!fast_menu_original_read) {
        uintptr_t entry = kwengine_va(0x85ec81U), hook = 0;
        unsigned char code[12];
        static const unsigned char expected[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x54};
        if (!entry || !kwengine_patch_site_valid(entry, 6) || !read_process_raw((void *)entry, code, sizeof(code)) ||
            memcmp(code, expected, sizeof(code)))
            return KWENGINE_NATIVE_ERROR_HOOK;
        __typeof__(&fast_menu_read) target = fast_menu_read;
        memcpy(&hook, &target, 4);
#ifdef KWUI_PLUGIN_RUNTIME
        void *trampoline = install_function_trampoline((unsigned char *)entry, 6, hook);
        if (!trampoline)
            return KWENGINE_NATIVE_ERROR_HOOK;
        memcpy(&fast_menu_original_read, &trampoline, 4);
#else
        /* Retain the original callee before publishing the startup detour. */
        unsigned char *trampoline = VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!trampoline)
            return KWENGINE_NATIVE_ERROR_HOOK;
        memcpy(trampoline, code, 6);
        trampoline[6] = 0xe9;
        int32_t relative = (int32_t)(entry + 6 - (uintptr_t)(trampoline + 11));
        memcpy(trampoline + 7, &relative, 4);
        DWORD protect, ignored;
        if (!VirtualProtect(trampoline, 11, PAGE_EXECUTE_READ, &protect)) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return KWENGINE_NATIVE_ERROR_HOOK;
        }
        FlushInstructionCache(GetCurrentProcess(), trampoline, 11);
        memcpy(&fast_menu_original_read, &trampoline, 4);
        if (!VirtualProtect((void *)entry, 6, PAGE_EXECUTE_READWRITE, &protect)) {
            fast_menu_original_read = NULL;
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return KWENGINE_NATIVE_ERROR_HOOK;
        }
        unsigned char jump[6] = {0xe9, 0, 0, 0, 0, 0x90};
        relative = (int32_t)(hook - entry - 5);
        memcpy(jump + 1, &relative, 4);
        memcpy((void *)entry, jump, 6);
        FlushInstructionCache(GetCurrentProcess(), (void *)entry, 6);
        if (!VirtualProtect((void *)entry, 6, protect, &ignored))
            return KWENGINE_NATIVE_ERROR_HOOK;
#endif
        fast_menu_hook_installed = 1;
    }
    if (!fast_menu_hook_installed)
        return KWENGINE_NATIVE_ERROR_HOOK;
    return KWENGINE_NATIVE_OK;
}
static uint32_t fast_menu_configure(uint32_t enabled)
{
    if (!enabled || ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED)) {
        InterlockedExchange(&fast_menu_enabled, 0);
        return KWENGINE_NATIVE_OK;
    }
    uint32_t result = fast_menu_install();
    if (result == KWENGINE_NATIVE_OK)
        InterlockedExchange(&fast_menu_enabled, (LONG)enabled);
    return result;
}
#ifdef KWUI_PLUGIN_RUNTIME
static uint32_t ingame_menu_configure(uint32_t mode)
{
    if (mode > 2)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!mode) {
        InterlockedExchange(&ingame_menu_enabled, 0);
        return KWENGINE_NATIVE_OK;
    }
    uint32_t result = fast_menu_install();
    if (result == KWENGINE_NATIVE_OK)
        InterlockedExchange(&ingame_menu_enabled, (LONG)mode);
    return result;
}
#endif

uint32_t KWENGINE_CALL KwuiConfigureFastMainMenu(uint32_t enabled)
{
    if (enabled > 1)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    return fast_menu_configure(enabled);
}
uint32_t KWENGINE_CALL KwuiConfigureMainMenuAnimationOff(uint32_t enabled)
{
    if (enabled > 1)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    return fast_menu_configure(enabled ? 2U : 0U);
}

uint32_t KWENGINE_CALL KwuiConfigureSnapMainMenu(uint32_t enabled)
{
    return KwuiConfigureMainMenuAnimationOff(enabled);
}
