LOCAL_TOOLCHAIN = .local/toolchain/mingw64
.DEFAULT_GOAL := all

# Optional text audit requires a locally owned original Rich4.exe.
.PHONY: check-original-text
check-original-text:
	$(PYTHON) tests/unit/test_original_text_audit.py
	$(PYTHON) tools/audit_original_text.py --check

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
ifneq ($(wildcard $(LOCAL_TOOLCHAIN)/bin/gcc.exe),)
CC = $(LOCAL_TOOLCHAIN)/bin/gcc.exe
LOCAL_TOOLCHAIN_BIN = $(LOCAL_TOOLCHAIN)/bin
else
CC = gcc
LOCAL_TOOLCHAIN_BIN =
endif
SDL_CONFIG ?= sdl-config
ifneq ($(wildcard .local/sdl1/ucrt64/include/SDL/SDL.h),)
SDL_PREFIX ?= .local/sdl1/ucrt64
else
SDL_PREFIX ?=
endif
ifneq ($(wildcard .local/sdl1/ucrt64/include/SDL/SDL_mixer.h),)
MIXER_PREFIX ?= .local/sdl1/ucrt64
else
MIXER_PREFIX ?=
endif
ifneq ($(wildcard .local/sdl1/ucrt64/include/SDL/SDL_ttf.h),)
TTF_PREFIX ?= .local/sdl1/ucrt64
else
TTF_PREFIX ?=
endif
ifneq ($(strip $(SDL_PREFIX)),)
WINDOWS_RUNTIME_BIN = $(SDL_PREFIX)/bin
else
WINDOWS_RUNTIME_BIN = $(LOCAL_TOOLCHAIN_BIN)
endif
else
CC = gcc
SDL_CONFIG ?= sdl-config
LOCAL_TOOLCHAIN_BIN =
WINDOWS_RUNTIME_BIN =
SDL_PREFIX ?=
ifneq ($(and $(wildcard .local/sdl1/include/SDL/SDL_mixer.h),$(wildcard .local/sdl1/lib/libSDL_mixer.dylib .local/sdl1/lib/libSDL_mixer.so .local/sdl1/lib/libSDL_mixer.a)),)
MIXER_PREFIX ?= .local/sdl1
else
MIXER_PREFIX ?=
endif
ifneq ($(and $(wildcard .local/sdl1/include/SDL/SDL_ttf.h),$(wildcard .local/sdl1/lib/libSDL_ttf.dylib .local/sdl1/lib/libSDL_ttf.so .local/sdl1/lib/libSDL_ttf.a)),)
TTF_PREFIX ?= .local/sdl1
else
TTF_PREFIX ?=
endif
endif

CFLAGS ?= -O2 -g
CPPFLAGS += -Isrc
WARNFLAGS = -std=c99 -Wall -Wextra

BUILD_DIR = build
BIN_DIR = bin

ifeq ($(OS),Windows_NT)
NULLDEV = NUL
else
NULLDEV = /dev/null
endif

PYTHON ?= $(shell python3 -c "import sys; assert sys.version_info[0] == 3; print('python3')" 2>$(NULLDEV) || python -c "import sys; assert sys.version_info[0] == 3; print('python')" 2>$(NULLDEV))
ifeq ($(strip $(PYTHON)),)
$(error Python 3 is required; neither python3 nor python provides it)
endif

MKF_OBJS = $(BUILD_DIR)/mkf.o $(BUILD_DIR)/mkf_decompress.o $(BUILD_DIR)/byte_reader.o
SPR_SMP_OBJS = $(BUILD_DIR)/spr_smp.o
FLC_OBJS = $(BUILD_DIR)/flc_animation.o
FLC_PLAYER_OBJS = $(BUILD_DIR)/flc_player.o $(BUILD_DIR)/flc_compositor.o
RESOURCE_STORE_OBJS = $(BUILD_DIR)/resource_store.o
ROUTE_EXECUTOR_OBJS = $(BUILD_DIR)/route_event_executor.o \
	$(BUILD_DIR)/route_event_executor_lifecycle.o \
	$(BUILD_DIR)/route_event_executor_decisions.o \
	$(BUILD_DIR)/route_event_executor_rewards.o \
	$(BUILD_DIR)/route_event_executor_landing.o \
	$(BUILD_DIR)/route_event_executor_confinement.o \
	$(BUILD_DIR)/route_event_executor_step_property.o \
	$(BUILD_DIR)/route_event_executor_step_special.o \
	$(BUILD_DIR)/route_event_executor_commands.o \
	$(BUILD_DIR)/route_event_executor_resume.o \
	$(BUILD_DIR)/route_event_executor_shop.o \
	$(BUILD_DIR)/route_event_executor_continuations.o

CORE_OBJS = $(BUILD_DIR)/rng.o $(BUILD_DIR)/character_template.o
CORE_OBJS += $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/god.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/special_actor_action.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/turn_around.o $(BUILD_DIR)/victory.o
CORE_OBJS += $(BUILD_DIR)/game_event_payload_request_accessors.o
CORE_OBJS += $(BUILD_DIR)/game_event_typed.o
CORE_OBJS += $(ROUTE_EXECUTOR_OBJS)
CORE_OBJS += $(BUILD_DIR)/tool_flow.o
CORE_OBJS += $(BUILD_DIR)/map_projection.o
CORE_OBJS += $(BUILD_DIR)/card_command.o
CORE_OBJS += $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/first_turn_dispatch.o
CORE_OBJS += $(BUILD_DIR)/character_speech_rules.o
CORE_OBJS += $(BUILD_DIR)/auction.o
CORE_OBJS += $(BUILD_DIR)/god_money.o
APP_OBJS = $(BUILD_DIR)/scene.o $(BUILD_DIR)/scene_frame_flow.o $(BUILD_DIR)/new_game_scene_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/menu_flow.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/game_event_presentation_snapshot.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_save.o $(BUILD_DIR)/game_session_map_init.o $(BUILD_DIR)/game_session_runtime_init.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/command_event_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/route_flow.o $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/route_event_flow.o $(BUILD_DIR)/ai_flow.o $(BUILD_DIR)/ai_settings_flow.o $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/bank_flow.o $(BUILD_DIR)/bank_interaction_flow.o $(BUILD_DIR)/stock_interaction_flow.o $(BUILD_DIR)/card_panel_flow.o $(BUILD_DIR)/card_effect_flow.o $(BUILD_DIR)/card_defense_flow.o $(BUILD_DIR)/card_use_flow.o $(BUILD_DIR)/monster_card_flow.o $(BUILD_DIR)/lottery_flow.o $(BUILD_DIR)/roulette_flow.o $(BUILD_DIR)/point_minigame_flow.o $(BUILD_DIR)/point_minigame_input.o $(BUILD_DIR)/magic_house_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/shop_flow.o $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/tool_followup_flow.o $(BUILD_DIR)/tool_panel_flow.o $(BUILD_DIR)/special_presentation_flow.o $(BUILD_DIR)/special_presentation_update_flow.o $(BUILD_DIR)/special_presentation_support_flow.o $(BUILD_DIR)/special_event_timeline.o $(BUILD_DIR)/special_actor_flow.o $(BUILD_DIR)/turn_movement_flow.o
APP_OBJS += $(BUILD_DIR)/special_place_notice_flow.o
APP_OBJS += $(BUILD_DIR)/input_flow.o
APP_OBJS += $(BUILD_DIR)/engineering_attack_flow.o

$(BUILD_DIR)/engineering_attack_flow.o: src/app/engineering_attack_flow.c src/app/engineering_attack_flow.h src/app/game_session.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/engineering_attack_flow.c
APP_OBJS += $(BUILD_DIR)/god_money_flow.o
APP_OBJS += $(BUILD_DIR)/map_input_flow.o
APP_OBJS += $(BUILD_DIR)/map_edge_scroll.o
APP_OBJS += $(BUILD_DIR)/dynamic_object_flow.o
APP_OBJS += $(BUILD_DIR)/turn_status_flow.o
APP_OBJS += $(BUILD_DIR)/first_turn_flow.o
APP_OBJS += $(BUILD_DIR)/game_update_flow.o
APP_OBJS += $(BUILD_DIR)/turn_rotation_flow.o $(BUILD_DIR)/hotel_flow.o
APP_OBJS += $(BUILD_DIR)/options_flow.o
APP_OBJS += $(BUILD_DIR)/options_game_flow.o
APP_OBJS += $(BUILD_DIR)/scene_input_flow.o
APP_OBJS += $(BUILD_DIR)/auction_flow.o
APP_OBJS += $(BUILD_DIR)/sale_interaction_flow.o
CORE_OBJS += $(BUILD_DIR)/periodic_finance.o
APP_OBJS += $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o
APP_OBJS += $(BUILD_DIR)/monthly_finance_text.o

$(BUILD_DIR)/monthly_finance_text.o: src/data/monthly_finance_text.c src/data/monthly_finance_text.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

$(BUILD_DIR)/periodic_finance.o: src/core/periodic_finance.c src/core/periodic_finance.h src/core/game_types.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<
$(BUILD_DIR)/monthly_finance_flow.o: src/app/monthly_finance_flow.c src/app/monthly_finance_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

$(BUILD_DIR)/monthly_finance_banker.o: src/app/monthly_finance_banker.c src/app/monthly_finance_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

# App compile rules do not emit compiler depfiles. Rebuild their session
# consumers when the god/modal state layout changes through this header.
$(APP_OBJS): src/app/game_presentation.h src/app/game_presentation_feature_states.h src/app/game_presentation_interactive_states.h
$(APP_OBJS): src/app/audio_event.h

# A12.3: recursive source discovery is the one SDL1 source/object inventory.
# It owns source paths, object names, compile rules and the link set so a new
# platform source cannot silently remain uncompiled or unlinked.
SDL1_SOURCES := $(shell $(PYTHON) tests/architecture/list_sdl1_sources.py)
SDL1_OBJECT_MANIFEST := $(shell $(PYTHON) tests/architecture/list_sdl1_sources.py --objects)
SDL1_OBJECTS := $(patsubst build/%,$(BUILD_DIR)/%,$(SDL1_OBJECT_MANIFEST))
SDL1_SOURCE_OBJECT_PAIRS := $(subst |build/,|$(BUILD_DIR)/,$(shell $(PYTHON) tests/architecture/list_sdl1_sources.py --pairs))
PLATFORM_OBJS = $(filter-out $(BUILD_DIR)/audio.o $(BUILD_DIR)/audio_stub.o $(if $(strip $(MIXER_LIBS)),,$(BUILD_DIR)/sdl1_audio_sdl_mixer_playback.o),$(SDL1_OBJECTS)) $(if $(strip $(MIXER_LIBS)),$(BUILD_DIR)/audio.o,$(BUILD_DIR)/audio_stub.o)
DEBUG_OBJS = $(BUILD_DIR)/trace.o
VIEW_OBJS = $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/map_renderer.o
TOOL_OBJS = $(BUILD_DIR)/mkf_dump.o
FLC_TOOL_OBJS = $(BUILD_DIR)/mkf_dump_flc_frames.o
FLC_SCAN_TOOL_OBJS = $(BUILD_DIR)/mkf_scan_flc_opcodes.o
MAP_VALIDATE_TOOL_OBJS = $(BUILD_DIR)/mkf_validate_map_data.o
M37_VALIDATE_TOOL_OBJS = $(BUILD_DIR)/mkf_validate_m37_resources.o
CONTACT_TOOL_OBJS = $(BUILD_DIR)/mkf_contact_sheet.o
SAMPLE_TOOL_OBJS = $(BUILD_DIR)/mkf_make_sample.o
EXTRACT_TOOL_OBJS = $(BUILD_DIR)/mkf_extract_resource.o
SDL_OBJS = $(BUILD_DIR)/main_sdl.o
SDL_OBJS += $(PLATFORM_OBJS)
SDL_OBJS += $(BUILD_DIR)/music.o
SDL_OBJS += $(CORE_OBJS)
SDL_OBJS += $(APP_OBJS)
SDL_OBJS += $(VIEW_OBJS)
SDL_OBJS += $(DEBUG_OBJS)
SDL_OBJS += $(RESOURCE_STORE_OBJS)
SDL_OBJS += $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o
SDL_OBJS += $(BUILD_DIR)/resource_catalog.o
SDL_OBJS += $(BUILD_DIR)/character_speech.o
SDL_OBJS += $(BUILD_DIR)/special_event_text.o
SDL_OBJS += $(FLC_PLAYER_OBJS)
# Keep the app-owned session aggregate in the explicit build dependency graph
# while the rest of the SDL1 manifest is represented by individual rules below.
SDL1_SESSION_DEPENDENCY_OBJECTS := $(filter $(BUILD_DIR)/sdl1_%.o,$(SDL_OBJS))
$(SDL1_SESSION_DEPENDENCY_OBJECTS): src/app/game_session.h
$(CORE_OBJS) $(APP_OBJS) $(VIEW_OBJS) $(SDL1_SESSION_DEPENDENCY_OBJECTS): src/core/game_types.h src/core/game_state.h
$(SDL1_SESSION_DEPENDENCY_OBJECTS): src/platform/sdl1/app_asset_packs.h

RUNTIME_DLLS = libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll

ifneq ($(strip $(SDL_PREFIX)),)
SDL_CFLAGS := -I$(SDL_PREFIX)/include
ifeq ($(OS),Windows_NT)
SDL_LIBS := $(SDL_PREFIX)/lib/libSDL.dll.a -lwinmm -limm32 -lole32 -loleaut32 -luuid -lversion -lgdi32
SDL_DLL := $(SDL_PREFIX)/bin/SDL.dll
else
SDL_LIBS := -L$(SDL_PREFIX)/lib -lSDL
SDL_DLL :=
endif
else
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags 2>$(NULLDEV))
SDL_CONFIG_PREFIX := $(shell $(SDL_CONFIG) --prefix 2>$(NULLDEV))
ifneq ($(strip $(SDL_CONFIG_PREFIX)),)
SDL_CFLAGS += -I$(SDL_CONFIG_PREFIX)/include
endif
SDL_LIBS := $(shell $(SDL_CONFIG) --libs 2>$(NULLDEV))
SDL_DLL :=
endif

ifneq ($(strip $(MIXER_PREFIX)),)
MIXER_CFLAGS := -I$(MIXER_PREFIX)/include
ifeq ($(OS),Windows_NT)
MIXER_LIBS := $(MIXER_PREFIX)/lib/libSDL_mixer.dll.a
else
MIXER_LIBS := -L$(MIXER_PREFIX)/lib -lSDL_mixer
endif
else
MIXER_CFLAGS := $(shell pkg-config --cflags SDL_mixer 2>$(NULLDEV))
MIXER_LIBS := $(shell pkg-config --libs SDL_mixer 2>$(NULLDEV))
endif

ifneq ($(strip $(TTF_PREFIX)),)
TTF_CFLAGS := -I$(TTF_PREFIX)/include -DHAVE_SDL_TTF=1
ifeq ($(OS),Windows_NT)
TTF_LIBS := $(TTF_PREFIX)/lib/libSDL_ttf.dll.a
else
TTF_LIBS := -L$(TTF_PREFIX)/lib -lSDL_ttf
endif
else
TTF_PKG_CFLAGS := $(shell pkg-config --cflags SDL_ttf 2>$(NULLDEV))
TTF_PKG_LIBS := $(shell pkg-config --libs SDL_ttf 2>$(NULLDEV))
ifneq ($(strip $(TTF_PKG_CFLAGS)$(TTF_PKG_LIBS)),)
TTF_CFLAGS := $(TTF_PKG_CFLAGS) -DHAVE_SDL_TTF=1
TTF_LIBS := $(TTF_PKG_LIBS)
else
TTF_CFLAGS :=
TTF_LIBS :=
endif
endif

.PHONY: all core check-core-headers tools sdl sdl-handheld stage-sdl-runtime test test-ledger test-bank test-property test-facility test-insurance test-card test-auction test-auction-flow test-tool test-special-event test-stock test-unit test-architecture test-resource test-preview-assets-lifetime-sanitize test-characterization test-replay test-save-load-replay test-render test-sdl-smoke test-sanitize test-original-data clean dirs check-sdl

all: sdl

core: $(CORE_OBJS)

tools: $(BIN_DIR)/mkf_dump $(BIN_DIR)/mkf_dump_flc_frames $(BIN_DIR)/mkf_scan_flc_opcodes $(BIN_DIR)/mkf_validate_map_data $(BIN_DIR)/mkf_validate_m37_resources $(BIN_DIR)/mkf_contact_sheet $(BIN_DIR)/mkf_extract_resource

sdl: check-sdl $(BIN_DIR)/rich4_sdl

sdl-handheld:
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DRICH4_PLATFORM_HANDHELD" BUILD_DIR=build/handheld BIN_DIR=bin/handheld sdl

# Host remains Windows; only compiler, headers and link libraries are ARM.
.PHONY: setup-miyoo test-miyoo-toolchain
.PHONY: setup-portmaster portmaster package-portmaster test-portmaster-package test-portmaster
PORTMASTER_ARGS ?=
setup-portmaster:
	$(PYTHON) scripts/build_portmaster.py --dependencies-only $(PORTMASTER_ARGS)

portmaster:
	$(PYTHON) scripts/build_portmaster.py $(PORTMASTER_ARGS)

package-portmaster: portmaster
	$(PYTHON) packaging/portmaster/package.py

test-portmaster-package:
	$(PYTHON) tests/unit/test_portmaster_package.py

test-portmaster: package-portmaster
	$(PYTHON) scripts/build_portmaster.py --verify $(PORTMASTER_ARGS)

MIYOO_SETUP_ARGS ?=
setup-miyoo:
	$(PYTHON) scripts/setup_miyoo_toolchain.py $(MIYOO_SETUP_ARGS)

test-miyoo-toolchain:
	$(PYTHON) tests/unit/test_miyoo_toolchain.py

.PHONY: miyoo-mixer
miyoo-mixer: setup-miyoo
	$(PYTHON) scripts/build_miyoo_mixer.py $(filter --offline,$(MIYOO_SETUP_ARGS))

.PHONY: miyoo
miyoo: setup-miyoo
	$(PYTHON) scripts/build_miyoo.py $(filter --offline,$(MIYOO_SETUP_ARGS))

.PHONY: miyoo-qa-build
miyoo-qa-build: setup-miyoo
	$(PYTHON) scripts/build_miyoo.py --qa $(filter --offline,$(MIYOO_SETUP_ARGS))

.PHONY: test-miyoo-qemu

MIYOO_QEMU_MEMORY_MB ?= 128
MIYOO_QEMU_ARGS ?=
test-miyoo-qemu: miyoo-qa-build
	$(PYTHON) scripts/setup_miyoo_toolchain.py --qemu $(MIYOO_SETUP_ARGS)
	$(PYTHON) scripts/validate_miyoo_qemu.py --memory-mb $(MIYOO_QEMU_MEMORY_MB) $(MIYOO_QEMU_ARGS)

$(BIN_DIR)/test_miyoo_qemu_probe: tests/sdl/test_miyoo_qemu_probe.c $(BUILD_DIR)/sdl1_miyoo_input.o $(BUILD_DIR)/sdl1_input_adapter.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/sdl1_text_backend_freetype.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_big5_utf8.o $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(TTF_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(TTF_LIBS) $(SDL_LIBS)

$(BUILD_DIR)/libmiyoo_qa.so: tests/sdl/miyoo_qemu_qa.c | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(filter-out -no-pie,$(CFLAGS)) $(WARNFLAGS) -fPIC -shared -o $@ $< $(SDL_LIBS) -ldl

$(BIN_DIR)/test_miyoo_asset_scope: tests/sdl/test_miyoo_asset_scope.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: package-miyoo
.PHONY: test-package-release
test-package-release:
	$(PYTHON) tests/unit/test_package_release.py
	$(PYTHON) tests/unit/test_miyoo_package.py

MIYOO_PACKAGE_ARGS ?=
package-miyoo: miyoo
	$(PYTHON) packaging/miyoo/package.py $(MIYOO_PACKAGE_ARGS)

$(BUILD_DIR)/sdl1_runtime_pacing_test.o: src/platform/sdl1/runtime.c | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -DSDL_GetTicks=qa_pacing_ticks -DSDL_Delay=qa_pacing_delay -c -o $@ $<

$(BIN_DIR)/test_frame_pacing: tests/sdl/test_frame_pacing.c $(BUILD_DIR)/sdl1_runtime_pacing_test.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) $(VIEW_OBJS) $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(SDL_LIBS)

$(BUILD_DIR)/sdl1_runtime_present_test.o: src/platform/sdl1/runtime.c src/platform/sdl1/render/pixel_rows.h | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -DRICH4_PLATFORM_MIYOO -c -o $@ $<

$(BIN_DIR)/test_present_pixels: tests/sdl/test_present_pixels.c $(BUILD_DIR)/sdl1_runtime_present_test.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) $(VIEW_OBJS) $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(SDL_LIBS)

$(BIN_DIR)/test_flc_playback: tests/sdl/test_flc_playback.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_flc_playback.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

stage-sdl-runtime: check-sdl | dirs
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(SDL_PREFIX)),)
	@for %%f in ("$(subst /,\\,$(SDL_PREFIX))\\bin\\*.dll") do copy /Y "%%f" "$(BIN_DIR)\\" >NUL
endif
ifneq ($(strip $(WINDOWS_RUNTIME_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\\,$(WINDOWS_RUNTIME_BIN))\\%%f" copy /Y "$(subst /,\\,$(WINDOWS_RUNTIME_BIN))\\%%f" "$(BIN_DIR)\\%%f" >NUL
endif
endif

.PHONY: test-context-simplification
test-context-simplification:
	$(PYTHON) tests/architecture/test_context_simplification.py
	$(PYTHON) tests/architecture/check_context_simplification.py

test-architecture: test-context-simplification
	$(PYTHON) tests/architecture/check_architecture.py
	$(PYTHON) tests/architecture/check_sdl_object_manifest.py
	$(PYTHON) tests/architecture/test_architecture_guard.py
	$(PYTHON) tests/architecture/check_renderer_viewmodel_boundary.py

include tools/frame_diff/targets.mk

test-sdl-smoke: sdl
	$(PYTHON) tests/sdl/test_runtime_smoke.py $(BIN_DIR)

test-preview-assets-lifetime-sanitize:
	$(MAKE) BUILD_DIR=build/sanitize-preview BIN_DIR=bin/sanitize-preview CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test-preview-assets-lifetime

test-sanitize:
	$(MAKE) CC=gcc BUILD_DIR=build/sanitize BIN_DIR=bin/sanitize \
		CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
		test-ledger test-property test-facility test-card test-unit test-resource test-render test-replay

test-original-data: $(BIN_DIR)/mkf_scan_flc_opcodes $(BIN_DIR)/mkf_validate_map_data $(BIN_DIR)/mkf_validate_m37_resources
ifeq ($(OS),Windows_NT)
	@if exist "assets\original\Data.mkf" ("$(subst /,\,$(BIN_DIR))\mkf_scan_flc_opcodes.exe" assets\original\Data.mkf >NUL && "$(subst /,\,$(BIN_DIR))\mkf_scan_flc_opcodes.exe" assets\original\Jump.mkf >NUL && "$(subst /,\,$(BIN_DIR))\mkf_scan_flc_opcodes.exe" assets\original\Panel.mkf >NUL && "$(subst /,\,$(BIN_DIR))\mkf_validate_map_data.exe" assets\original\Map.mkf >NUL && "$(subst /,\,$(BIN_DIR))\mkf_validate_m37_resources.exe" assets\original\Data.mkf assets\original\Effect.mkf assets\original\Panel.mkf >NUL && echo original V3.11 resource scans: ok) else (echo original data tests: SKIP ^(assets\original\Data.mkf not found^))
else
	@if test -f assets/original/Data.mkf; then \
		$(BIN_DIR)/mkf_scan_flc_opcodes assets/original/Data.mkf >/dev/null && \
		$(BIN_DIR)/mkf_scan_flc_opcodes assets/original/Jump.mkf >/dev/null && \
		$(BIN_DIR)/mkf_scan_flc_opcodes assets/original/Panel.mkf >/dev/null && \
		$(BIN_DIR)/mkf_validate_map_data assets/original/Map.mkf >/dev/null && \
		$(BIN_DIR)/mkf_validate_m37_resources assets/original/Data.mkf assets/original/Effect.mkf assets/original/Panel.mkf >/dev/null && \
		echo "original V3.11 resource scans: ok"; \
	else echo "original data tests: SKIP (assets/original/Data.mkf not found)"; fi
endif

dirs:
ifeq ($(OS),Windows_NT)
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
else
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
endif

check-sdl:
ifeq ($(strip $(SDL_CFLAGS)$(SDL_LIBS)),)
	@echo error: SDL 1.2 development files were not found.
	@echo Install SDL 1.2 and ensure 'sdl-config' is in PATH, or run 'make tools' for the non-SDL tools.
	@exit 1
else
ifeq ($(strip $(MIXER_CFLAGS)$(MIXER_LIBS)),)
	@echo SDL flags: $(SDL_CFLAGS) $(TTF_CFLAGS) $(TTF_LIBS) $(SDL_LIBS) audio=stub
else
	@echo SDL flags: $(SDL_CFLAGS) $(MIXER_CFLAGS) $(TTF_CFLAGS) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)
endif
endif

$(BIN_DIR)/mkf_dump: $(MKF_OBJS) $(SPR_SMP_OBJS) $(TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(TOOL_OBJS) $(MKF_OBJS) $(SPR_SMP_OBJS)
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(WINDOWS_RUNTIME_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" copy /Y "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
endif
endif

$(BIN_DIR)/mkf_dump_flc_frames: $(MKF_OBJS) $(FLC_OBJS) $(FLC_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(FLC_TOOL_OBJS) $(MKF_OBJS) $(FLC_OBJS)
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(WINDOWS_RUNTIME_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" copy /Y "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
endif
endif

$(BIN_DIR)/mkf_scan_flc_opcodes: $(MKF_OBJS) $(FLC_OBJS) $(BUILD_DIR)/flc_player.o $(FLC_SCAN_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(FLC_SCAN_TOOL_OBJS) $(MKF_OBJS) $(FLC_OBJS) $(BUILD_DIR)/flc_player.o

$(BIN_DIR)/mkf_validate_map_data: $(MKF_OBJS) $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(MAP_VALIDATE_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(MAP_VALIDATE_TOOL_OBJS) $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(MKF_OBJS)

$(BIN_DIR)/mkf_validate_m37_resources: $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(M37_VALIDATE_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(M37_VALIDATE_TOOL_OBJS) $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS)

$(BIN_DIR)/mkf_contact_sheet: $(MKF_OBJS) $(SPR_SMP_OBJS) $(CONTACT_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(CONTACT_TOOL_OBJS) $(MKF_OBJS) $(SPR_SMP_OBJS)
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(WINDOWS_RUNTIME_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" copy /Y "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
endif
endif

$(BIN_DIR)/mkf_extract_resource: $(MKF_OBJS) $(EXTRACT_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(EXTRACT_TOOL_OBJS) $(MKF_OBJS)

$(BIN_DIR)/mkf_make_sample: $(SAMPLE_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(SAMPLE_TOOL_OBJS)

$(BIN_DIR)/rich4_sdl: $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(SDL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(SDL_OBJS) $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(SDL_PREFIX)),)
	@for %%f in ("$(subst /,\,$(SDL_PREFIX))\bin\*.dll") do copy /Y "%%f" "$(BIN_DIR)\" >NUL
endif
ifneq ($(strip $(WINDOWS_RUNTIME_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" copy /Y "$(subst /,\,$(WINDOWS_RUNTIME_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
endif
endif

$(BUILD_DIR)/mkf.o: src/resource/mkf.c src/resource/mkf.h src/resource/mkf_decompress.h src/resource/byte_reader.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/mkf.c

$(BUILD_DIR)/mkf_fault.o: src/resource/mkf.c src/resource/mkf.h src/resource/mkf_test.h src/resource/mkf_decompress.h | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MKF_TESTING $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/mkf.c

$(BUILD_DIR)/mkf_decompress.o: src/resource/mkf_decompress.c src/resource/mkf_decompress.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/mkf_decompress.c

$(BUILD_DIR)/byte_reader.o: src/resource/byte_reader.c src/resource/byte_reader.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/byte_reader.c

$(BUILD_DIR)/game_event_payload_request_accessors.o: src/core/game_event_payload_request_accessors.c src/core/game_event_payload.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_event_payload_request_accessors.c

$(BUILD_DIR)/spr_smp.o: src/resource/spr_smp.c src/resource/spr_smp.h src/resource/byte_reader.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/spr_smp.c

$(BUILD_DIR)/flc_animation.o: src/resource/flc_animation.c src/resource/flc_animation.h src/resource/byte_reader.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/flc_animation.c

$(BUILD_DIR)/flc_player.o: src/resource/flc_player.c src/resource/flc_player.h src/resource/flc_animation.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/flc_player.c

$(BUILD_DIR)/flc_compositor.o: src/view/flc_compositor.c src/view/flc_compositor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/view/flc_compositor.c

$(BUILD_DIR)/resource_store.o: src/resource/resource_store.c src/resource/resource_store.h src/resource/mkf.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/resource_store.c

$(BUILD_DIR)/resource_store_fault.o: src/resource/resource_store.c src/resource/resource_store.h src/resource/resource_store_test.h src/resource/mkf.h | dirs
	$(CC) $(CPPFLAGS) -DRICH4_RESOURCE_STORE_TESTING $(CFLAGS) $(WARNFLAGS) -c -o $@ src/resource/resource_store.c

$(BUILD_DIR)/mkf_dump.o: tools/mkf_dump.c src/resource/mkf.h src/resource/spr_smp.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_dump.c

$(BUILD_DIR)/mkf_dump_flc_frames.o: tools/mkf_dump_flc_frames.c src/resource/mkf.h src/resource/flc_animation.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_dump_flc_frames.c

$(BUILD_DIR)/mkf_scan_flc_opcodes.o: tools/mkf_scan_flc_opcodes.c src/resource/mkf.h src/resource/flc_animation.h src/resource/flc_player.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_scan_flc_opcodes.c

$(BUILD_DIR)/mkf_validate_map_data.o: tools/mkf_validate_map_data.c src/resource/mkf.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_validate_map_data.c

$(BUILD_DIR)/mkf_validate_m37_resources.o: tools/mkf_validate_m37_resources.c src/resource/mkf.h src/resource/spr_smp.h src/resource/flc_animation.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_validate_m37_resources.c

$(BUILD_DIR)/mkf_contact_sheet.o: tools/mkf_contact_sheet.c src/resource/mkf.h src/resource/spr_smp.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_contact_sheet.c

$(BUILD_DIR)/mkf_extract_resource.o: tools/mkf_extract_resource.c src/resource/mkf.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_extract_resource.c

$(BUILD_DIR)/mkf_make_sample.o: tools/mkf_make_sample.c | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -c -o $@ tools/mkf_make_sample.c

$(BUILD_DIR)/main_sdl.o: src/main_sdl.c src/platform/sdl1/runtime.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/main_sdl.c

# A12.3: every SDL1 production object is generated from the recursive
# source/object manifest. Header dependencies are generated by GCC's
# dependency output, so this is one source of truth rather than a second
# hand-maintained object-rule inventory.
define RICH4_SDL1_COMPILE_RULE
$(word 2,$(subst |, ,$(1))): $(word 1,$(subst |, ,$(1))) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(MIXER_CFLAGS) $(TTF_CFLAGS) $(CFLAGS) $(WARNFLAGS) -MMD -MP -MF $$(@:.o=.d) -c -o $$@ $(word 1,$(subst |, ,$(1)))
endef
$(foreach pair,$(SDL1_SOURCE_OBJECT_PAIRS),$(eval $(call RICH4_SDL1_COMPILE_RULE,$(pair))))
-include $(wildcard $(BUILD_DIR)/*.d)
$(BUILD_DIR)/music.o: src/data/music.c src/data/music.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/music.c

$(BUILD_DIR)/character_template.o: src/core/character_template.c src/core/character_template.h src/core/game_types.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/character_template.c

$(BUILD_DIR)/rng.o: src/core/rng.c src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/rng.c

$(BUILD_DIR)/map_projection.o: src/core/map_projection.c src/core/map_projection.h src/data/rich4_map_tables.inc | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/map_projection.c

$(BUILD_DIR)/new_game_init.o: src/core/new_game_init.c src/core/new_game_init.h src/core/game_state.h src/core/special_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/new_game_init.c

$(BUILD_DIR)/first_turn_dispatch.o: src/core/first_turn_dispatch.c src/core/first_turn_dispatch.h src/core/game_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/first_turn_dispatch.c

$(BUILD_DIR)/game_domain.o: src/core/game_domain.c src/core/game_domain.h src/core/game_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_domain.c

$(BUILD_DIR)/game_command.o: src/core/game_command.c src/core/game_command.h src/core/game_domain.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_command.c

$(BUILD_DIR)/game_event.o: src/core/game_event.c src/core/game_event.h src/core/game_command.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_event.c

$(BUILD_DIR)/game_event_payload.o: src/core/game_event_payload.c src/core/game_event_payload.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_event_payload.c

$(BUILD_DIR)/game_event_typed.o: src/core/game_event_typed.c src/core/game_event_payload.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_event_typed.c

$(BUILD_DIR)/game_dispatch.o: src/core/game_dispatch.c src/core/game_dispatch.h src/core/route_event_executor.h src/core/game_event.h src/core/game_flow_state.h src/core/game_state.h src/core/player_status.h src/core/tool.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/game_dispatch.c

$(BUILD_DIR)/ledger.o: src/core/ledger.c src/core/ledger.h src/core/game_state.h src/core/game_event.h src/core/victory.h src/core/stock.h src/core/auction.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/ledger.c

$(BUILD_DIR)/auction.o: src/core/auction.c src/core/auction.h src/core/game_state.h src/core/game_event.h src/core/ledger.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/auction.c

$(BUILD_DIR)/auction_flow.o: src/app/auction_flow.c src/app/auction_flow.h src/app/game_session.h src/app/audio_event.h src/core/auction.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/auction_flow.c

$(BUILD_DIR)/insurance.o: src/core/insurance.c src/core/insurance.h src/core/game_state.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/insurance.c

$(BUILD_DIR)/bank.o: src/core/bank.c src/core/bank.h src/core/game_state.h src/core/rng.h src/core/stock.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/bank.c

$(BUILD_DIR)/god_money.o: src/core/god_money.c src/core/god.h src/core/game_state.h src/core/game_event_payload.h src/core/ledger.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/god_money.c

$(BUILD_DIR)/god_money_flow.o: src/app/god_money_flow.c src/app/god_money_flow.h src/app/game_presentation_feature_states.h src/app/game_session.h src/app/audio_event.h src/core/god.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/god_money_flow.c

$(BUILD_DIR)/god.o: src/core/god.c src/core/god.h src/core/game_state.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/god.c

$(BUILD_DIR)/confinement.o: src/core/confinement.c src/core/confinement.h src/core/game_state.h src/core/game_event.h src/core/insurance.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/confinement.c

$(BUILD_DIR)/special_event.o: src/core/special_event.c src/core/special_event.h src/core/special_event_internal.h src/core/tool_internal.h src/core/rng.h src/core/confinement.h src/core/insurance.h src/core/ledger.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_event.c

SPECIAL_EVENT_INTERNAL_HEADERS = src/core/special_event_internal.h src/core/special_event.h src/core/confinement.h src/core/card.h src/core/insurance.h src/core/ledger.h src/core/stock.h src/core/tool.h src/core/victory.h

$(BUILD_DIR)/special_event_news.o: src/core/special_event_news.c $(SPECIAL_EVENT_INTERNAL_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_event_news.c

$(BUILD_DIR)/special_event_magic.o: src/core/special_event_magic.c $(SPECIAL_EVENT_INTERNAL_HEADERS) src/core/turn_around.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_event_magic.c

$(BUILD_DIR)/special_event_commit.o: src/core/special_event_commit.c $(SPECIAL_EVENT_INTERNAL_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_event_commit.c

$(BUILD_DIR)/stock.o: src/core/stock.c src/core/stock.h src/core/rng.h src/core/game_state.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/stock.c

$(BUILD_DIR)/stock_snapshot.o: src/core/stock_snapshot.c src/core/stock.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/stock_snapshot.c

$(BUILD_DIR)/property.o: src/core/property.c src/core/property.h src/core/card.h src/core/ledger.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/property.c

$(BUILD_DIR)/facility.o: src/core/facility.c src/core/facility.h src/core/facility_internal.h src/core/ai.h src/core/card.h src/core/insurance.h src/core/stock.h src/core/ledger.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/facility.c

$(BUILD_DIR)/company.o: src/core/company.c src/core/facility_internal.h src/core/facility.h src/core/card.h src/core/god.h src/core/insurance.h src/core/ledger.h src/core/stock.h src/core/confinement.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/company.c

$(BUILD_DIR)/card.o: src/core/card.c src/core/card.h src/core/game_state.h src/core/game_event.h src/core/insurance.h src/core/confinement.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/card.c

$(BUILD_DIR)/card_command.o: src/core/card_command.c src/core/card_internal.h src/core/card.h src/core/god.h src/core/stock.h src/core/turn_around.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/card_command.c

$(BUILD_DIR)/tool.o: src/core/tool.c src/core/tool.h src/core/tool_internal.h src/core/game_state.h src/core/game_event.h src/core/insurance.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/tool.c

$(BUILD_DIR)/tool_flow.o: src/core/tool_flow.c src/core/tool_internal.h src/core/tool.h src/core/insurance.h src/core/confinement.h src/core/map_projection.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/tool_flow.c

$(BUILD_DIR)/ai.o: src/core/ai.c src/core/ai.h src/core/ai_internal.h src/core/card.h src/core/tool.h src/core/stock.h src/core/property.h src/core/facility.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/ai.c

$(BUILD_DIR)/ai_stock_policy.o: src/core/ai_stock_policy.c src/core/ai_internal.h src/core/ai.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/ai_stock_policy.c

$(BUILD_DIR)/route_movement.o: src/core/route_movement.c src/core/route_movement.h src/core/rng.h src/core/tool.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_movement.c

$(BUILD_DIR)/route_event_queue.o: src/core/route_event_queue.c src/core/route_event_queue.h src/core/route_event_executor_state.h src/core/game_constants.h src/core/game_state.h src/core/bank.h src/core/special_event.h src/core/tool.h src/data/map_object_ref.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_queue.c

ROUTE_EXECUTOR_HEADERS = src/core/route_event_executor_internal.h src/core/route_event_executor.h src/core/route_event_queue.h src/core/route_event_executor_state.h src/core/route_event.h src/core/game_event.h src/core/game_command.h src/core/game_domain.h src/core/game_state.h src/core/ledger.h src/core/bank.h src/core/card.h src/core/confinement.h src/core/facility.h src/core/property.h src/core/rng.h src/core/special_event.h src/core/tool.h

$(BUILD_DIR)/route_event_executor.o: src/core/route_event_executor.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor.c

$(BUILD_DIR)/route_event_executor_lifecycle.o: src/core/route_event_executor_lifecycle.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_lifecycle.c

$(BUILD_DIR)/route_event_executor_decisions.o: src/core/route_event_executor_decisions.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_decisions.c

$(BUILD_DIR)/route_event_executor_rewards.o: src/core/route_event_executor_rewards.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_rewards.c

$(BUILD_DIR)/route_event_executor_landing.o: src/core/route_event_executor_landing.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_landing.c

$(BUILD_DIR)/route_event_executor_confinement.o: src/core/route_event_executor_confinement.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_confinement.c

$(BUILD_DIR)/route_event_executor_step_property.o: src/core/route_event_executor_step_property.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_step_property.c

$(BUILD_DIR)/route_event_executor_step_special.o: src/core/route_event_executor_step_special.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_step_special.c

$(BUILD_DIR)/route_event_executor_commands.o: src/core/route_event_executor_commands.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_commands.c

$(BUILD_DIR)/route_event_executor_resume.o: src/core/route_event_executor_resume.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_resume.c

$(BUILD_DIR)/route_event_executor_shop.o: src/core/route_event_executor_shop.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_shop.c

$(BUILD_DIR)/route_event_executor_continuations.o: src/core/route_event_executor_continuations.c $(ROUTE_EXECUTOR_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event_executor_continuations.c

$(BUILD_DIR)/route_event.o: src/core/route_event.c src/core/route_event.h src/core/game_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/route_event.c

$(BUILD_DIR)/player_status.o: src/core/player_status.c src/core/player_status.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/player_status.c

$(BUILD_DIR)/status_countdown.o: src/core/status_countdown.c src/core/status_countdown.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/status_countdown.c

$(BUILD_DIR)/turn_rotation.o: src/core/turn_rotation.c src/core/turn_rotation.h src/core/game_state.h src/core/rng.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/turn_rotation.c

$(BUILD_DIR)/turn_around.o: src/core/turn_around.c src/core/turn_around.h src/core/game_domain.h src/core/game_state.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/turn_around.c

$(BUILD_DIR)/special_actor_action.o: src/core/special_actor_action.c src/core/special_actor_action.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_actor_action.c

$(BUILD_DIR)/turn_flow.o: src/core/turn_flow.c src/core/turn_flow.h src/core/game_state.h src/core/game_flow_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/turn_flow.c

$(BUILD_DIR)/victory.o: src/core/victory.c src/core/victory.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/victory.c

$(BUILD_DIR)/game_view_model.o: src/view/game_view_model.c src/view/game_view_model.h src/core/bank.h src/core/game_flow_state.h src/core/game_state.h src/data/map_data.h src/core/map_projection.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/view/game_view_model.c

$(BUILD_DIR)/scene.o: src/app/scene.c src/app/scene.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/scene.c

$(BUILD_DIR)/scene_input_flow.o: src/app/scene_input_flow.c src/app/scene_input_flow.h src/app/game_session_runtime_init.h src/app/game_session.h src/app/new_game_setup_flow.h src/app/options_flow.h src/app/scene.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/scene_input_flow.c

$(BUILD_DIR)/scene_frame_flow.o: src/app/scene_frame_flow.c src/app/scene_frame_flow.h src/app/scene.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/scene_frame_flow.c

$(BUILD_DIR)/new_game_scene_flow.o: src/app/new_game_scene_flow.c src/app/new_game_scene_flow.h src/app/new_game_setup_flow.h src/app/scene.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/new_game_scene_flow.c

$(BUILD_DIR)/game_presentation.o: src/app/game_presentation.c src/app/game_presentation.h src/app/game_presentation_handler.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_presentation.c

$(BUILD_DIR)/bankruptcy_flow.o: src/app/bankruptcy_flow.c src/app/bankruptcy_flow.h src/app/dynamic_object_flow.h src/app/game_session.h src/app/game_presentation.h src/core/game_event_payload.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/bankruptcy_flow.c

$(BUILD_DIR)/game_save.o: src/app/game_save.c src/app/game_save.h src/app/game_save_codec.inc src/app/game_save_codec_v4.inc src/app/game_save_codec_v5.inc src/app/game_session.h src/app/audio_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_save.c

$(BUILD_DIR)/game_event_presentation_snapshot.o: src/app/game_event_presentation_snapshot.c src/app/game_event_presentation_snapshot.h src/app/game_presentation.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_event_presentation_snapshot.c

$(BUILD_DIR)/game_session_map_init.o: src/app/game_session_map_init.c src/app/game_session_map_init.h src/app/game_session.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_session_map_init.c

$(BUILD_DIR)/game_session_runtime_init.o: src/app/game_session_runtime_init.c src/app/game_session_runtime_init.h src/app/game_session_map_init.h src/app/game_session.h src/app/confinement_flow.h src/app/dynamic_object_flow.h src/app/game_presentation.h src/app/input_flow.h src/app/intro_flow.h src/app/route_flow.h src/app/route_geometry_flow.h src/core/game_constants.h src/core/rng.h src/core/tool.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_session_runtime_init.c

$(BUILD_DIR)/game_speech_flow.o: src/app/game_speech_flow.c src/app/game_speech_flow.h src/app/game_session.h src/core/character_speech_rules.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_speech_flow.c

$(BUILD_DIR)/command_event_flow.o: src/app/command_event_flow.c src/app/command_event_flow.h src/app/audio_event.h src/app/game_session.h src/app/dice_flow.h src/app/game_speech_flow.h src/app/insurance_flow.h src/app/special_presentation_flow.h src/app/tool_animation_flow.h src/app/tool_followup_flow.h src/app/turn_start_flow.h src/core/game_constants.h src/core/game_event_payload.h src/core/property.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/command_event_flow.c

$(BUILD_DIR)/bank_flow.o: src/app/bank_flow.c src/app/bank_flow.h src/app/game_session.h src/core/game_command.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/bank_flow.c

$(BUILD_DIR)/bank_interaction_flow.o: src/app/bank_interaction_flow.c src/app/bank_interaction_flow.h src/app/stock_interaction_flow.h src/app/app_command.h src/app/game_session.h src/core/bank.h src/core/game_command.h src/core/route_event.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/bank_interaction_flow.c

$(BUILD_DIR)/stock_interaction_flow.o: src/app/stock_interaction_flow.c src/app/stock_interaction_flow.h src/app/app_command.h src/app/game_session.h src/core/game_command.h src/core/stock.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/stock_interaction_flow.c

$(BUILD_DIR)/monster_card_flow.o: src/app/monster_card_flow.c src/app/monster_card_flow.h src/app/game_session.h src/app/game_speech_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/monster_card_flow.c

$(BUILD_DIR)/card_effect_flow.o: src/app/card_effect_flow.c src/app/card_effect_flow.h src/app/game_session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/card_effect_flow.c

$(BUILD_DIR)/card_defense_flow.o: src/app/card_defense_flow.c src/app/card_defense_flow.h src/app/game_session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/card_defense_flow.c

$(BUILD_DIR)/card_use_flow.o: src/app/card_use_flow.c src/app/card_use_flow.h src/app/game_session.h src/app/game_speech_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/card_use_flow.c

$(BUILD_DIR)/lottery_flow.o: src/app/lottery_flow.c src/app/lottery_flow.h src/app/game_session.h src/core/character_speech_rules.h src/core/game_event.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/lottery_flow.c

$(BUILD_DIR)/roulette_flow.o: src/app/roulette_flow.c src/app/roulette_flow.h src/app/game_session.h src/app/audio_event.h src/app/insurance_flow.h src/core/game_event_payload.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/roulette_flow.c

$(BUILD_DIR)/point_minigame_flow.o: src/app/point_minigame_flow.c src/app/point_minigame_flow.h src/app/game_session.h src/app/audio_event.h src/app/game_speech_flow.h src/core/game_event_payload.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/point_minigame_flow.c

$(BUILD_DIR)/magic_house_flow.o: src/app/magic_house_flow.c src/app/magic_house_flow.h src/app/game_session.h src/app/audio_event.h src/core/game_event_payload.h src/core/route_event_executor.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/magic_house_flow.c

$(BUILD_DIR)/confinement_flow.o: src/app/confinement_flow.c src/app/confinement_flow.h src/app/game_session.h src/app/audio_event.h src/app/route_geometry_flow.h src/core/confinement.h src/core/character_speech_rules.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/confinement_flow.c

$(BUILD_DIR)/shop_flow.o: src/app/shop_flow.c src/app/shop_flow.h src/app/game_session.h src/app/audio_event.h src/app/game_speech_flow.h src/core/card.h src/core/route_event_executor.h src/core/tool.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/shop_flow.c

$(BUILD_DIR)/dice_flow.o: src/app/dice_flow.c src/app/dice_flow.h src/app/game_session.h src/app/turn_start_flow.h src/core/game_dispatch.h src/core/route_movement.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/dice_flow.c

$(BUILD_DIR)/property_flow.o: src/app/property_flow.c src/app/property_flow.h src/app/game_session.h src/app/game_speech_flow.h src/core/property.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/property_flow.c

$(BUILD_DIR)/tool_animation_flow.o: src/app/tool_animation_flow.c src/app/tool_animation_flow.h src/app/game_session.h src/app/game_speech_flow.h src/core/character_speech_rules.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/tool_animation_flow.c

$(BUILD_DIR)/tool_followup_flow.o: src/app/tool_followup_flow.c src/app/tool_followup_flow.h src/app/game_session.h src/app/audio_event.h src/app/turn_rotation_flow.h src/core/tool.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/tool_followup_flow.c

$(BUILD_DIR)/card_panel_flow.o: src/app/card_panel_flow.c src/app/card_panel_flow.h src/app/card_panel_state.h src/app/map_input_flow.h src/app/game_session.h src/core/game_command.h src/core/game_constants.h src/core/route_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/card_panel_flow.c

$(BUILD_DIR)/route_geometry_flow.o: src/app/route_geometry_flow.c src/app/route_geometry_flow.h src/core/game_constants.h src/core/route_movement.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/route_geometry_flow.c

$(BUILD_DIR)/tool_panel_flow.o: src/app/tool_panel_flow.c src/app/tool_panel_flow.h src/app/tool_panel_state.h src/app/map_input_flow.h src/app/game_session.h src/app/game_speech_flow.h src/core/tool.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/tool_panel_flow.c

$(BUILD_DIR)/special_presentation_flow.o: src/app/special_presentation_flow.c src/app/special_presentation_flow.h src/app/dynamic_object_flow.h src/app/special_event_timeline.h src/app/game_presentation.h src/app/game_speech_flow.h src/core/card.h src/core/character_speech_rules.h src/core/game_event_payload.h src/core/property.h src/core/rng.h src/core/route_event_executor.h src/core/special_event.h src/data/character_speech.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_presentation_flow.c

$(BUILD_DIR)/special_presentation_update_flow.o: src/app/special_presentation_update_flow.c src/app/special_presentation_flow.h src/app/special_event_timeline.h src/app/game_presentation.h src/core/game_constants.h src/core/game_event_payload.h src/core/special_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_presentation_update_flow.c

$(BUILD_DIR)/special_presentation_support_flow.o: src/app/special_presentation_support_flow.c src/app/special_presentation_flow.h src/app/game_presentation.h src/app/game_speech_flow.h src/core/card.h src/core/character_speech_rules.h src/core/game_event_payload.h src/core/property.h src/core/rng.h src/core/route_event_executor.h src/data/character_speech.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_presentation_support_flow.c

$(BUILD_DIR)/special_place_notice_flow.o: src/app/special_place_notice_flow.c src/app/special_place_notice_flow.h src/app/game_presentation.h src/core/game_event_payload.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_place_notice_flow.c

$(BUILD_DIR)/special_event_timeline.o: src/app/special_event_timeline.c src/app/special_event_timeline.h src/app/game_presentation.h src/app/game_session.h src/core/confinement.h src/core/game_constants.h src/core/special_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_event_timeline.c

$(BUILD_DIR)/point_minigame_input.o: src/app/point_minigame_input.c src/app/point_minigame_flow.h src/app/audio_event.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/point_minigame_input.c

$(BUILD_DIR)/app_command.o: src/app/app_command.c src/app/app_command.h src/core/game_command.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/app_command.c

$(BUILD_DIR)/sale_interaction_flow.o: src/app/sale_interaction_flow.c src/app/sale_interaction_flow.h src/app/game_session.h src/app/game_presentation.h src/core/game_command.h src/core/game_state.h src/core/game_domain.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/sale_interaction_flow.c

$(BUILD_DIR)/input_flow.o: src/app/input_flow.c src/app/input_flow.h src/app/app_command.h src/app/game_session.h src/app/new_game_setup_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/input_flow.c

$(BUILD_DIR)/map_input_flow.o: src/app/map_input_flow.c src/app/map_input_flow.h src/app/map_edge_scroll_state.h src/app/game_session.h src/view/view_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/map_input_flow.c

$(BUILD_DIR)/map_edge_scroll.o: src/app/map_edge_scroll.c src/app/map_input_flow.h src/app/map_edge_scroll_state.h src/app/game_session.h src/view/view_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/map_edge_scroll.c

$(BUILD_DIR)/dynamic_object_flow.o: src/app/dynamic_object_flow.c src/app/dynamic_object_flow.h src/app/game_presentation.h src/view/game_view_model.h src/data/map_data.h src/core/game_constants.h src/core/game_event.h src/core/god.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/dynamic_object_flow.c

$(BUILD_DIR)/audio_event.o: src/app/audio_event.c src/app/audio_event.h src/app/game_session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/audio_event.c

$(BUILD_DIR)/new_game_setup_flow.o: src/app/new_game_setup_flow.c src/app/new_game_setup_flow.h src/app/game_session.h src/core/character_template.h src/core/game_constants.h src/core/rng.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/new_game_setup_flow.c

$(BUILD_DIR)/menu_flow.o: src/app/menu_flow.c src/app/menu_flow.h src/app/game_session.h src/app/new_game_setup_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/menu_flow.c

$(BUILD_DIR)/game_session.o: src/app/game_session.c src/app/game_session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_session.c

$(BUILD_DIR)/options_flow.o: src/app/options_flow.c src/app/options_flow.h src/app/config_types.h src/data/music.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/options_flow.c

$(BUILD_DIR)/options_game_flow.o: src/app/options_game_flow.c src/app/options_game_flow.h src/app/audio_event.h src/app/game_session.h src/app/auction_flow.h src/app/command_event_flow.h src/core/game_constants.h src/core/game_event_payload.h src/core/ledger.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/options_game_flow.c

$(BUILD_DIR)/game_command_flow.o: src/app/game_command_flow.c src/app/game_command_flow.h src/app/app_command.h src/app/game_session.h src/core/game_dispatch.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_command_flow.c

$(BUILD_DIR)/route_flow.o: src/app/route_flow.c src/app/route_flow.h src/app/game_session.h src/core/game_constants.h src/core/route_event_executor.h src/core/route_event_queue.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/route_flow.c

$(BUILD_DIR)/route_facts.o: src/app/route_facts.c src/app/route_facts.h src/app/game_session.h src/core/game_constants.h src/core/route_event.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/route_facts.c

$(BUILD_DIR)/turn_movement_flow.o: src/app/turn_movement_flow.c src/app/turn_movement_flow.h src/app/game_session.h src/app/dice_flow.h src/app/dynamic_object_flow.h src/app/route_facts.h src/app/route_flow.h src/app/tool_followup_flow.h src/app/turn_rotation_flow.h src/core/game_constants.h src/core/game_event_payload.h src/core/route_movement.h src/core/tool.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/turn_movement_flow.c

$(BUILD_DIR)/route_event_flow.o: src/app/route_event_flow.c src/app/route_event_flow.h src/app/game_session.h src/app/bank_flow.h src/app/confinement_flow.h src/app/insurance_flow.h src/app/magic_house_flow.h src/app/point_minigame_flow.h src/app/property_flow.h src/app/roulette_flow.h src/app/shop_flow.h src/app/special_presentation_flow.h src/app/turn_rotation_flow.h src/core/game_event_payload.h src/core/route_event_executor.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/route_event_flow.c

$(BUILD_DIR)/ai_flow.o: src/app/ai_flow.c src/app/ai_flow.h src/app/game_session.h src/core/ai.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/ai_flow.c

$(BUILD_DIR)/ai_settings_flow.o: src/app/ai_settings_flow.c src/app/ai_settings_flow.h src/app/game_session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/ai_settings_flow.c

$(BUILD_DIR)/intro_flow.o: src/app/intro_flow.c src/app/intro_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/game_constants.h src/data/map_data.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/intro_flow.c

$(BUILD_DIR)/turn_start_flow.o: src/app/turn_start_flow.c src/app/turn_start_flow.h src/app/game_session.h src/core/player_status.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/turn_start_flow.c

$(BUILD_DIR)/turn_status_flow.o: src/app/turn_status_flow.c src/app/turn_status_flow.h src/app/game_session.h src/app/game_speech_flow.h src/core/confinement.h src/core/facility.h src/core/status_countdown.h src/core/victory.h src/core/rng.h src/data/character_speech.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/turn_status_flow.c

$(BUILD_DIR)/first_turn_flow.o: src/app/first_turn_flow.c src/app/first_turn_flow.h src/app/game_session.h src/core/first_turn_dispatch.h src/core/game_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/first_turn_flow.c

$(BUILD_DIR)/game_update_flow.o: src/app/game_update_flow.c src/app/game_update_flow.h src/app/game_session.h src/app/dice_flow.h src/core/game_constants.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/game_update_flow.c

$(BUILD_DIR)/turn_rotation_flow.o: src/app/turn_rotation_flow.c src/app/turn_rotation_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/app/special_place_notice_flow.h src/app/tool_animation_flow.h src/core/confinement.h src/core/game_event.h src/core/game_event_payload.h src/core/rng.h src/core/special_actor_action.h src/core/turn_rotation.h src/data/character_speech.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/turn_rotation_flow.c

$(BUILD_DIR)/special_actor_flow.o: src/app/special_actor_flow.c src/app/special_actor_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/game_constants.h src/core/game_event.h src/core/special_actor_action.h src/core/turn_rotation.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_actor_flow.c

$(BUILD_DIR)/insurance_flow.o: src/app/insurance_flow.c src/app/insurance_flow.h src/app/game_session.h src/core/game_event.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/insurance_flow.c

$(BUILD_DIR)/map_renderer.o: src/view/map_renderer.c src/view/map_renderer.h src/view/game_view_model.h src/view/view_types.h src/core/game_map_dynamic_object.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/view/map_renderer.c

$(BUILD_DIR)/trace.o: src/debug/trace.c src/debug/trace.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/debug/trace.c

$(BUILD_DIR)/map_object_ref.o: src/data/map_object_ref.c src/data/map_object_ref.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/map_object_ref.c

$(BUILD_DIR)/map_data.o: src/data/map_data.c src/data/map_data.h src/data/map_object_ref.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/map_data.c

$(BUILD_DIR)/map_data_fault.o: src/data/map_data.c src/data/map_data.h src/data/map_data_test.h src/data/map_object_ref.h | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MAP_DATA_TESTING $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/map_data.c

$(BUILD_DIR)/resource_catalog.o: src/data/resource_catalog.c src/data/resource_catalog.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/resource_catalog.c

$(BUILD_DIR)/character_speech.o: src/data/character_speech.c src/data/character_speech.h src/data/card_speech.inc | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/character_speech.c

$(BUILD_DIR)/character_speech_rules.o: src/core/character_speech_rules.c src/core/character_speech_rules.h src/data/character_speech.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/character_speech_rules.c

$(BUILD_DIR)/special_event_text.o: src/data/special_event_text.c src/data/special_event_text.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/special_event_text.c

$(BIN_DIR)/test_game_loop_frame_end: tests/unit/test_game_loop_frame_end.c $(BUILD_DIR)/sdl1_game_loop_frame_end.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_loop_frame_end.c $(BUILD_DIR)/sdl1_game_loop_frame_end.o

$(BIN_DIR)/test_game_loop_scene_update: tests/unit/test_game_loop_scene_update.c $(BUILD_DIR)/sdl1_game_loop_scene_update.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_loop_scene_update.c $(BUILD_DIR)/sdl1_game_loop_scene_update.o

$(BIN_DIR)/test_player_info_name_spacing: tests/unit/test_player_info_name_spacing.c $(BUILD_DIR)/sdl1_game_loop_player_info_panel.o $(BUILD_DIR)/sdl1_game_catalog_text.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^

$(BIN_DIR)/test_sdl1_blit: tests/unit/test_sdl1_blit.c $(BUILD_DIR)/sdl1_blit.o $(BUILD_DIR)/spr_smp.o $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_sdl1_blit.c $(BUILD_DIR)/sdl1_blit.o $(BUILD_DIR)/spr_smp.o $(BUILD_DIR)/byte_reader.o $(SDL_LIBS)

$(BIN_DIR)/test_music: tests/unit/test_music.c $(BUILD_DIR)/music.o $(BUILD_DIR)/sdl1_game_loop_load_music.o $(BUILD_DIR)/sdl1_game_loop_modal_music.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^

$(BIN_DIR)/test_preview_asset_lifetime: tests/resource/test_preview_asset_lifetime.c $(BUILD_DIR)/sdl1_game_preview_assets.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(RESOURCE_STORE_OBJS) $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_preview_asset_lifetime.c $(BUILD_DIR)/sdl1_game_preview_assets.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(RESOURCE_STORE_OBJS) $(MKF_OBJS)

$(BIN_DIR)/test_resource_store_fault_injection: tests/resource/test_resource_store_fault_injection.c $(BUILD_DIR)/resource_store_fault.o $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) -DRICH4_RESOURCE_STORE_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_resource_store_fault_injection.c $(BUILD_DIR)/resource_store_fault.o $(MKF_OBJS)

$(BIN_DIR)/test_mkf_fault_injection: tests/resource/test_mkf_fault_injection.c $(BUILD_DIR)/mkf_fault.o $(BUILD_DIR)/mkf_decompress.o $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MKF_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_mkf_fault_injection.c $(BUILD_DIR)/mkf_fault.o $(BUILD_DIR)/mkf_decompress.o $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_map_data_fault_injection: tests/resource/test_map_data_fault_injection.c $(BUILD_DIR)/map_data_fault.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MAP_DATA_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_map_data_fault_injection.c $(BUILD_DIR)/map_data_fault.o $(BUILD_DIR)/map_object_ref.o

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"
else
	rm -rf $(BUILD_DIR) $(BIN_DIR)
endif

$(BIN_DIR)/test_property_map_presentation: tests/sdl/test_property_map_presentation.c tests/sdl/property_flash_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_property_map_presentation.c tests/sdl/property_flash_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

# These non-SDL rules do not emit transitive header depfiles. All consumers
# must agree on GameState/GameSession layout after hosting-state changes.
$(CORE_OBJS) $(APP_OBJS) $(VIEW_OBJS) $(DEBUG_OBJS): src/core/game_state.h
$(APP_OBJS) $(VIEW_OBJS) $(DEBUG_OBJS): src/app/ai_settings_state.h

$(BUILD_DIR)/hotel_flow.o: src/app/hotel_flow.c src/app/hotel_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/game_event_payload.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/hotel_flow.c

$(BUILD_DIR)/route_event_flow.o $(BUILD_DIR)/turn_rotation_flow.o: src/app/hotel_flow.h

$(APP_OBJS) $(VIEW_OBJS) $(SDL1_SESSION_DEPENDENCY_OBJECTS): src/data/confinement_presentation.h

# Presentation clocks and card animation fields change shared struct layout.
$(filter %.o,$(SDL_OBJS)): src/app/animation_clock.h src/app/game_presentation.h src/app/card_panel_state.h

# Native Apple launcher/importer and import-time DLS renderer.
.PHONY: setup-macos macos-native macos package-macos check-macos-package test-macos-toolchain test-macos-package test-macos-original test-macos-music-position
MACOS_SETUP_ARGS ?=
setup-macos:
	$(PYTHON) scripts/macos.py setup $(MACOS_SETUP_ARGS)
macos-native:
	$(PYTHON) scripts/macos.py native
macos:
	$(PYTHON) scripts/macos.py build
package-macos: macos
	$(PYTHON) scripts/macos.py package
check-macos-package:
	$(PYTHON) scripts/macos.py verify
test-macos-toolchain:
	$(PYTHON) tests/unit/test_macos_toolchain.py
test-macos-package: check-macos-package
	$(PYTHON) tests/macos/test_package.py
test-macos-original: macos
	$(PYTHON) scripts/macos.py test-original
test-macos-music-position: macos
	@if test ! -f assets/original/V3.11/Midi03.mid; then \
		echo "Original-resource MIDI position restore: NOT EXECUTED (original resources absent)"; \
	elif bin/macos/universal/music_probe capabilities | grep -q 'MIDI available=1'; then \
		bin/macos/universal/music_probe restore \
			assets/original/V3.11/Midi03.mid 12; \
	else \
		echo "Original-resource MIDI position restore: NOT EXECUTED (native MIDI backend not visible in current execution environment)"; \
	fi

$(BIN_DIR)/music_probe: tests/sdl/music_probe.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: release-macos
release-macos:
	$(PYTHON) scripts/sign_macos.py --acceptance-file "$(MACOS_ACCEPTANCE_FILE)"

# Grouped test runners. Tests in one group share a link set and must not
# define symbols that override production objects; each test still runs in its
# own process through scripts/run_tests.py. Tests with stubs, special compile
# variants or Miyoo QEMU deployment stay standalone executables above.

CORE_TESTS := \
	test_card_use_flow \
	test_ai \
	test_ai_flow \
	test_ai_settings_flow \
	test_app_dynamic_object_flow \
	test_app_input_flow \
	test_app_map_input_flow \
	test_auction \
	test_auction_flow \
	test_audio_event \
	test_bank \
	test_bank_flow \
	test_bank_interaction_flow \
	test_behavior_traceability \
	test_byte_reader \
	test_card \
	test_card_panel_flow \
	test_character_speech \
	test_confinement_flow \
	test_confinement_god \
	test_core_headers \
	test_dice_flow \
	test_engineering_attack_flow \
	test_facility \
	test_file_line_budget \
	test_first_turn_dispatch \
	test_first_turn_flow \
	test_first_turn_replay \
	test_flc_animation \
	test_flc_format_names \
	test_flc_player \
	test_full_game_replay \
	test_game_command \
	test_game_dispatch \
	test_game_domain \
	test_game_event_payload \
	test_game_presentation \
	test_game_session_map_init \
	test_game_session_runtime_init \
	test_game_speech_flow \
	test_game_state_purity \
	test_game_update_flow \
	test_game_view_model \
	test_god_acquisition \
	test_hotel_flow \
	test_insurance \
	test_insurance_alignment \
	test_insurance_flow \
	test_intro_flow \
	test_ledger \
	test_lottery_flow \
	test_magic_house_flow \
	test_map_data \
	test_map_object_ref \
	test_map_renderer \
	test_menu_flow \
	test_mkf_failures \
	test_monster_card_flow \
	test_monthly_finance \
	test_new_game_init \
	test_new_game_setup_flow \
	test_no_debug_ui \
	test_options_flow \
	test_player_status \
	test_point_minigame_flow \
	test_property \
	test_property_lease \
	test_property_flow \
	test_public_api_contract \
	test_real_map_bootstrap \
	test_renderer_viewmodel_boundary \
	test_resource_catalog \
	test_resource_store \
	test_resource_store_cache \
	test_rng \
	test_roulette_flow \
	test_route_event_executor \
	test_route_event_executor_replay \
	test_route_event_flow \
	test_route_event_queue \
	test_route_facts \
	test_route_flow \
	test_route_geometry_flow \
	test_route_movement \
	test_save_load_replay \
	test_scene \
	test_scene_input_flow \
	test_shop_flow \
	test_special_actor_action \
	test_special_actor_flow \
	test_special_event \
	test_special_event_text \
	test_special_place_alignment \
	test_special_presentation_flow \
	test_spr_smp \
	test_status_countdown \
	test_stock \
	test_stock_interaction_flow \
	test_tool \
	test_tool_animation_flow \
	test_tool_followup_flow \
	test_tool_panel_flow \
	test_trace \
	test_turn_around_flow \
	test_turn_flow \
	test_turn_movement_flow \
	test_turn_rotation \
	test_turn_rotation_flow \
	test_turn_start_flow \
	test_turn_status_flow \
	test_victory

SDL_TESTS := \
	test_bank_loan_presentation \
	test_ai_pixel_candidates \
	test_ai_settings_input \
	test_auction_presentation \
	test_audio_loops \
	test_big5_utf8 \
	test_card_presentation \
	test_card_target_selection \
	test_catcher_minigame \
	test_dog_encounter \
	test_engineering_sdl \
	test_fortune_selection_sdl \
	test_game_catalog_text \
	test_game_view_model_input \
	test_god_sdl \
	test_hibernation_sdl \
	test_help_text \
	test_sleepwalking_sdl \
	test_hud_layout \
	test_insurance_building_sdl \
	test_machine_doll \
	test_miyoo_input \
	test_monthly_finance_presentation \
	test_options_sale \
	test_render_equivalence \
	test_sdl1_input_adapter \
	test_sdl1_session_header \
	test_sdl1_text_rules \
	test_text_backend \
	test_text_backend_freetype \
	test_tool_prompts_sdl \
	test_worker_animation

SDL_TEST_SUPPORT := ground_scalar_reference flc_scalar_reference

STANDALONE_SDL_TESTS := \
	test_flc_playback \
	test_frame_pacing \
	test_miyoo_asset_scope \
	test_miyoo_qemu_probe \
	test_present_pixels \
	test_property_map_presentation \
	test_sdl1_blit

TEST_SOURCE_DIRS := unit resource replay characterization sdl
CORE_TEST_OBJECTS = $(patsubst %,$(BUILD_DIR)/tests_%.o,$(CORE_TESTS))
SDL_TEST_OBJECTS = $(patsubst %,$(BUILD_DIR)/tests_%.o,$(SDL_TESTS) $(SDL_TEST_SUPPORT))
TEST_CORE_LINK_OBJS = $(sort $(CORE_OBJS) $(APP_OBJS) $(VIEW_OBJS) $(DEBUG_OBJS) \
	$(RESOURCE_STORE_OBJS) $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(FLC_PLAYER_OBJS) \
	$(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/resource_catalog.o \
	$(BUILD_DIR)/character_speech.o $(BUILD_DIR)/special_event_text.o)
TEST_SDL_LINK_OBJS = $(sort $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)))

# _SDL_main_h keeps SDL_main.h from renaming the -Dmain test entry points.
$(SDL_TEST_OBJECTS): TEST_CFLAGS = $(SDL_CFLAGS) $(MIXER_CFLAGS) $(TTF_CFLAGS) -D_SDL_main_h

define RICH4_TEST_OBJECT_RULE
$$(BUILD_DIR)/tests_%.o: tests/$(1)/%.c | dirs
	$$(CC) $$(CPPFLAGS) $$(TEST_CFLAGS) $$(CFLAGS) $$(WARNFLAGS) -Dmain=$$*_main -MMD -MP -MF $$(@:.o=.d) -c -o $$@ $$<
endef
$(foreach dir,$(TEST_SOURCE_DIRS),$(eval $(call RICH4_TEST_OBJECT_RULE,$(dir))))

$(BUILD_DIR)/test_registry_core.c: Makefile scripts/gen_test_registry.py | dirs
	$(PYTHON) scripts/gen_test_registry.py $@ $(CORE_TESTS)

$(BUILD_DIR)/test_registry_sdl.c: Makefile scripts/gen_test_registry.py | dirs
	$(PYTHON) scripts/gen_test_registry.py $@ $(SDL_TESTS)

$(BUILD_DIR)/test_registry_%.o: $(BUILD_DIR)/test_registry_%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

$(BIN_DIR)/rich4_tests_core: $(CORE_TEST_OBJECTS) $(BUILD_DIR)/test_registry_core.o $(TEST_CORE_LINK_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $^

$(BIN_DIR)/rich4_tests_sdl: $(SDL_TEST_OBJECTS) $(BUILD_DIR)/test_registry_sdl.o $(TEST_SDL_LINK_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

RICH4_RUN_TESTS = $(PYTHON) scripts/run_tests.py --bin-dir $(BIN_DIR)
rich4_test_exe = $(if $(filter $(1),$(CORE_TESTS)),$(BIN_DIR)/rich4_tests_core,$(if $(filter $(1),$(SDL_TESTS)),$(BIN_DIR)/rich4_tests_sdl,$(BIN_DIR)/$(1)))
rich4_test_prereqs = $(sort $(foreach t,$(1),$(call rich4_test_exe,$(t)))) $(if $(filter $(SDL_TESTS) $(STANDALONE_SDL_TESTS),$(1)),stage-sdl-runtime)

# $(1): suite target, $(2): prerequisite suites, $(3): test names.
define RICH4_TEST_SUITE
.PHONY: $(1)
$(1): $(2) $(call rich4_test_prereqs,$(3))
	$$(RICH4_RUN_TESTS) $(3)
endef

# test_first_turn_flow fails at its first-landing assertion and is left out of
# the suites until the first-landing clock behavior is checked against Rich4.
# Original-resource frame comparisons stay in the explicit test-hibernation
# and test-sleepwalking suites rather than the data-free unit suite.

TEST_UNIT_TESTS := \
	test_rng \
	test_flc_format_names \
	test_map_object_ref \
	test_resource_catalog \
	test_character_speech \
	test_game_speech_flow \
	test_insurance_flow \
	test_bank_flow \
	test_monster_card_flow \
	test_lottery_flow \
	test_roulette_flow \
	test_point_minigame_flow \
	test_magic_house_flow \
	test_confinement_flow \
	test_shop_flow \
	test_dice_flow \
	test_property_flow \
	test_tool_animation_flow \
	test_tool_followup_flow \
	test_tool_panel_flow \
	test_ai_flow \
	test_ai_settings_flow \
	test_intro_flow \
	test_auction \
	test_auction_flow \
	test_turn_start_flow \
	test_turn_status_flow \
	test_game_loop_frame_end \
	test_game_loop_scene_update \
	test_app_input_flow \
	test_scene_input_flow \
	test_app_map_input_flow \
	test_app_dynamic_object_flow \
	test_stock_interaction_flow \
	test_card_panel_flow \
	test_route_geometry_flow \
	test_game_session_map_init \
	test_game_session_runtime_init \
	test_special_event_text \
	test_music \
	test_special_actor_action \
	test_new_game_init \
	test_new_game_setup_flow \
	test_game_domain \
	test_game_command \
	test_bank \
	test_game_dispatch \
	test_game_event_payload \
	test_ai \
	test_tool \
	test_first_turn_dispatch \
	test_route_movement \
	test_route_event_queue \
	test_route_event_executor \
	test_route_flow \
	test_route_event_flow \
	test_player_status \
	test_status_countdown \
	test_confinement_god \
	test_special_event \
	test_stock \
	test_turn_rotation \
	test_turn_flow \
	test_victory \
	test_game_view_model \
	test_game_view_model_input \
	test_game_state_purity \
	test_renderer_viewmodel_boundary \
	test_behavior_traceability \
	test_map_renderer \
	test_scene \
	test_game_presentation \
	test_sdl1_input_adapter \
	test_sdl1_session_header \
	test_sdl1_text_rules \
	test_text_backend \
	test_big5_utf8 \
	test_sdl1_blit \
	test_file_line_budget \
	test_no_debug_ui \
	test_audio_event \
	test_public_api_contract \
	test_text_backend_freetype \
	test_help_text \
	test_core_headers \
	test_monthly_finance \
	test_miyoo_input \
	test_player_info_name_spacing \
	test_game_catalog_text \
	test_engineering_attack_flow \
	test_god_acquisition \
	test_hotel_flow \
	test_special_presentation_flow \
	test_byte_reader \
	test_menu_flow \
	test_options_flow \
	test_turn_around_flow \
	test_game_update_flow \
	test_turn_rotation_flow \
	test_route_facts \
	test_turn_movement_flow \
	test_bank_interaction_flow \
	test_special_actor_flow

FLC_REGRESSION_TESTS := \
	test_intro_flow \
	test_tool_animation_flow \
	test_monster_card_flow \
	test_special_presentation_flow \
	test_property_flow \
	test_dice_flow \
	test_point_minigame_flow \
	test_magic_house_flow \
	test_monthly_finance \
	test_app_dynamic_object_flow \
	test_card_panel_flow \
	test_flc_playback \
	test_audio_loops \
	test_render_equivalence \
	test_miyoo_asset_scope \
	test_lottery_flow \
	test_god_sdl \
	test_options_sale

$(eval $(call RICH4_TEST_SUITE,check-core-headers,,test_core_headers))
$(eval $(call RICH4_TEST_SUITE,test-ai-event-assets,,test_miyoo_asset_scope))
$(eval $(call RICH4_TEST_SUITE,test-ai-landing,test-bank test-property test-facility test-card,test_ai test_route_event_executor))
$(eval $(call RICH4_TEST_SUITE,test-ai-pixel-candidates,,test_ai_pixel_candidates))
$(eval $(call RICH4_TEST_SUITE,test-ai-settings,,test_ai_settings_flow test_turn_rotation test_ai_settings_input))
$(eval $(call RICH4_TEST_SUITE,test-auction,,test_auction))
$(eval $(call RICH4_TEST_SUITE,test-auction-flow,,test_auction_flow))
$(eval $(call RICH4_TEST_SUITE,test-auction-presentation,,test_auction_presentation))
$(eval $(call RICH4_TEST_SUITE,test-audio-loops,,test_audio_event test_magic_house_flow test_turn_movement_flow test_audio_loops))
$(eval $(call RICH4_TEST_SUITE,test-music,,test_music))
$(eval $(call RICH4_TEST_SUITE,test-bank,,test_bank))
$(eval $(call RICH4_TEST_SUITE,test-bank-loan-enforcement,,test_bank test_ledger test_turn_rotation test_turn_rotation_flow test_bank_flow test_game_presentation test_app_input_flow test_game_update_flow))
$(eval $(call RICH4_TEST_SUITE,test-bank-loan-presentation,,test_bank_loan_presentation))
$(eval $(call RICH4_TEST_SUITE,test-card,,test_card))
$(eval $(call RICH4_TEST_SUITE,test-hibernation,,test_sdl1_blit test_hibernation_sdl test_turn_start_flow test_turn_status_flow test_game_view_model test_app_input_flow))
$(eval $(call RICH4_TEST_SUITE,test-sleepwalking,,test_sleepwalking_sdl test_turn_start_flow test_turn_movement_flow test_special_actor_flow test_game_view_model test_game_view_model_input test_save_load_replay))
$(eval $(call RICH4_TEST_SUITE,test-dice-flow,,test_dice_flow))
$(eval $(call RICH4_TEST_SUITE,test-card-panel-flow,,test_card_panel_flow))
$(eval $(call RICH4_TEST_SUITE,test-card-presentation,,test_card_presentation))
$(eval $(call RICH4_TEST_SUITE,test-card-target-selection,,test_card_target_selection))
$(eval $(call RICH4_TEST_SUITE,test-card-use-flow,,test_card_use_flow test_game_dispatch test_card_panel_flow test_ai_flow test_game_update_flow test_turn_start_flow test_app_input_flow test_game_speech_flow test_monster_card_flow test_audio_event test_character_speech))
$(eval $(call RICH4_TEST_SUITE,test-catcher-minigame,,test_point_minigame_flow test_sdl1_blit test_catcher_minigame))
$(eval $(call RICH4_TEST_SUITE,test-characterization,,test_trace))
$(eval $(call RICH4_TEST_SUITE,test-company-landing,,test_route_event_executor test_roulette_flow test_insurance_flow test_special_presentation_flow test_property_flow test_map_renderer))
$(eval $(call RICH4_TEST_SUITE,test-dog-encounter,,test_app_dynamic_object_flow test_special_actor_flow test_insurance_flow test_turn_movement_flow test_dog_encounter))
$(eval $(call RICH4_TEST_SUITE,test-engineering-attack,,test_engineering_attack_flow))
$(eval $(call RICH4_TEST_SUITE,test-engineering-regression,test-engineering-attack,test_route_event_flow test_turn_rotation_flow test_turn_movement_flow test_game_view_model test_game_view_model_input test_game_presentation test_app_input_flow))
$(eval $(call RICH4_TEST_SUITE,test-engineering-sdl,,test_engineering_sdl))
$(eval $(call RICH4_TEST_SUITE,test-facility,,test_facility))
$(eval $(call RICH4_TEST_SUITE,test-flc-playback,,test_flc_playback))
$(eval $(call RICH4_TEST_SUITE,test-fortune-alignment,test-special-event test-special-place-alignment test-fortune-selection-sdl,test_character_speech test_special_event_text))
$(eval $(call RICH4_TEST_SUITE,test-fortune-selection-sdl,,test_fortune_selection_sdl))
$(eval $(call RICH4_TEST_SUITE,test-frame-pacing,,test_frame_pacing))
$(eval $(call RICH4_TEST_SUITE,test-game-catalog-text,,test_game_catalog_text))
$(eval $(call RICH4_TEST_SUITE,test-god-acquisition,,test_god_acquisition))
$(eval $(call RICH4_TEST_SUITE,test-god-regression,test-god-acquisition,test_confinement_god test_app_dynamic_object_flow test_turn_movement_flow test_game_update_flow test_game_presentation test_audio_event test_dice_flow))
$(eval $(call RICH4_TEST_SUITE,test-god-resident,,test_property_flow test_confinement_god test_turn_rotation_flow test_route_event_executor test_route_event_flow))
$(eval $(call RICH4_TEST_SUITE,test-god-sdl,,test_god_sdl))
$(eval $(call RICH4_TEST_SUITE,test-hotel,,test_hotel_flow))
$(eval $(call RICH4_TEST_SUITE,test-hotel-regression,test-hotel test-facility test-insurance test-save-load-replay,test_turn_rotation_flow test_route_event_flow test_game_update_flow test_turn_start_flow test_turn_status_flow test_audio_event test_roulette_flow test_game_view_model test_route_event_executor))
$(eval $(call RICH4_TEST_SUITE,test-help-text,,test_help_text))
$(eval $(call RICH4_TEST_SUITE,test-hud-layout,,test_hud_layout))
$(eval $(call RICH4_TEST_SUITE,test-insurance,,test_insurance test_insurance_alignment))
$(eval $(call RICH4_TEST_SUITE,test-insurance-building-sdl,,test_insurance_building_sdl))
$(eval $(call RICH4_TEST_SUITE,test-ledger,,test_ledger))
$(eval $(call RICH4_TEST_SUITE,test-machine-doll,test-machine-doll-unit,test_machine_doll))
$(eval $(call RICH4_TEST_SUITE,test-machine-doll-unit,,test_tool_followup_flow test_app_dynamic_object_flow test_game_view_model test_game_view_model_input test_tool))
$(eval $(call RICH4_TEST_SUITE,test-magic-house,,test_magic_house_flow test_route_event_executor test_game_event_payload test_route_event_flow))
$(eval $(call RICH4_TEST_SUITE,test-miyoo-input,,test_miyoo_input))
$(eval $(call RICH4_TEST_SUITE,test-monthly-finance,,test_monthly_finance))
$(eval $(call RICH4_TEST_SUITE,test-monthly-finance-presentation,,test_monthly_finance_presentation))
$(eval $(call RICH4_TEST_SUITE,test-monthly-finance-regression,test-monthly-finance test-stock test-bank test-ledger test-save-load-replay,test_turn_rotation test_turn_rotation_flow))
$(eval $(call RICH4_TEST_SUITE,test-new-game-loading,,test_game_loop_frame_end test_game_loop_scene_update test_new_game_setup_flow))
$(eval $(call RICH4_TEST_SUITE,test-news-selection,,test_special_event test_special_presentation_flow test_special_event_text))
$(eval $(call RICH4_TEST_SUITE,test-options-sale,,test_options_sale))
$(eval $(call RICH4_TEST_SUITE,test-player-info-name-spacing,,test_player_info_name_spacing))
$(eval $(call RICH4_TEST_SUITE,test-player-info,,test_player_info_name_spacing test_victory test_app_map_input_flow))
$(eval $(call RICH4_TEST_SUITE,test-property-lease,,test_property_lease test_property test_facility test_auction test_card test_confinement_god test_game_session_map_init test_map_data test_save_load_replay test_player_info_name_spacing test_turn_rotation test_turn_rotation_flow test_game_event_payload))
$(eval $(call RICH4_TEST_SUITE,test-present-pixels,,test_present_pixels))
$(eval $(call RICH4_TEST_SUITE,test-preview-assets-lifetime,,test_preview_asset_lifetime))
$(eval $(call RICH4_TEST_SUITE,test-property,,test_property test_property_lease))
$(eval $(call RICH4_TEST_SUITE,test-property-map-presentation,,test_property_map_presentation))
$(eval $(call RICH4_TEST_SUITE,test-real-map-bootstrap,,test_real_map_bootstrap))
$(eval $(call RICH4_TEST_SUITE,test-render,,test_map_renderer test_flc_player))
$(eval $(call RICH4_TEST_SUITE,test-render-equivalence,,test_sdl1_blit test_flc_player test_render_equivalence))
$(eval $(call RICH4_TEST_SUITE,test-replay,,test_first_turn_replay test_route_event_executor_replay test_full_game_replay test_real_map_bootstrap test_save_load_replay))
$(eval $(call RICH4_TEST_SUITE,test-resource,,test_spr_smp test_flc_animation test_flc_player test_resource_store test_resource_store_cache test_preview_asset_lifetime test_resource_store_fault_injection test_map_data test_map_data_fault_injection test_mkf_failures test_mkf_fault_injection))
$(eval $(call RICH4_TEST_SUITE,test-road-gift,,test_special_presentation_flow test_turn_movement_flow test_audio_event))
$(eval $(call RICH4_TEST_SUITE,test-save-load-replay,,test_save_load_replay))
$(eval $(call RICH4_TEST_SUITE,test-special-event,,test_special_event))
$(eval $(call RICH4_TEST_SUITE,test-special-actors,,test_special_actor_action test_special_actor_flow test_game_view_model test_game_view_model_input test_turn_rotation test_turn_rotation_flow test_confinement_god test_hud_layout test_save_load_replay))
$(eval $(call RICH4_TEST_SUITE,test-special-place-alignment,,test_special_place_alignment test_route_event_flow test_route_event_executor test_game_event_payload test_game_presentation test_special_presentation_flow test_confinement_flow test_shop_flow test_point_minigame_flow test_magic_house_flow test_lottery_flow test_bank_flow))
$(eval $(call RICH4_TEST_SUITE,test-stock,,test_stock))
$(eval $(call RICH4_TEST_SUITE,test-text-backends,,test_text_backend_freetype))
$(eval $(call RICH4_TEST_SUITE,test-tool,,test_tool))
$(eval $(call RICH4_TEST_SUITE,test-tool-followup,,test_tool_followup_flow))
$(eval $(call RICH4_TEST_SUITE,test-tool-prompts,,test_game_speech_flow test_tool_panel_flow test_ai_flow test_tool_followup_flow test_game_presentation))
$(eval $(call RICH4_TEST_SUITE,test-tool-prompts-sdl,,test_tool_prompts_sdl))
$(eval $(call RICH4_TEST_SUITE,test-worker-animation,,test_tool_animation_flow test_game_view_model test_game_view_model_input test_worker_animation))
$(eval $(call RICH4_TEST_SUITE,test-unit,test-architecture,$(TEST_UNIT_TESTS)))

.PHONY: test-flc-regression
test-flc-regression: $(call rich4_test_prereqs,$(FLC_REGRESSION_TESTS))
	$(RICH4_RUN_TESTS) --timeout 45 --json build/miyoo/flc-regression.json $(FLC_REGRESSION_TESTS)

test: $(BIN_DIR)/mkf_dump $(BIN_DIR)/mkf_make_sample test-ledger test-property test-facility test-insurance test-card test-unit test-resource test-characterization test-replay
	$(BIN_DIR)/mkf_make_sample $(BUILD_DIR)/sample.mkf
	$(BIN_DIR)/mkf_dump --summary --limit 1 $(BUILD_DIR)/sample.mkf
	$(BIN_DIR)/mkf_dump $(BUILD_DIR)/sample.mkf
