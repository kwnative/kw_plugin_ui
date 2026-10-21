/* KW's W3DFontLibrary::loadFontData rejects integer sizes above 100.
 * Execute a private copy of its validated prefix with a 512-point comparison
 * for every font loaded through this library while scaling is enabled. All native
 * font creation, caching, Unicode fallback, cleanup and renderer code remains
 * in the engine. Entry/branch/call targets come from the runtime binding. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#define TITLE_FONT_THIS __attribute__((thiscall))
#define TITLE_FONT_FAST __attribute__((fastcall))
#else
#define TITLE_FONT_THIS __thiscall
#define TITLE_FONT_FAST __fastcall
#endif
typedef uint8_t(TITLE_FONT_THIS *title_font_loader_fn)(void *, void *);
static title_font_loader_fn title_font_original_loader, title_font_expanded_loader;
static unsigned title_font_load_reports;
#define TITLE_FONT_POINT_LIMIT 512U
static uint8_t TITLE_FONT_FAST title_font_load_hook(void *self, void *unused, void *font)
{
    int expanded = title_enabled;
    uint8_t result;
    (void)unused;
#ifdef KWUI_TITLE_BRIDGE
    if ((uint32_t)ui_render_flags & KWENGINE_RENDER_DISABLED)
        expanded = 0;
#endif
    result = (expanded ? title_font_expanded_loader : title_font_original_loader)(self, font);
    if (expanded && title_span(font, 0x20U, 0)) {
        float points;
        uint32_t quality;
        memcpy(&points, (unsigned char *)font + 0xcU, 4);
        memcpy(&quality, (unsigned char *)font + 0x1cU, 4);
        /* Report oversized loads even if many small fonts loaded first. Atomic
         * accounting keeps this shared hook's diagnostics bounded across threads. */
        if (!(points > 100.0f) || __atomic_fetch_add(&title_font_load_reports, 1U, __ATOMIC_RELAXED) >= 32U)
            return result;
        title_note("engine-font-load-r7", result, title_float_bits(points), quality, 100U, TITLE_FONT_POINT_LIMIT);
        title_log("[menu-title r7] shared native font loader points=%.2f quality=%lu limit=%lu loaded=%u", (double)points, (unsigned long)quality,
                  (unsigned long)TITLE_FONT_POINT_LIMIT, (unsigned)result);
    }
    return result;
}
static uintptr_t title_font_relative_target(const unsigned char *entry, size_t offset)
{
    int32_t displacement;
    memcpy(&displacement, entry + offset + 1U, 4);
    return (uintptr_t)(entry + offset + 5U) + (uintptr_t)(intptr_t)displacement;
}
static int title_font_code_ok(uintptr_t address)
{
#ifdef KWENGINE_USE_RUNTIME_RESOLVER
    return kwengine_is_code(address);
#else
    return title_span((void *)address, 1U, 0);
#endif
}
static int title_font_limit_install(unsigned char *image, size_t image_size)
{
    static const unsigned char prefix[] = {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x75, 0x07, 0x32, 0xc0, 0xe9, 0,    0,    0,    0, 0xd9,
                                           0x46, 0x0c, 0x53, 0x57, 0xd9, 0xc0, 0xe8, 0,    0,    0,    0,    0x83, 0xf8, 0x64, 0x77, 0};
    unsigned char *entry, *copy;
    uint32_t rva = kwengine_rva(0x3b9a27U), relative, limit = TITLE_FONT_POINT_LIMIT;
    uintptr_t tail, convert, reject, hook_address;
    uint8_t(TITLE_FONT_FAST * hook)(void *, void *, void *) = title_font_load_hook;
    uintptr_t table = (uintptr_t)image + kwengine_rva(0x69a3dcU), method = 0;
    void **slot = (void **)(table + 0x38U);
    size_t i;
    if (title_font_original_loader && title_font_expanded_loader)
        return 1;
    if (!image || (uint64_t)rva + sizeof(prefix) > image_size)
        return 0;
    entry = image + rva;
#ifdef KWENGINE_USE_RUNTIME_RESOLVER
    if (!kwengine_code_span((uintptr_t)entry, sizeof(prefix)) || !kwengine_is_data((uintptr_t)slot, 4U))
        return 0;
#endif
    if (!title_span(entry, sizeof(prefix), 0) || !title_span(slot, 4U, 0))
        return 0;
    memcpy(&method, slot, 4);
    if (method != (uintptr_t)entry)
        return 0;
    for (i = 0; i < sizeof(prefix); ++i) {
        if ((i >= 12U && i <= 15U) || (i >= 24U && i <= 27U) || i == 32U)
            continue;
        if (entry[i] != prefix[i])
            return 0;
    }
    tail = title_font_relative_target(entry, 11U);
    convert = title_font_relative_target(entry, 23U);
    reject = (uintptr_t)(entry + 33U) + (uintptr_t)(intptr_t)(int8_t)entry[32];
    if (!title_font_code_ok(tail) || !title_font_code_ok(convert) || !title_font_code_ok(reject) || !title_span((void *)tail, 4U, 0) ||
        memcmp((void *)tail, "\x5e\xc2\x04\x00", 4) || !title_span((void *)reject, 4U, 0) || memcmp((void *)reject, "\xdd\xd8\x32\xc0", 4))
        return 0;
#ifndef KWUI_TITLE_FIXTURE
    copy = VirtualAlloc(NULL, 44U, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!copy)
        return 0;
#else
    copy = mmap(NULL, 44U, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (copy == MAP_FAILED)
        return 0;
#endif
    memcpy(copy, entry, 28U);
    relative = (uint32_t)tail - (uint32_t)(uintptr_t)(copy + 16U);
    memcpy(copy + 12U, &relative, 4);
    relative = (uint32_t)convert - (uint32_t)(uintptr_t)(copy + 28U);
    memcpy(copy + 24U, &relative, 4);
    copy[28] = 0x3d;
    memcpy(copy + 29U, &limit, 4); /* cmp eax, unsigned 512 */
    copy[33] = 0x0f;
    copy[34] = 0x87; /* ja original rejection/cleanup */
    relative = (uint32_t)reject - (uint32_t)(uintptr_t)(copy + 39U);
    memcpy(copy + 35U, &relative, 4);
    copy[39] = 0xe9;
    relative = (uint32_t)(uintptr_t)(entry + 33U) - (uint32_t)(uintptr_t)(copy + 44U);
    memcpy(copy + 40U, &relative, 4);
    memcpy(&hook_address, &hook, sizeof(hook_address));
#ifndef KWUI_TITLE_FIXTURE
#ifndef KWUI_PLUGIN_RUNTIME
    DWORD old_protection, ignored;
    if (!VirtualProtect(slot, 4U, PAGE_READWRITE, &old_protection)) {
        VirtualFree(copy, 0, MEM_RELEASE);
        return 0;
    }
#else
    DWORD old_protection;
    if (!VirtualProtect(copy, 44U, PAGE_EXECUTE_READ, &old_protection)) {
        VirtualFree(copy, 0, MEM_RELEASE);
        return 0;
    }
#endif
    FlushInstructionCache(GetCurrentProcess(), copy, 44U);
#endif
    /* Publish both implementations before atomically replacing the virtual
     * method slot. The engine's instructions and 100-point guard stay intact. */
    memcpy(&title_font_expanded_loader, &copy, sizeof(copy));
    memcpy(&title_font_original_loader, &method, sizeof(method));
#ifndef KWUI_TITLE_FIXTURE
    MemoryBarrier();
#ifdef KWUI_PLUGIN_RUNTIME
    if (!ui_write_slot((uintptr_t)slot, method, hook_address)) {
#else
    void *found = InterlockedCompareExchangePointer(slot, (void *)hook_address, (void *)method);
    VirtualProtect(slot, 4U, old_protection, &ignored);
    if (found != (void *)method) {
#endif
        title_font_original_loader = NULL;
        title_font_expanded_loader = NULL;
        VirtualFree(copy, 0, MEM_RELEASE);
        return 0;
    }
#else
    memcpy(slot, &hook_address, 4);
#endif
    title_note("engine-font-limit-install-r7", 0, (uint32_t)(uintptr_t)entry, (uint32_t)convert, 100U, TITLE_FONT_POINT_LIMIT);
    return 1;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef TITLE_FONT_THIS
#undef TITLE_FONT_FAST
