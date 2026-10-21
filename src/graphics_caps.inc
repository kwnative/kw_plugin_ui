/* Native D3D9 capability queries; included by the backend and its API fixture. */
static int sample_supported(IDirect3D9 *d, UINT adapter, D3DFORMAT adapter_format, D3DFORMAT colour, D3DFORMAT depth, BOOL windowed, unsigned samples)
{
    DWORD levels = 0;
    if (FAILED(IDirect3D9_CheckDeviceMultiSampleType(d, adapter, D3DDEVTYPE_HAL, colour, windowed, (D3DMULTISAMPLE_TYPE)samples, &levels)) || !levels)
        return 0;
    if (FAILED(IDirect3D9_CheckDeviceMultiSampleType(d, adapter, D3DDEVTYPE_HAL, depth, windowed, (D3DMULTISAMPLE_TYPE)samples, &levels)) || !levels)
        return 0;
    return SUCCEEDED(IDirect3D9_CheckDepthStencilMatch(d, adapter, D3DDEVTYPE_HAL, adapter_format, colour, depth));
}
struct kw_graphics_caps kw_graphics_d3d_query(const struct kw_graphics_device *p)
{
    struct kw_graphics_caps result = {0};
    D3DCAPS9 caps;
    D3DDISPLAYMODE mode;
    result.texture_result = (int32_t)0x80004005U;
    IDirect3D9 *d = (IDirect3D9 *)p->d3d;
    UINT adapter = p->adapter;
    if (!d || FAILED(IDirect3D9_GetDeviceCaps(d, adapter, D3DDEVTYPE_HAL, &caps)))
        return result;
    /* The existing VP6 path decodes every frame already. Auto qualifies the
     * normal shader/texture presentation path instead of guessing CPU age. */
    result.video_full = caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && caps.MaxTextureWidth >= 2048 && caps.MaxTextureHeight >= 2048;
    if (!result.video_full || !p->colour || !p->depth)
        return result;
    result.max_width = caps.MaxTextureWidth;
    result.max_height = caps.MaxTextureHeight;
    /* AdapterFormat describes a display mode, not the alpha-capable render
     * target. A8R8G8B8 backbuffers are valid with X8R8G8B8 display modes, while
     * using A8R8G8B8 as AdapterFormat can reject every modern AA option. */
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(d, adapter, &mode)))
        return result;
    result.adapter_format = (uint32_t)mode.Format;
    result.texture_result =
        IDirect3D9_CheckDeviceFormat(d, adapter, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, (D3DFORMAT)p->colour);
    /* Zero dimensions request the automatic window size. The draw path checks
     * the actual render-target descriptor before allocating/copying. */
    result.fxaa = p->width <= caps.MaxTextureWidth && p->height <= caps.MaxTextureHeight && SUCCEEDED(result.texture_result);
    for (unsigned n = 2; n <= 8; n *= 2) {
        if (sample_supported(d, adapter, mode.Format, (D3DFORMAT)p->colour, (D3DFORMAT)p->depth, (BOOL)p->windowed, n) &&
            sample_supported(d, adapter, mode.Format, D3DFMT_A8R8G8B8, (D3DFORMAT)p->depth, (BOOL)p->windowed, n) &&
            sample_supported(d, adapter, mode.Format, D3DFMT_A8R8G8B8, D3DFMT_D24X8, (BOOL)p->windowed, n))
            result.samples |= 1U << n;
    }
    return result;
}
