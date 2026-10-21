/* UI-owned menu-title scaling hooks, included by the UI plugin runtime. */
#ifndef KWUI_TITLE_FIXTURE
#define KWENGINE_USE_RUNTIME_RESOLVER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include <kwengine/detail/native_log.h>
#define TITLE_CDECL __cdecl
#else
#ifndef KWUI_TITLE_PLATFORM_PROVIDED
#error "KWUI_TITLE_FIXTURE requires an explicitly supplied test platform"
#endif
#endif
#include <kwengine/engine_address.h>
#include <stdint.h>
#include <string.h>
#include <kwui/menu_title_scaling.h>

#ifndef KWUI_TITLE_FIXTURE
static HMODULE title_module;
static wchar_t title_directory[MAX_PATH];
static unsigned title_log_errors;
static int title_path(wchar_t output[MAX_PATH], const wchar_t *name)
{
    size_t directory_size = wcslen(title_directory), name_size = wcslen(name);
    if (directory_size + name_size + 1U > MAX_PATH)
        return 0;
    memcpy(output, title_directory, directory_size * sizeof(wchar_t));
    memcpy(output + directory_size, name, (name_size + 1U) * sizeof(wchar_t));
    return 1;
}
static void title_log(const char *format, ...)
{
    DWORD saved_error = GetLastError(), written = 0, error = 0;
    wchar_t path[MAX_PATH];
    char line[384];
    va_list args;
    HANDLE file;
    int length;
    va_start(args, format);
    length = vsnprintf(line, sizeof(line) - 2U, format, args);
    va_end(args);
    if (length < 0) {
        if (!(title_log_errors & 1U)) {
            title_log_errors |= 1U;
            kwengine_log_event(title_module, "menu-title-log-format", ERROR_INVALID_DATA, 0, 0, 0, 0);
        }
        goto done;
    }
    if ((size_t)length > sizeof(line) - 3U)
        length = (int)sizeof(line) - 3;
    line[length++] = '\r';
    line[length++] = '\n';
    /* Always mirror to the working startup log, including when the separate
     * title file cannot be opened. No text/player message content is logged. */
    kwengine_log_line(title_module, line, (size_t)length);
    if (!title_path(path, L"kw_title_scaling.log"))
        error = ERROR_BUFFER_OVERFLOW;
    else {
        file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           NULL);
        if (file == INVALID_HANDLE_VALUE)
            error = GetLastError();
        else {
            if (!WriteFile(file, line, (DWORD)length, &written, NULL))
                error = GetLastError();
            else if (written != (DWORD)length)
                error = ERROR_WRITE_FAULT;
            CloseHandle(file);
        }
    }
    if (error && !(title_log_errors & 2U)) {
        title_log_errors |= 2U;
        kwengine_log_event(title_module, "menu-title-log-io", error, 0, 0, 0, 0);
    }
done:
    SetLastError(saved_error);
}
static int title_span(const void *p, size_t size, int writable)
{
    MEMORY_BASIC_INFORMATION info;
    DWORD protection;
    uintptr_t start = (uintptr_t)p, end;
    if (!p || !size || start > UINTPTR_MAX - size || !VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return 0;
    end = (uintptr_t)info.BaseAddress + info.RegionSize;
    if (start + size > end)
        return 0;
    protection = info.Protect & 0xffU;
    if (writable)
        return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE ||
               protection == PAGE_EXECUTE_WRITECOPY;
    return protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
#ifndef KWUI_PLUGIN_RUNTIME
#error "Menu-title runtime must be included by the UI plugin; fixtures provide their own platform helpers."
#endif
static void *title_trampoline(unsigned char *entry, size_t stolen, uintptr_t target)
{
    return install_function_trampoline(entry, stolen, target);
}
#endif

static void title_note(const char *stage, uint32_t result, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
#ifndef KWUI_TITLE_FIXTURE
    kwengine_log_event(title_module, stage, result, a, b, c, d);
#else
    (void)stage;
    (void)result;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
#endif
}
static uint32_t title_float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return bits;
}

typedef void *(TITLE_CDECL *title_factory_fn)(void *);
static title_factory_fn title_original_factory;
static uintptr_t title_image;
static int title_enabled = 1;
static unsigned title_reports, title_observations;
static unsigned title_factory_entries;
#include "menu_title_font_limit.inc"
static void title_font_label(const char *name, char label[32])
{
    unsigned i;
    if (!name) {
        label[0] = 0;
        return;
    }
    for (i = 0; i < 31U && title_span(name + i, 1U, 0); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!c)
            break;
        label[i] = (char)((c >= 32U && c < 127U) ? c : '?');
    }
    label[i] = 0;
}
#ifndef KWUI_TITLE_FIXTURE
/* Native GameFont +14 -> FontChars; FontChars +48 is the HFONT used by
 * Create_GDI_Font (0x881e67). Read bounded diagnostics only; no font changes. */
static void title_report_font_face(uintptr_t resource, const char *kind)
{
    typedef int(WINAPI * object_fn)(HGDIOBJ, int, LPVOID);
    typedef HDC(WINAPI * dc_fn)(HDC);
    typedef HGDIOBJ(WINAPI * select_fn)(HDC, HGDIOBJ);
    typedef int(WINAPI * face_fn)(HDC, int, LPSTR);
    typedef BOOL(WINAPI * delete_fn)(HDC);
    DWORD saved_error = GetLastError();
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    HFONT handle;
    LOGFONTA logical;
    char face[LF_FACESIZE] = {0};
    float points;
    object_fn object;
    dc_fn create;
    select_fn select;
    face_fn get_face;
    delete_fn destroy;
    HDC dc;
    HGDIOBJ old;
    if (!gdi || !title_span((void *)resource, 0x4cU, 0))
        goto done;
    memcpy(&handle, (void *)(resource + 0x48U), sizeof(handle));
    memcpy(&points, (void *)(resource + 0x3cU), 4);
    object = (object_fn)(uintptr_t)GetProcAddress(gdi, "GetObjectA");
    create = (dc_fn)(uintptr_t)GetProcAddress(gdi, "CreateCompatibleDC");
    select = (select_fn)(uintptr_t)GetProcAddress(gdi, "SelectObject");
    get_face = (face_fn)(uintptr_t)GetProcAddress(gdi, "GetTextFaceA");
    destroy = (delete_fn)(uintptr_t)GetProcAddress(gdi, "DeleteDC");
    if (!object || !create || !select || !get_face || !destroy || object(handle, sizeof(logical), &logical) != sizeof(logical))
        goto done;
    dc = create(NULL);
    if (!dc)
        goto done;
    old = select(dc, handle);
    if (old && old != HGDI_ERROR) {
        (void)get_face(dc, sizeof(face), face);
        face[sizeof(face) - 1U] = 0;
        title_log("[menu-title r7] %s points=%.2f GDI_requested=%.31s GDI_selected=%.31s logical_height=%ld", kind, (double)points,
                  logical.lfFaceName, face, (long)logical.lfHeight);
        (void)select(dc, old);
    }
    (void)destroy(dc);
done:
    SetLastError(saved_error);
}
#endif
static int title_font_name(const char *name)
{
    static const char expected[] = "RussellSquare";
    unsigned i;
    if (!title_span(name, sizeof(expected), 0))
        return 0;
    for (i = 0; i < sizeof(expected); ++i) {
        unsigned char c = (unsigned char)name[i], e = (unsigned char)expected[i];
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c + 'a' - 'A');
        if (e >= 'A' && e <= 'Z')
            e = (unsigned char)(e + 'a' - 'A');
        if (c != e)
            return 0;
    }
    return 1;
}
static void *TITLE_CDECL title_factory_hook(void *descriptor)
{
    unsigned char *text = descriptor;
    struct kwui_menu_title_metrics m;
    const char *font;
    uint16_t flags, original_flags;
    float scales[2] = {1.0f, 1.0f};
    void *result;
    int report;
#ifdef KWUI_TITLE_BRIDGE
    if (((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U)
        goto stock;
#endif
    if (!title_enabled)
        goto stock;
    if (!title_span(text, 0x70U, 1)) {
        if (title_factory_entries++ == 0U) {
            title_note("menu-title-factory-enter-r5", 1, 0, 0, 0, 0);
            title_log("[menu-title r7] factory entered; descriptor rejected (not writable/readable)");
        }
        goto stock;
    }
    if (title_factory_entries++ == 0U) {
        title_note("menu-title-factory-enter-r5", 0, 0, 0, 0, 0);
        title_log("[menu-title r7] factory entered; descriptor readable/writable");
    }
    memcpy(&font, text, sizeof(font));
    memcpy(&m.left, text + 4U, 4);
    memcpy(&m.top, text + 8U, 4);
    memcpy(&m.right, text + 0xcU, 4);
    memcpy(&m.bottom, text + 0x10U, 4);
    memcpy(&m.alignment, text + 0x14U, 4);
    memcpy(&m.color, text + 0x24U, 4);
    memcpy(&m.font_height, text + 0x38U, 4);
    if ((m.color & 0xffffffU) == 0xff0000U && title_observations++ < 16U) {
        char label[32];
        title_font_label(font, label);
        memcpy(&flags, text + 0x48U, 2);
        title_note("menu-title-red-r5", flags, title_float_bits(m.font_height), title_float_bits(m.right - m.left),
                   title_float_bits(m.bottom - m.top), m.alignment);
        title_note("menu-title-predicate-r5", 0, (uint32_t)title_font_name(font), (uint32_t)kwui_is_shared_menu_title(&m), m.color, flags);
        title_log("[menu-title r7] observed red text face=%s font=%.2f width=%.2f height=%.2f alignment=%lu flags=%04x matches=%d", label,
                  (double)m.font_height, (double)(m.right - m.left), (double)(m.bottom - m.top), (unsigned long)m.alignment, (unsigned)flags,
                  title_font_name(font) && kwui_is_shared_menu_title(&m));
    }
    if (!title_font_name(font))
        goto stock;
    if (!kwui_is_shared_menu_title(&m))
        goto stock;
    memcpy(&flags, text + 0x48U, 2);
    original_flags = flags;
    /* Factory's stock native-font branch uses mask 0x406. Keep the native bit
     * on this descriptor for its whole lifetime so later text/cleanup callbacks
     * see the same representation. Other descriptor flags remain intact. */
    if (!(flags & 0x406U))
        flags = (uint16_t)(flags | 0x400U);
    memcpy(text + 0x48U, &flags, 2);
    report = title_reports++ < 8U;
    if (report) {
        unsigned char native_scaling = 0;
        if (title_span((void *)(title_image + kwengine_rva(0x794094U)), sizeof(scales), 0))
            memcpy(scales, (void *)(title_image + kwengine_rva(0x794094U)), sizeof(scales));
        if (title_span((void *)(title_image + kwengine_rva(0x793e0bU)), 1U, 0))
            memcpy(&native_scaling, (void *)(title_image + kwengine_rva(0x793e0bU)), 1);
        title_note("menu-title-scale-r5", native_scaling, original_flags, flags, title_float_bits(scales[0]), title_float_bits(scales[1]));
        title_log("[menu-title r7] title flags=%04x->%04x font=%.2f color=%08lx width=%.2f scale_x=%.3f scale_y=%.3f native_scaling=%u",
                  (unsigned)original_flags, (unsigned)flags, (double)m.font_height, (unsigned long)m.color, (double)(m.right - m.left),
                  (double)scales[0], (double)scales[1], (unsigned)native_scaling);
    }
    result = title_original_factory(descriptor);
    if (report && title_span(result, 0xcU, 0)) {
        uintptr_t display_string, native_font;
        int32_t pitch = -1;
        float font_points = 0;
        memcpy(&display_string, (unsigned char *)result + 8U, sizeof(display_string));
        if (title_span((void *)display_string, 0xcU, 0)) {
            memcpy(&native_font, (unsigned char *)display_string + 8U, sizeof(native_font));
            if (title_span((void *)native_font, 0x18U, 0)) {
                memcpy(&pitch, (unsigned char *)native_font + 0x10U, 4);
                memcpy(&font_points, (unsigned char *)native_font + 0xcU, 4);
#ifndef KWUI_TITLE_FIXTURE
                uintptr_t resource, alternate;
                memcpy(&resource, (void *)(native_font + 0x14U), sizeof(resource));
                title_report_font_face(resource, "primary-font");
                if (title_span((void *)resource, 0xcU, 0)) {
                    memcpy(&alternate, (void *)(resource + 8U), sizeof(alternate));
                    title_report_font_face(alternate, "unicode-fallback-font");
                }
#endif
            }
        }
        title_note("menu-title-font-r5", pitch < 0 ? 1U : 0U, title_float_bits(font_points), (uint32_t)pitch, 0, 0);
        title_log("[menu-title r7] native title font points=%.2f pitch=%ld", (double)font_points, (long)pitch);
    }
    return result;
stock:
    return title_original_factory(descriptor);
}
static int title_install(unsigned char *image, size_t image_size)
{
    static const unsigned char factory_signature[] = {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x74, 0x2c, 0x66, 0xf7, 0x46, 0x48, 0x06, 0x04};
    static const unsigned char constructor_signature[] = {0x55, 0x8b, 0xec, 0x0f, 0x57, 0xc0, 0x83, 0xec, 0x1c};
    title_factory_fn hook = title_factory_hook;
    uintptr_t address;
    void *trampoline;
    if (title_original_factory)
        return 1;
    if (!image || (uint64_t)kwengine_rva(0x3c1158U) + sizeof(factory_signature) > image_size ||
        (uint64_t)kwengine_rva(0x3bf780U) + sizeof(constructor_signature) > image_size || (uint64_t)kwengine_rva(0x794094U) + 4U > image_size ||
        (uint64_t)kwengine_rva(0x794098U) + 4U > image_size || !title_span(image + kwengine_rva(0x3c1158U), sizeof(factory_signature), 0) ||
        !title_span(image + kwengine_rva(0x3bf780U), sizeof(constructor_signature), 0) ||
        memcmp(image + kwengine_rva(0x3c1158U), factory_signature, sizeof(factory_signature)) ||
        memcmp(image + kwengine_rva(0x3bf780U), constructor_signature, sizeof(constructor_signature)) || !title_font_limit_install(image, image_size))
        return 0;
    memcpy(&address, &hook, sizeof(address));
    trampoline = title_trampoline(image + kwengine_rva(0x3c1158U), 7U, address);
    if (!trampoline)
        return 0;
    memcpy(&title_original_factory, &trampoline, sizeof(trampoline));
    title_image = (uintptr_t)image;
    return 1;
}

#if defined(KWUI_TITLE_BRIDGE) && !defined(KWUI_TITLE_FIXTURE)
uint32_t KWENGINE_CALL KwuiConfigureMenuTitleScaling(uint32_t enabled)
{
    DWORD length;
    wchar_t *separator;
    int installed;
    if (enabled > 1U)
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!enabled || ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED) != 0U) {
        kwengine_log_event(title_module ? title_module : NULL, "menu-title-disabled-r5", 0, enabled, (uint32_t)ui_render_flags, 0, 0);
        title_enabled = 0;
        return KWENGINE_NATIVE_OK;
    }
    if (InterlockedCompareExchange(&ui_initialization_state, 0, 0) != 2 || ui_hook_status.result != KWENGINE_NATIVE_OK ||
        (ui_hook_status.state != KWENGINE_NATIVE_STATE_PASSIVE_READY && ui_hook_status.state != KWENGINE_NATIVE_STATE_ACTIVE_READY))
        return KWENGINE_NATIVE_ERROR_CONFIG;
    if (!title_module) {
        uint32_t(KWENGINE_CALL * self)(uint32_t) = KwuiConfigureMenuTitleScaling;
        uintptr_t address;
        memcpy(&address, &self, sizeof(address));
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)address,
                                &title_module))
            return KWENGINE_NATIVE_ERROR_IO;
    }
    length = GetModuleFileNameW(title_module, title_directory, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return KWENGINE_NATIVE_ERROR_IO;
    separator = wcsrchr(title_directory, L'\\');
    if (!separator)
        separator = wcsrchr(title_directory, L'/');
    if (!separator)
        return KWENGINE_NATIVE_ERROR_IO;
    separator[1] = 0;
    title_enabled = 1;
    installed = title_install((unsigned char *)(uintptr_t)ui_hook_status.host_image_base, ui_hook_status.host_image_size);
    kwengine_log_event(title_module, "menu-title-install-r5", installed ? 0 : KWENGINE_NATIVE_ERROR_HOOK, (uint32_t)kwengine_va(0x7c1158U),
                       (uint32_t)kwengine_va(0x7bf780U), (uint32_t)ui_render_flags, (uint32_t)installed);
    title_log("[menu-title r7] bridge startup enabled=1 hook-install=%d factory=%08lx constructor=%08lx", installed,
              (unsigned long)kwengine_va(0x7c1158U), (unsigned long)kwengine_va(0x7bf780U));
    return installed ? KWENGINE_NATIVE_OK : KWENGINE_NATIVE_ERROR_HOOK;
}
#endif
