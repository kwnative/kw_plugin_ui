/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#include "graphics_layout.h"
#include "graphics_policy.h"
#include "graphics_d3d.h"
#if WITH_THREADING_INTEGRATION
#include <kwthreading/integration/presentation_worker.h>
#endif
#include <stdio.h>
static struct kw_graphics_layout graphics_layout;
static struct kw_graphics_device graphics_cached_device;
static struct kw_graphics_caps graphics_caps;
static struct kw_aa_modes graphics_modes;
static struct kw_aa_modes graphics_label_modes;
static uintptr_t graphics_label_owner;
static int graphics_label_busy;
static int graphics_ready, graphics_cached, graphics_pending, graphics_legacy, graphics_marker;
static unsigned graphics_video_logs, graphics_fxaa_logs, graphics_fxaa_errors;
#if WITH_THREADING_INTEGRATION
static unsigned graphics_worker_logs, graphics_worker_sequence;
static const struct kwthreading_presentation_api *graphics_worker_api(void)
{
    if (!ui_context || !ui_context->host->service)
        return NULL;
    const struct kwthreading_presentation_api *api = ui_context->host->service(ui_context->owner, KWTHREADING_PRESENTATION_SERVICE, 1);
    return api && api->size >= sizeof(*api) && api->major == 1 && api->queue && api->drain ? api : NULL;
}
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(KWUI_THISCALL *graphics_get_fn)(void *, int);
typedef void(KWUI_THISCALL *graphics_set_fn)(void *, int, int);
typedef void(KWUI_THISCALL *graphics_string_ctor_fn)(void *, const char *);
typedef void(KWUI_THISCALL *graphics_string_dtor_fn)(void *);
typedef int(KWUI_THISCALL *graphics_pref_get_fn)(void *, const void *, int);
typedef void(KWUI_THISCALL *graphics_pref_set_fn)(void *, const void *, int);
typedef unsigned char(KWUI_THISCALL *graphics_video_fn)(void *);
typedef void(KWUI_THISCALL *graphics_label_set_fn)(void *, const void *, const void *, uint32_t);
#pragma GCC diagnostic pop
static uint32_t (*graphics_count_original)(void);
static unsigned char (*graphics_map_original)(int, uint32_t *, uint32_t *);
static graphics_get_fn graphics_get_original;
static graphics_set_fn graphics_set_original;
static graphics_video_fn graphics_video_original;
static uint32_t (*graphics_reset_original)(int);
static void *(*graphics_wide_format)(void *, const wchar_t *, ...);
static void (*graphics_hud_original)(void);
static graphics_string_ctor_fn graphics_string_ctor;
static graphics_string_dtor_fn graphics_string_dtor;
static graphics_pref_get_fn graphics_pref_get;
static graphics_pref_set_fn graphics_pref_set;
static graphics_label_set_fn graphics_label_set;
static graphics_string_dtor_fn graphics_wide_dtor;
static void graphics_log(const char *text)
{
    ui_context->host->log(ui_context->owner, text);
    kw_graphics_log(ui_context->plugin_directory, text);
}
static void graphics_fxaa_result(int32_t result)
{
    if (result < 0) {
        char line[100];
        snprintf(line, sizeof(line), "ui graphics: FXAA draw failed HRESULT=%08lx", (unsigned long)(uint32_t)result);
        graphics_log(line);
        ++graphics_fxaa_errors;
    } else if (!graphics_fxaa_logs++)
        graphics_log("ui graphics: FXAA scene pass active before movie/UI layers");
}
static void graphics_worker_fence(void)
{
#if WITH_THREADING_INTEGRATION
    const struct kwthreading_presentation_api *api = graphics_worker_api();
    struct kwthreading_presentation_result result;
    if (api && api->drain(&result) == KWTHREADING_PRESENTATION_QUEUED && result.sequence > graphics_worker_sequence) {
        graphics_worker_sequence = result.sequence;
        if (result.kind == KWTHREADING_PRESENTATION_FXAA)
            graphics_fxaa_result(result.result);
    }
#endif
}
#if WITH_THREADING_INTEGRATION
static int32_t KWF_CALL graphics_worker_run(uintptr_t device)
{
    return kw_graphics_fxaa_draw(device);
}
#endif
static struct kw_graphics_device graphics_device(void)
{
    struct kw_graphics_device d;
    memset(&d, 0, sizeof(d));
    d.d3d = kg_u32((void *)graphics_layout.d3d);
    d.device = kg_u32((void *)graphics_layout.device);
    d.adapter = kg_u32((void *)graphics_layout.adapter);
    d.colour = kg_u32((void *)(graphics_layout.parameters + 8));
    d.depth = kg_u32((void *)(graphics_layout.parameters + 40));
    d.windowed = *(const unsigned char *)graphics_layout.windowed;
    d.width = kg_u32((void *)graphics_layout.parameters);
    d.height = kg_u32((void *)(graphics_layout.parameters + 4));
    return d;
}
static int graphics_modern(void)
{
    struct kw_graphics_device d = graphics_device();
    if (!graphics_cached || memcmp(&d, &graphics_cached_device, sizeof(d))) {
        unsigned prior_index = kg_u32((void *)graphics_layout.selection);
        unsigned prior_mode = graphics_caps.fxaa && prior_index < graphics_modes.count ? graphics_modes.mode[prior_index] : UINT32_MAX;
        graphics_cached_device = d;
        graphics_caps = kw_graphics_d3d_query(&d);
        graphics_cached = 1;
        graphics_modes = kw_aa_modes_build((int)graphics_caps.fxaa, graphics_caps.samples);
        if (ui_config.antialiasing_modes != KWUI_GRAPHICS_STOCK && graphics_caps.fxaa && prior_mode != UINT32_MAX) {
            unsigned next = kw_aa_mode_index(&graphics_modes, prior_mode);
            memcpy((void *)graphics_layout.selection, &next, 4);
        }
        if (d.d3d) {
            char line[320];
            snprintf(
                line, sizeof(line),
                "ui graphics: adapter=%lu windowed=%lu colour=%lu depth=%lu FXAA=%u MSAA2=%u MSAA4=%u MSAA8=%u video_full=%u adapter_format=%lu size=%lux%lu texture_max=%lux%lu texture_result=%08lx",
                (unsigned long)d.adapter, (unsigned long)d.windowed, (unsigned long)d.colour, (unsigned long)d.depth, graphics_caps.fxaa,
                !!(graphics_caps.samples & (1U << 2)), !!(graphics_caps.samples & (1U << 4)), !!(graphics_caps.samples & (1U << 8)),
                graphics_caps.video_full, (unsigned long)graphics_caps.adapter_format, (unsigned long)d.width, (unsigned long)d.height,
                (unsigned long)graphics_caps.max_width, (unsigned long)graphics_caps.max_height,
                (unsigned long)(uint32_t)graphics_caps.texture_result);
            graphics_log(line);
        }
    }
    return graphics_caps.fxaa != 0;
}
static void graphics_refresh_labels(int modern, unsigned count)
{
    uintptr_t owner = kg_u32((void *)graphics_layout.label_owner);
    struct kw_aa_modes labels = graphics_modes;
    if (!modern) {
        memset(&labels, 0, sizeof(labels));
        labels.count = count;
    }
    if (!owner || graphics_label_busy || (owner == graphics_label_owner && !memcmp(&labels, &graphics_label_modes, sizeof(labels))))
        return;
    graphics_label_busy = 1;
    for (unsigned i = 1; i < count && i < 64; ++i) {
        char text[64];
        uint32_t key = 0, value = 0;
        snprintf(text, sizeof(text), "APT:AdvancedOption0_%u", i);
        graphics_string_ctor(&key, text);
        unsigned mode = modern ? graphics_modes.mode[i] : i;
        if (modern && mode == KW_AA_FXAA)
            graphics_wide_format(&value, L"FXAA");
        else
            graphics_wide_format(&value, modern ? L"%ux MSAA" : L"%ux", mode);
        graphics_label_set((void *)owner, &key, &value, 0);
        graphics_wide_dtor(&value);
        graphics_string_dtor(&key);
    }
    graphics_label_modes = labels;
    graphics_label_owner = owner;
    graphics_label_busy = 0;
}
static int graphics_profile_valid(void *self)
{
    uintptr_t vt = 0, a = 0, b = 0;
    return read_process_raw(self, &vt, 4) && read_process_raw((void *)(vt + 0x18), &a, 4) && read_process_raw((void *)(vt + 0x2c), &b, 4) &&
           a == graphics_layout.pref_get && b == graphics_layout.pref_set;
}
static int graphics_profile_marker(void *self, int write, int value)
{
    uint32_t key = 0;
    int result = 0;
    if (!graphics_profile_valid(self))
        return 0;
    graphics_string_ctor(&key, "KWPatchAntiAliasing");
    if (write)
        graphics_pref_set(self, &key, value);
    else
        result = graphics_pref_get(self, &key, 0);
    graphics_string_dtor(&key);
    return result;
}
static uint32_t graphics_count_hook(void)
{
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    int modern = graphics_modern();
    unsigned result = graphics_modes.count;
    if (!modern) {
        kwengine_hook_context_restore(&guard);
        result = graphics_count_original();
        kwengine_hook_context_save(&guard);
    }
    graphics_refresh_labels(modern, result);
    kwengine_hook_context_restore(&guard);
    return result;
}
static unsigned char graphics_map_hook(int index, uint32_t *type, uint32_t *quality)
{
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    int prior = (int)kg_u32((void *)graphics_layout.selection), modern = graphics_modern();
    if (modern && index == prior)
        index = (int)kg_u32((void *)graphics_layout.selection);
    if (modern) {
        if (graphics_pending && (int)kg_u32((void *)graphics_layout.selection) == index && index == graphics_legacy) {
            index = (int)kw_aa_migrate(&graphics_modes, graphics_legacy, graphics_marker);
            /* This is the renderer's AA selector, not simulation state. */
            memcpy((void *)graphics_layout.selection, &index, 4);
            graphics_pending = 0;
        }
        if (index < 0)
            index = 0;
        if ((unsigned)index >= graphics_modes.count)
            index = (int)graphics_modes.count - 1;
        unsigned mode = graphics_modes.mode[index];
        *type = mode >= 2 ? mode : 0;
        *quality = 0;
    }
    kwengine_hook_context_restore(&guard);
    return modern ? 1 : graphics_map_original(index, type, quality);
}
static int KWUI_FASTCALL graphics_get_hook(void *self, void *unused, int option)
{
    (void)unused;
    int value = graphics_get_original(self, option);
    if (option != 0)
        return value;
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    int marker = graphics_profile_marker(self, 0, 0);
    if (graphics_modern()) {
        unsigned index = kw_aa_migrate(&graphics_modes, value, marker);
        if (!kw_aa_marker_valid(marker)) {
            graphics_profile_marker(self, 1, KW_AA_PROFILE_VERSION + (int)graphics_modes.mode[index]);
            char line[140];
            snprintf(line, sizeof(line), "ui graphics: profile AA migrated legacy=%d mode=%u", value, graphics_modes.mode[index]);
            graphics_log(line);
        }
        value = (int)index;
        graphics_pending = 0;
    } else {
        graphics_pending = 1;
        graphics_legacy = value;
        graphics_marker = marker;
    }
    kwengine_hook_context_restore(&guard);
    return value;
}
static void KWUI_FASTCALL graphics_set_hook(void *self, void *unused, int option, int value)
{
    (void)unused;
    graphics_set_original(self, option, value);
    if (option != 0)
        return;
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    if (graphics_modern() && value >= 0 && (unsigned)value < graphics_modes.count) {
        graphics_profile_marker(self, 1, KW_AA_PROFILE_VERSION + (int)graphics_modes.mode[value]);
        graphics_pending = 0;
    }
    kwengine_hook_context_restore(&guard);
}
static void *graphics_label_hook(void *dest, const wchar_t *format, int index)
{
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    int modern = graphics_modern();
    unsigned mode = index >= 0 && (unsigned)index < graphics_modes.count ? graphics_modes.mode[index] : 0;
    kwengine_hook_context_restore(&guard);
    if (modern && mode == KW_AA_FXAA)
        return graphics_wide_format(dest, L"FXAA");
    return graphics_wide_format(dest, modern ? L"%ux MSAA" : format, modern ? (int)mode : index);
}
static void graphics_hud_hook(void)
{
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    graphics_worker_fence();
    if (graphics_modern()) {
        unsigned index = kg_u32((void *)graphics_layout.selection);
        if (index < graphics_modes.count && graphics_modes.mode[index] == KW_AA_FXAA && graphics_fxaa_errors < 3) {
#if WITH_THREADING_INTEGRATION
            const struct kwthreading_presentation_api *api = graphics_worker_api();
            uint32_t offered = KWTHREADING_PRESENTATION_SERIAL;
            if (api) {
                struct kwthreading_presentation_task task = {
                    sizeof(task), KWTHREADING_PRESENTATION_FXAA, graphics_cached_device.device, graphics_worker_run, 0, 0};
                uint16_t cw;
                __asm__ volatile("fnstcw %0\n\tstmxcsr %1" : "=m"(cw), "=m"(task.mxcsr));
                task.cw = cw;
                offered = api->queue(&task);
            }
            if (offered != KWTHREADING_PRESENTATION_QUEUED)
                graphics_fxaa_result(kw_graphics_fxaa_draw(graphics_cached_device.device));
            else if (!graphics_worker_logs++)
                graphics_log(
                    "ui graphics: FXAA queued on rendering worker; actual result collected at dependency fence; HUD/movie/Present order retained");
#else
            graphics_fxaa_result(kw_graphics_fxaa_draw(graphics_cached_device.device));
#endif
        }
    }
    kwengine_hook_context_restore(&guard);
    graphics_hud_original();
}
static uint32_t graphics_reset_hook(int force)
{
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    graphics_worker_fence();
    kw_graphics_d3d_release();
    graphics_cached = 0;
    graphics_fxaa_errors = 0;
    kwengine_hook_context_restore(&guard);
    return graphics_reset_original(force);
}
static unsigned char KWUI_FASTCALL graphics_video_hook(void *self, void *unused)
{
    (void)unused;
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    int full = ui_config.video_playback == KWUI_GRAPHICS_FULL;
    if (!full) {
        graphics_modern();
        full = graphics_caps.video_full != 0;
    }
    unsigned char old = *((unsigned char *)self + 0x88);
    if (full)
        *((unsigned char *)self + 0x88) = 0;
    if (graphics_video_logs++ < 8) {
        char line[120];
        snprintf(line, sizeof(line), "ui graphics: VP6 stream reduced_output=%u -> %u mode=%u", (unsigned)old,
                 (unsigned)*((unsigned char *)self + 0x88), (unsigned)ui_config.video_playback);
        graphics_log(line);
    }
    kwengine_hook_context_restore(&guard);
    return graphics_video_original(self);
}
static uint32_t graphics_prepare(const struct kwf_context *context)
{
    if (ui_config.antialiasing_modes == KWUI_GRAPHICS_STOCK && ui_config.video_playback == KWUI_GRAPHICS_STOCK)
        return KWF_OK;
    if (!kw_graphics_discover((const void *)context->game_image, context->game_image_size, &graphics_layout)) {
        graphics_log("ui graphics: unsupported/ambiguous graphics signatures; stock AA and video retained");
        return KWF_OK;
    }
    const unsigned ids[] = {KG_COUNT, KG_MAP, KG_GET, KG_SET, KG_LABEL, KG_HUD, KG_RESET, KG_VIDEO};
    const unsigned offsets[] = {0, 0, 0, 0, 8, 7, 0, 0}, lengths[] = {9, 6, 6, 5, 5, 5, 6, 7};
    for (unsigned i = 0; i < 8; ++i) {
        if ((i == 7 ? ui_config.video_playback : ui_config.antialiasing_modes) == KWUI_GRAPHICS_STOCK)
            continue;
        uintptr_t at = graphics_layout.site[ids[i]] + offsets[i];
        uint32_t result = context->host->reserve(context->owner, at, lengths[i]);
        if (result != KWF_OK)
            return result;
    }
    graphics_ready = 1;
    return KWF_OK;
}
static uint32_t graphics_call_patch(uintptr_t at, uintptr_t target)
{
    unsigned char before[5], after[5];
    memcpy(before, (void *)at, 5);
    after[0] = 0xe8;
    uint32_t relative = (uint32_t)target - (uint32_t)(at + 5);
    memcpy(after + 1, &relative, 4);
    return ui_context->host->write(ui_context->owner, at, before, after, 5);
}
static uint32_t graphics_activate(void)
{
    if (!graphics_ready)
        return KWF_OK;
#define KG_BIND(dst, address)                                                                                                                        \
    do {                                                                                                                                             \
        uintptr_t p = (address);                                                                                                                     \
        memcpy(&(dst), &p, 4);                                                                                                                       \
    } while (0)
#define KG_HOOK(dst, id, n, hook)                                                                                                                    \
    do {                                                                                                                                             \
        __typeof__(&(hook)) f = &(hook);                                                                                                             \
        uintptr_t target;                                                                                                                            \
        memcpy(&target, &f, 4);                                                                                                                      \
        void *t = install_function_trampoline_proven((unsigned char *)graphics_layout.site[id], n, target, (void *)graphics_layout.site[id]);        \
        if (!t) {                                                                                                                                    \
            return KWF_ERROR;                                                                                                                        \
        }                                                                                                                                            \
        memcpy(&(dst), &t, 4);                                                                                                                       \
    } while (0)
    KG_BIND(graphics_string_ctor, graphics_layout.ascii_ctor);
    KG_BIND(graphics_string_dtor, graphics_layout.ascii_dtor);
    KG_BIND(graphics_pref_get, graphics_layout.pref_get);
    KG_BIND(graphics_pref_set, graphics_layout.pref_set);
    if (ui_config.antialiasing_modes != KWUI_GRAPHICS_STOCK) {
        KG_HOOK(graphics_count_original, KG_COUNT, 9, graphics_count_hook);
        KG_HOOK(graphics_map_original, KG_MAP, 6, graphics_map_hook);
        KG_HOOK(graphics_get_original, KG_GET, 6, graphics_get_hook);
        KG_HOOK(graphics_set_original, KG_SET, 5, graphics_set_hook);
        KG_HOOK(graphics_reset_original, KG_RESET, 6, graphics_reset_hook);
        KG_BIND(graphics_wide_format, graphics_layout.wide_format);
        KG_BIND(graphics_hud_original, graphics_layout.hud_original);
        KG_BIND(graphics_label_set, graphics_layout.label_set);
        KG_BIND(graphics_wide_dtor, graphics_layout.wide_dtor);
        uintptr_t target;
        __typeof__(&graphics_label_hook) label = &graphics_label_hook;
        memcpy(&target, &label, 4);
        if (graphics_call_patch(graphics_layout.site[KG_LABEL] + 8, target) != KWF_OK)
            return KWF_ERROR;
        __typeof__(&graphics_hud_hook) hud = &graphics_hud_hook;
        memcpy(&target, &hud, 4);
        if (graphics_call_patch(graphics_layout.site[KG_HUD] + 7, target) != KWF_OK)
            return KWF_ERROR;
    }
    if (ui_config.video_playback != KWUI_GRAPHICS_STOCK)
        KG_HOOK(graphics_video_original, KG_VIDEO, 7, graphics_video_hook);
#undef KG_HOOK
#undef KG_BIND
    graphics_log("ui graphics: native AA/video hooks installed; capabilities evaluated on renderer owner thread");
    return KWF_OK;
}
