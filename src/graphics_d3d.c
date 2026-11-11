/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include "graphics_d3d.h"
#include "fxaa_shader.inc"

/* One execution domain at a time: stock owner, or the optional fenced FXAA
 * worker. The UI hook drains before owner access/reset/release; source GPU
 * dependencies fence through the worker provider. No engine objects are used. */
static IDirect3DDevice9 *fx_device;
static IDirect3DTexture9 *fx_texture;
static IDirect3DPixelShader9 *fx_shader;
static IDirect3DStateBlock9 *fx_state;
static UINT fx_width, fx_height;
static D3DFORMAT fx_format;
void kw_graphics_log(const uint16_t *directory, const char *message)
{
    wchar_t path[MAX_PATH];
    const wchar_t *root = (const wchar_t *)directory;
    size_t n = root ? wcslen(root) : MAX_PATH;
    if (n + sizeof(L"\\logs\\kw_ui_graphics.log") / sizeof(wchar_t) > MAX_PATH)
        return;
    memcpy(path, root, n * sizeof(wchar_t));
    memcpy(path + n, L"\\logs", sizeof(L"\\logs"));
    if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return;
    memcpy(path + n, L"\\logs\\kw_ui_graphics.log", sizeof(L"\\logs\\kw_ui_graphics.log"));
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    SYSTEMTIME t;
    GetSystemTime(&t);
    char prefix[96];
    int length = snprintf(prefix, sizeof(prefix), "%04u-%02u-%02uT%02u:%02u:%02uZ pid=%lu ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                          (unsigned long)GetCurrentProcessId());
    DWORD written;
    if (length > 0 && (size_t)length < sizeof(prefix))
        WriteFile(file, prefix, (DWORD)length, &written, NULL);
    WriteFile(file, message, (DWORD)strlen(message), &written, NULL);
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);
}
void kw_graphics_d3d_release(void)
{
    if (fx_state)
        IDirect3DStateBlock9_Release(fx_state);
    if (fx_texture)
        IDirect3DTexture9_Release(fx_texture);
    if (fx_shader)
        IDirect3DPixelShader9_Release(fx_shader);
    fx_state = NULL;
    fx_texture = NULL;
    fx_shader = NULL;
    fx_device = NULL;
    fx_width = fx_height = 0;
}
#include "graphics_caps.inc"
struct fx_vertex {
    float x, y, z, rhw, u, v;
};
int32_t kw_graphics_fxaa_draw(uintptr_t pointer)
{
    IDirect3DDevice9 *device = (IDirect3DDevice9 *)pointer;
    IDirect3DSurface9 *target[4] = {0}, *depth = NULL, *copy = NULL;
    D3DSURFACE_DESC desc;
    D3DVIEWPORT9 viewport;
    RECT scissor;
    D3DCAPS9 caps;
    HRESULT result = D3DERR_INVALIDCALL, restored = S_OK;
    int captured = 0;
    if (!device)
        return result;
    if (FAILED(IDirect3DDevice9_GetDeviceCaps(device, &caps)) || FAILED(IDirect3DDevice9_GetRenderTarget(device, 0, &target[0])))
        return result;
    if (FAILED(IDirect3DSurface9_GetDesc(target[0], &desc)) || !desc.Width || !desc.Height || desc.MultiSampleType != D3DMULTISAMPLE_NONE ||
        desc.Width > caps.MaxTextureWidth || desc.Height > caps.MaxTextureHeight)
        goto done;
    if (fx_device != device || fx_width != desc.Width || fx_height != desc.Height || fx_format != desc.Format) {
        kw_graphics_d3d_release();
        fx_device = device;
        result = IDirect3DDevice9_CreatePixelShader(device, kw_fxaa_shader, &fx_shader);
        if (FAILED(result))
            goto done;
        result = IDirect3DDevice9_CreateTexture(device, desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &fx_texture,
                                                NULL);
        if (FAILED(result))
            goto done;
        result = IDirect3DDevice9_CreateStateBlock(device, D3DSBT_ALL, &fx_state);
        if (FAILED(result))
            goto done;
        fx_width = desc.Width;
        fx_height = desc.Height;
        fx_format = desc.Format;
    }
    result = IDirect3DDevice9_GetViewport(device, &viewport);
    if (FAILED(result))
        goto done;
    result = IDirect3DDevice9_GetScissorRect(device, &scissor);
    if (FAILED(result))
        goto done;
    result = IDirect3DStateBlock9_Capture(fx_state);
    if (FAILED(result))
        goto done;
    captured = 1;
    for (unsigned i = 1; i < caps.NumSimultaneousRTs && i < 4; ++i)
        IDirect3DDevice9_GetRenderTarget(device, i, &target[i]);
    IDirect3DDevice9_GetDepthStencilSurface(device, &depth);
    result = IDirect3DTexture9_GetSurfaceLevel(fx_texture, 0, &copy);
    if (FAILED(result))
        goto restore;
    /* StretchRect is performed outside BeginScene/EndScene. Reopen the stock
     * scene even when the copy fails so subsequent GUI calls retain its state. */
    result = IDirect3DDevice9_EndScene(device);
    if (FAILED(result))
        goto restore;
    result = IDirect3DDevice9_StretchRect(device, target[0], NULL, copy, NULL, D3DTEXF_NONE);
    restored = IDirect3DDevice9_BeginScene(device);
    if (FAILED(restored)) {
        result = restored;
        goto restore;
    }
    if (FAILED(result))
        goto restore;
    for (unsigned i = 1; i < caps.NumSimultaneousRTs && i < 4; ++i) {
        result = IDirect3DDevice9_SetRenderTarget(device, i, NULL);
        if (FAILED(result))
            goto restore;
    }
    result = IDirect3DDevice9_SetDepthStencilSurface(device, NULL);
    if (FAILED(result))
        goto restore;
    for (unsigned i = 0; i < 16; ++i) {
        result = IDirect3DDevice9_SetTexture(device, i, NULL);
        if (FAILED(result))
            goto restore;
    }
#define FX_CHECK(call)                                                                                                                               \
    do {                                                                                                                                             \
        result = (call);                                                                                                                             \
        if (FAILED(result))                                                                                                                          \
            goto restore;                                                                                                                            \
    } while (0)
    FX_CHECK(IDirect3DDevice9_SetVertexShader(device, NULL));
    FX_CHECK(IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW | D3DFVF_TEX1));
    FX_CHECK(IDirect3DDevice9_SetPixelShader(device, fx_shader));
    FX_CHECK(IDirect3DDevice9_SetTexture(device, 0, (IDirect3DBaseTexture9 *)fx_texture));
    const struct {
        D3DRENDERSTATETYPE name;
        DWORD value;
    } states[] = {{D3DRS_ZENABLE, FALSE},
                  {D3DRS_ZWRITEENABLE, FALSE},
                  {D3DRS_STENCILENABLE, FALSE},
                  {D3DRS_ALPHATESTENABLE, FALSE},
                  {D3DRS_ALPHABLENDENABLE, FALSE},
                  {D3DRS_CULLMODE, D3DCULL_NONE},
                  {D3DRS_FOGENABLE, FALSE},
                  {D3DRS_LIGHTING, FALSE},
                  {D3DRS_SCISSORTESTENABLE, FALSE},
                  {D3DRS_SRGBWRITEENABLE, FALSE},
                  {D3DRS_COLORWRITEENABLE, 15},
                  {D3DRS_CLIPPING, FALSE},
                  {D3DRS_CLIPPLANEENABLE, 0},
                  {D3DRS_WRAP0, 0},
                  {D3DRS_VERTEXBLEND, D3DVBF_DISABLE},
                  {D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE},
                  {D3DRS_FILLMODE, D3DFILL_SOLID}};
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); ++i)
        FX_CHECK(IDirect3DDevice9_SetRenderState(device, states[i].name, states[i].value));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
    FX_CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_SRGBTEXTURE, FALSE));
    FX_CHECK(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_TEXCOORDINDEX, 0));
    FX_CHECK(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
    D3DVIEWPORT9 full = {0, 0, desc.Width, desc.Height, 0, 1};
    FX_CHECK(IDirect3DDevice9_SetViewport(device, &full));
    float pixel[4] = {1.0f / (float)desc.Width, 1.0f / (float)desc.Height, 0, 0};
    FX_CHECK(IDirect3DDevice9_SetPixelShaderConstantF(device, 0, pixel, 1));
    const struct fx_vertex vertices[] = {{-0.5f, -0.5f, 0, 1, 0, 0},
                                         {(float)desc.Width - 0.5f, -0.5f, 0, 1, 1, 0},
                                         {-0.5f, (float)desc.Height - 0.5f, 0, 1, 0, 1},
                                         {(float)desc.Width - 0.5f, (float)desc.Height - 0.5f, 0, 1, 1, 1}};
    FX_CHECK(IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0])));
#undef FX_CHECK
restore:
    /* Render targets/depth are not included in a D3D9 state block. */
    if (captured) {
        for (unsigned i = 0; i < caps.NumSimultaneousRTs && i < 4; ++i) {
            HRESULT hr = IDirect3DDevice9_SetRenderTarget(device, i, target[i]);
            if (FAILED(hr))
                restored = hr;
        }
        HRESULT hr = IDirect3DDevice9_SetDepthStencilSurface(device, depth);
        if (FAILED(hr))
            restored = hr;
        hr = IDirect3DStateBlock9_Apply(fx_state);
        if (FAILED(hr))
            restored = hr;
        hr = IDirect3DDevice9_SetViewport(device, &viewport);
        if (FAILED(hr))
            restored = hr;
        hr = IDirect3DDevice9_SetScissorRect(device, &scissor);
        if (FAILED(hr))
            restored = hr;
        if (FAILED(restored))
            result = restored;
    }
done:
    if (copy)
        IDirect3DSurface9_Release(copy);
    if (depth)
        IDirect3DSurface9_Release(depth);
    for (unsigned i = 0; i < 4; ++i)
        if (target[i])
            IDirect3DSurface9_Release(target[i]);
    if (FAILED(result))
        kw_graphics_d3d_release();
    return result;
}
