/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#include "list_scroll.h"
static struct kw_list_scroll_plan list_scroll_plan;
static int list_scroll_ready;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef void(KWUI_THISCALL *list_bind_fn)(void *, const char *, void *, void *);
#pragma GCC diagnostic pop
typedef void (*list_resize_fn)(void *, int);
static list_bind_fn list_bind_original;
static list_resize_fn list_resize;
static void KWUI_FASTCALL list_bind_hook(void *self, void *unused, const char *name, void *value, void *gadget)
{
    (void)unused;
    list_bind_original(self, name, value, gadget);
    struct kwengine_hook_context guard;
    kwengine_hook_context_save(&guard);
    uintptr_t current = 0, state = 0;
    int16_t capacity = 0, columns = 0;
    if (gadget && read_process_value((unsigned char *)self + 0x328, &current, 4) && current == (uintptr_t)gadget &&
        read_process_value((unsigned char *)gadget + 0x2c, &state, 4) && state && read_process_value((void *)state, &capacity, 2) && capacity > 0 &&
        read_process_value((void *)(state + 2), &columns, 2) && columns == 5 && (uint32_t)capacity < ui_config.map_list_capacity) {
        list_resize(gadget, (int)ui_config.map_list_capacity);
        int16_t after = 0;
        read_process_value((void *)state, &after, 2);
        kwengine_log_event(ui_log_module(), "map-list-capacity", after == (int)ui_config.map_list_capacity ? 0 : KWENGINE_NATIVE_ERROR_HOOK,
                           (uint32_t)capacity, (uint32_t)(uint16_t)after, ui_config.map_list_capacity, 0);
    }
    kwengine_hook_context_restore(&guard);
}
static uint32_t list_scroll_prepare(const struct kwf_context *context)
{
    if (!ui_config.map_list_high_resolution_fix)
        return KWF_OK;
    if (!kw_list_scroll_plan_build((const void *)context->game_image, context->game_image_size, &list_scroll_plan)) {
        context->host->log(context->owner, "high-resolution list fix unsupported: incomplete/ambiguous coordinate signatures; no list bytes changed");
        kwengine_log_event(ui_log_module(), "list-scroll-discovery", KWENGINE_NATIVE_ERROR_SIGNATURE, list_scroll_plan.count,
                           (uint32_t)KW_LIST_PATCH_COUNT, list_scroll_plan.missing, 0);
        return KWF_OK; /* Other independently supported UI features remain usable. */
    }
    for (unsigned i = 0; i < KW_LIST_COORDINATE_COUNT; ++i) {
        uintptr_t address = list_scroll_plan.sites[i];
        unsigned length = kw_list_patterns[i].length;
        if (!kwengine_code_span(address, length))
            return KWF_UNSUPPORTED;
        uint32_t result = context->host->reserve(context->owner, address, length);
        if (result != KWF_OK)
            return result;
    }
    if (!kwengine_code_span(list_scroll_plan.sites[KW_LIST_BIND_INDEX], 6) || !kwengine_code_span(list_scroll_plan.sites[KW_LIST_RESIZE_INDEX], 16))
        return KWF_UNSUPPORTED;
    uint32_t result = context->host->reserve(context->owner, list_scroll_plan.sites[KW_LIST_BIND_INDEX], 6);
    if (result != KWF_OK)
        return result;
    list_scroll_ready = 1;
    return KWF_OK;
}
static uint32_t list_scroll_activate(void)
{
    if (!list_scroll_ready)
        return KWF_OK;
    for (unsigned i = 0; i < KW_LIST_COORDINATE_COUNT; ++i) {
        const struct kw_list_pattern *p = &kw_list_patterns[i];
        uint32_t result = ui_context->host->write(ui_context->owner, list_scroll_plan.sites[i], p->before, p->after, p->length);
        if (result != KWF_OK)
            return result;
    }
    uintptr_t hook = 0, resize = list_scroll_plan.sites[KW_LIST_RESIZE_INDEX];
    __typeof__(&list_bind_hook) target = list_bind_hook;
    memcpy(&hook, &target, 4);
    /* The dedicated compiled resolver proved this additional entry during
     * prepare; its six stolen bytes contain no relative instruction. */
    void *trampoline = install_function_trampoline_proven((unsigned char *)list_scroll_plan.sites[KW_LIST_BIND_INDEX], 6, hook,
                                                          kw_list_patterns[KW_LIST_BIND_INDEX].bytes);
    if (!trampoline)
        return KWF_ERROR;
    memcpy(&list_bind_original, &trampoline, 4);
    memcpy(&list_resize, &resize, 4);
    ui_context->host->log(ui_context->owner, "high-resolution native lists use 32-bit viewport/scroll coordinates");
    kwengine_log_event(ui_log_module(), "list-scroll-32bit", KWENGINE_NATIVE_OK, list_scroll_plan.count, 32, 0, 0);
    return KWF_OK;
}
