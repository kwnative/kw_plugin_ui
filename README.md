# kw_plugin_ui

Licensed under **GPLv3**, with a game interoperability permission. See
[LICENSING.md](LICENSING.md) for scope, source-distribution obligations and
[third-party notices](THIRD_PARTY_NOTICES.md).

Optional threading cooperation requires a provider header with the presentation
dependency contract (`kwthreading_presentation_task_dependencies` and
`KWTHREADING_PRESENTATION_NO_VERTEX_INDEX_ACCESS`). Automatic bundle discovery
falls back to ordinary serial presentation for an older header. Explicit
`WITH_THREADING_INTEGRATION=1` requires the newer contract and reports a build
error if it is unavailable. This does not enable the experimental provider.

Public headers are under `include/kwui/`, for example
`#include <kwui/fast_mainmenu_api.h>`. Shared engine types use `kwengine/`;
loader contracts use `kwf/`. No AI-plugin include dependency is introduced.

Rendered UI, chat, menu/list and graphics presentation.

## Layout and build

Production implementation is under `src/`; supported/public or compatibility
headers are under `include/`. Generated files go only to ignored `build/`:
`build/bin` for DLLs/executables, `build/lib` for libraries, `build/obj` for
objects/dependency files, `build/config` for copied configuration templates.
Use GNU Make and x86 MinGW (`i686-w64-mingw32-gcc/g++/ar`).

```sh
make -j4
make sdk-paths
```

For relocated checkouts set `KWMOD_SDK_ROOT=/path/to/kw_modloader`.
It is the only mandatory SDK dependency: generic loader contracts and KW engine support
are both in its `include/` tree. Environment variables and Make arguments work.
`KWMOD_BIN_DIR` and `KWMOD_LIB_DIR` select matching SDK build artifacts;
`sdk-build` builds missing/stale SDK artifacts before linking. Compiler overrides
are `WINDOWS_CC`, `WINDOWS_CXX`, `WINDOWS_AR`; `BUILD_DIR` selects plugin output.

Run `make bundle` (or `make stage`) to assemble `build/bundle/` with this
plugin's DLLs, configuration and manifest in their engine-root-relative layout.
Override `BUNDLE_DIR=/path/to/staging` if needed. This is a plugin overlay,
not a standalone loader installation. Developer variants are not included.
Bundle generation replaces only previously staged, unmodified files and
refuses an unowned or locally edited output directory. Do not point it at an
installed game. See `kw_modloader/docs/BUNDLES.md` for composition and AI caveats.

Build and staging targets do not install into a game or overwrite user settings.
Use matching loader/SDK artifacts and preserve customized INIs when installing.
Runtime loading uses `.kwplugin` manifests; configuration uses `.ini` files.

Standalone `make`/`all` is serial and requires no threading headers/repository.
`bundle`/`stage` may compile optional rendering-worker cooperation when the
provider's public header is available; a workspace composition additionally
requires threading to be explicitly selected. Normal composition excludes
threading, so that cooperation stays off. Explicit
`WITH_THREADING_INTEGRATION=0|1 THREADING_SDK_ROOT=/path/to/kw_plugin_threading`
overrides automatic selection. Compiling cooperation does not bundle or enable
the provider; absence at runtime retains serial rendering.
UI configuration is owned by `kwui/config.h` and `src/config.c`, not the AI schema.

Menu-title scaling is implemented in `src/hooks/menu_title.inc`, included by
`src/ui_hooks.inc`. The standalone DInput8 proxy/entry has been removed: this file
contains UI hooks, not another game loader.
Only `kw_modloader` supplies the DInput8 entry point.
