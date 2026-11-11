# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KWMOD_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_modloader)
include $(KWMOD_SDK_ROOT)/mk/plugin.mk
.DEFAULT_GOAL := all
# Compile optional cooperation for bundles without enabling the provider.
THREADING_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_plugin_threading)
THREADING_INTEGRATION_HEADER := $(THREADING_SDK_ROOT)/include/kwthreading/integration/presentation_worker.h
THREADING_AUTODETECT := $(if $(filter undefined,$(origin WITH_THREADING_INTEGRATION)),1,0)
WITH_THREADING_INTEGRATION ?= $(call kwmod_bundle_integration_default,$(THREADING_INTEGRATION_HEADER),kw_plugin_threading)
WITH_THREADING_INTEGRATION := $(WITH_THREADING_INTEGRATION)
ifneq ($(WITH_THREADING_INTEGRATION),0)
ifneq ($(WITH_THREADING_INTEGRATION),1)
$(error WITH_THREADING_INTEGRATION must be 0 or 1)
endif
endif
ifeq ($(WITH_THREADING_INTEGRATION),1)
ifeq ($(wildcard $(THREADING_INTEGRATION_HEADER)),)
$(error Set THREADING_SDK_ROOT to the optional provider repository)
endif
THREADING_CONTRACT_SUPPORTED := $(shell $(WINDOWS_CC) -std=c11 -Werror -fsyntax-only \
    -I"$(KWMOD_SDK_ROOT)/include" -I"$(THREADING_SDK_ROOT)/include" \
    "$(REPO_ROOT)/tools/check_threading_contract.c" >/dev/null 2>&1 && printf 1 || printf 0)
ifneq ($(THREADING_CONTRACT_SUPPORTED),1)
ifeq ($(THREADING_AUTODETECT),1)
WITH_THREADING_INTEGRATION := 0
$(info kw_ui: older threading SDK detected; using serial presentation)
else
$(error The selected threading SDK lacks the presentation dependency contract; update it or set WITH_THREADING_INTEGRATION=0)
endif
endif
endif
CPPFLAGS += -DWITH_THREADING_INTEGRATION=$(WITH_THREADING_INTEGRATION)
ifeq ($(WITH_THREADING_INTEGRATION),1)
CPPFLAGS += -I"$(THREADING_SDK_ROOT)/include"
endif
INTEGRATION_SETTINGS := THREADING=$(WITH_THREADING_INTEGRATION):$(THREADING_SDK_ROOT)
UI_SOURCES := src/config.c src/graphics_d3d.c src/ui.c
UI_OBJECTS := $(call objects,$(UI_SOURCES))
ALL_OBJECTS := $(sort $(UI_OBJECTS))
BIN_TARGETS := $(BIN_DIR)/kw_ui.dll
CONFIG_INPUTS := $(wildcard $(REPO_ROOT)/config/*)
CONFIG_TARGETS := $(patsubst $(REPO_ROOT)/config/%,$(CONFIG_DIR)/%,$(CONFIG_INPUTS))
.PHONY: all config developer clean
all: $(BIN_DIR)/kw_ui.dll config
developer: $(BIN_TARGETS) config
config: $(CONFIG_TARGETS)
$(CONFIG_DIR)/%: $(REPO_ROOT)/config/%
	@mkdir -p "$(@D)"
	cp "$<" "$@"
ENGINE_ARTIFACTS := $(KWMOD_CONFIG_LIB) $(KWMOD_ENGINE_DLL)
include $(KWMOD_SDK_ROOT)/mk/sdk_dependencies.mk
$(ENGINE_ARTIFACTS): | $(SDK_ARTIFACT_STAMP)
	@test -f "$@"
$(BIN_DIR)/kw_ui.dll: $(UI_OBJECTS) $(KWMOD_CONFIG_LIB) $(KWMOD_ENGINE_DLL)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $(UI_OBJECTS) $(KWMOD_CONFIG_LIB)  -o "$@"
clean:
	$(RM) $(BIN_TARGETS) $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d) $(CONFIG_TARGETS)
-include $(ALL_OBJECTS:.o=.d)

BUNDLE_FILES := $(BIN_DIR)/kw_ui.dll=plugins/kw_ui.dll \
    $(REPO_ROOT)/config/kw_ui.kwplugin=plugins/kw_ui.kwplugin \
    $(REPO_ROOT)/config/kw_ui.ini=plugins/kw_ui.ini
include $(KWMOD_SDK_ROOT)/mk/bundle.mk
include $(KWMOD_SDK_ROOT)/mk/integration.mk
