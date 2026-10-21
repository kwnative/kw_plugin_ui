#ifndef KW_UI_GRAPHICS_D3D_H
#define KW_UI_GRAPHICS_D3D_H
#include <stdint.h>
struct kw_graphics_device {
    uintptr_t d3d, device;
    uint32_t adapter, colour, depth, windowed, width, height;
};
struct kw_graphics_caps {
    unsigned fxaa, video_full, samples;
    uint32_t adapter_format, max_width, max_height;
    int32_t texture_result;
};
struct kw_graphics_caps kw_graphics_d3d_query(const struct kw_graphics_device *);
/* HRESULT-style result; no engine state writes or simulation calls. */
int32_t kw_graphics_fxaa_draw(uintptr_t device);
void kw_graphics_d3d_release(void);
void kw_graphics_log(const uint16_t *plugin_directory, const char *message);
#endif
