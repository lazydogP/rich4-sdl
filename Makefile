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
ifneq ($(wildcard .local/sdl1/mingw64/include/SDL/SDL.h),)
SDL_PREFIX ?= .local/sdl1/mingw64
else
SDL_PREFIX ?=
endif
ifneq ($(wildcard .local/sdl1/mingw64/include/SDL/SDL_mixer.h),)
MIXER_PREFIX ?= .local/sdl1/mingw64
else
MIXER_PREFIX ?=
endif
ifneq ($(wildcard .local/sdl1/mingw64/include/SDL/SDL_ttf.h),)
TTF_PREFIX ?= .local/sdl1/mingw64
else
TTF_PREFIX ?=
endif
else
CC = gcc
SDL_CONFIG ?= sdl-config
LOCAL_TOOLCHAIN_BIN =
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
ROUTE_FLOW_TEST_OBJS = $(BUILD_DIR)/company.o $(BUILD_DIR)/card_command.o \
	$(BUILD_DIR)/god.o \
	$(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/special_event_news.o \
	$(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o \
	$(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/ai_stock_policy.o
ROUTE_FLOW_LINK_OBJS = $(BUILD_DIR)/route_flow.o $(CORE_OBJS) $(BUILD_DIR)/map_object_ref.o

CORE_OBJS = $(BUILD_DIR)/rng.o $(BUILD_DIR)/character_template.o
CORE_OBJS += $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/god.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/special_actor_action.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/victory.o
CORE_OBJS += $(BUILD_DIR)/game_event_payload_request_accessors.o
CORE_OBJS += $(BUILD_DIR)/game_event_typed.o
CORE_OBJS += $(ROUTE_EXECUTOR_OBJS)
CORE_OBJS += $(BUILD_DIR)/tool_flow.o
CORE_OBJS += $(BUILD_DIR)/card_command.o
CORE_OBJS += $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/first_turn_dispatch.o
CORE_OBJS += $(BUILD_DIR)/character_speech_rules.o
CORE_OBJS += $(BUILD_DIR)/auction.o
CORE_OBJS += $(BUILD_DIR)/god_money.o
APP_OBJS = $(BUILD_DIR)/scene.o $(BUILD_DIR)/scene_frame_flow.o $(BUILD_DIR)/new_game_scene_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/menu_flow.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/game_event_presentation_snapshot.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_save.o $(BUILD_DIR)/game_session_map_init.o $(BUILD_DIR)/game_session_runtime_init.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/command_event_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/route_flow.o $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/route_event_flow.o $(BUILD_DIR)/ai_flow.o $(BUILD_DIR)/ai_settings_flow.o $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/bank_flow.o $(BUILD_DIR)/bank_interaction_flow.o $(BUILD_DIR)/stock_interaction_flow.o $(BUILD_DIR)/card_panel_flow.o $(BUILD_DIR)/monster_card_flow.o $(BUILD_DIR)/lottery_flow.o $(BUILD_DIR)/roulette_flow.o $(BUILD_DIR)/point_minigame_flow.o $(BUILD_DIR)/point_minigame_input.o $(BUILD_DIR)/magic_house_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/shop_flow.o $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/tool_followup_flow.o $(BUILD_DIR)/tool_panel_flow.o $(BUILD_DIR)/special_presentation_flow.o $(BUILD_DIR)/special_presentation_update_flow.o $(BUILD_DIR)/special_presentation_support_flow.o $(BUILD_DIR)/special_event_timeline.o $(BUILD_DIR)/special_actor_flow.o $(BUILD_DIR)/turn_movement_flow.o
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
RICH4_SDL1_PROFILE := desktop
ifneq (,$(findstring RICH4_PLATFORM_HANDHELD,$(CPPFLAGS)))
RICH4_SDL1_PROFILE := handheld
endif
ifneq (,$(findstring RICH4_PLATFORM_MIYOO,$(CPPFLAGS)))
RICH4_SDL1_PROFILE := miyoo
endif
SDL1_SOURCES := $(shell $(PYTHON) tests/architecture/list_sdl1_sources.py)
# Keep generated rules for the complete source inventory so standalone tests
# can build either backend, but link only the selected profile's production
# backend.  This is the explicit A10 profile difference.
SDL1_OBJECT_MANIFEST := $(shell $(PYTHON) tests/architecture/list_sdl1_sources.py --objects --profile $(RICH4_SDL1_PROFILE))
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

.PHONY: test-monthly-finance
test-monthly-finance: $(BIN_DIR)/test_monthly_finance
	$(BIN_DIR)/test_monthly_finance
test-monthly-finance-regression: test-monthly-finance test-stock test-bank test-ledger test-save-load-replay $(BIN_DIR)/test_turn_rotation $(BIN_DIR)/test_turn_rotation_flow
	$(BIN_DIR)/test_turn_rotation
	$(BIN_DIR)/test_turn_rotation_flow
.PHONY: test-monthly-finance-presentation
test-monthly-finance-presentation: stage-sdl-runtime $(BIN_DIR)/test_monthly_finance_presentation
	$(BIN_DIR)/test_monthly_finance_presentation
$(BIN_DIR)/test_monthly_finance_presentation: tests/sdl/test_monthly_finance_presentation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)
test-unit: test-monthly-finance
$(BIN_DIR)/test_monthly_finance: tests/unit/test_monthly_finance.c $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o $(CORE_OBJS) $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^
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

check-core-headers: $(BIN_DIR)/test_core_headers
	$(BIN_DIR)/test_core_headers

tools: $(BIN_DIR)/mkf_dump $(BIN_DIR)/mkf_dump_flc_frames $(BIN_DIR)/mkf_scan_flc_opcodes $(BIN_DIR)/mkf_validate_map_data $(BIN_DIR)/mkf_validate_m37_resources $(BIN_DIR)/mkf_contact_sheet $(BIN_DIR)/mkf_extract_resource

sdl: check-sdl $(BIN_DIR)/rich4_sdl

sdl-handheld:
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DRICH4_PLATFORM_HANDHELD" BUILD_DIR=build/handheld BIN_DIR=bin/handheld sdl

# Host remains Windows; only compiler, headers and link libraries are ARM.
.PHONY: setup-miyoo test-miyoo-toolchain
MIYOO_SETUP_ARGS ?=
setup-miyoo:
	$(PYTHON) scripts/setup_miyoo_toolchain.py $(MIYOO_SETUP_ARGS)

test-miyoo-toolchain:
	$(PYTHON) tests/unit/test_miyoo_toolchain.py

.PHONY: miyoo
miyoo: setup-miyoo
	$(PYTHON) scripts/build_miyoo.py

.PHONY: miyoo-qa-build
miyoo-qa-build: setup-miyoo
	$(PYTHON) scripts/build_miyoo.py --qa

.PHONY: test-miyoo-qemu
.PHONY: test-new-game-loading
test-new-game-loading: $(BIN_DIR)/test_game_loop_frame_end $(BIN_DIR)/test_game_loop_scene_update $(BIN_DIR)/test_new_game_setup_flow
	$(BIN_DIR)/test_game_loop_frame_end
	$(BIN_DIR)/test_game_loop_scene_update
	$(BIN_DIR)/test_new_game_setup_flow

MIYOO_QEMU_MEMORY_MB ?= 256
MIYOO_QEMU_ARGS ?=
test-miyoo-qemu: miyoo-qa-build
	$(PYTHON) scripts/setup_miyoo_toolchain.py --qemu $(MIYOO_SETUP_ARGS)
	$(PYTHON) scripts/validate_miyoo_qemu.py --memory-mb $(MIYOO_QEMU_MEMORY_MB) $(MIYOO_QEMU_ARGS)

.PHONY: font-bake-miyoo
font-bake-miyoo: font-bake
	$(PYTHON) scripts/bake_miyoo_font.py

$(BIN_DIR)/test_miyoo_qemu_probe: tests/sdl/test_miyoo_qemu_probe.c $(BUILD_DIR)/sdl1_miyoo_input.o $(BUILD_DIR)/sdl1_input_adapter.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_text_backend_atlas.o $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(SDL_LIBS)

$(BUILD_DIR)/libmiyoo_qa.so: tests/sdl/miyoo_qemu_qa.c | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(filter-out -no-pie,$(CFLAGS)) $(WARNFLAGS) -fPIC -shared -o $@ $< $(SDL_LIBS) -ldl

$(BIN_DIR)/test_miyoo_asset_scope: tests/sdl/test_miyoo_asset_scope.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-ai-event-assets
test-ai-event-assets: stage-sdl-runtime $(BIN_DIR)/test_miyoo_asset_scope
	$(BIN_DIR)/test_miyoo_asset_scope

.PHONY: package-miyoo
.PHONY: test-package-release
test-package-release:
	$(PYTHON) tests/unit/test_package_release.py

MIYOO_PACKAGE_ARGS ?=
package-miyoo: miyoo
	$(PYTHON) scripts/setup_miyoo_toolchain.py --package $(MIYOO_SETUP_ARGS)
	$(PYTHON) packaging/miyoo/package.py $(MIYOO_PACKAGE_ARGS)

.PHONY: test-miyoo-input
.PHONY: test-render-equivalence
$(BUILD_DIR)/sdl1_runtime_pacing_test.o: src/platform/sdl1/runtime.c | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -DSDL_GetTicks=qa_pacing_ticks -DSDL_Delay=qa_pacing_delay -c -o $@ $<

$(BIN_DIR)/test_frame_pacing: tests/sdl/test_frame_pacing.c $(BUILD_DIR)/sdl1_runtime_pacing_test.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) $(VIEW_OBJS) $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(SDL_LIBS)

.PHONY: test-frame-pacing
test-frame-pacing: stage-sdl-runtime $(BIN_DIR)/test_frame_pacing
	$(BIN_DIR)/test_frame_pacing

$(BUILD_DIR)/sdl1_runtime_present_test.o: src/platform/sdl1/runtime.c src/platform/sdl1/render/pixel_rows.h | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -DRICH4_PLATFORM_MIYOO -c -o $@ $<

$(BIN_DIR)/test_present_pixels: tests/sdl/test_present_pixels.c $(BUILD_DIR)/sdl1_runtime_present_test.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) $(VIEW_OBJS) $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(SDL_LIBS)

.PHONY: test-present-pixels
test-present-pixels: stage-sdl-runtime $(BIN_DIR)/test_present_pixels
	$(BIN_DIR)/test_present_pixels

test-render-equivalence: stage-sdl-runtime $(BIN_DIR)/test_render_equivalence $(BIN_DIR)/test_sdl1_blit $(BIN_DIR)/test_flc_player
	$(BIN_DIR)/test_sdl1_blit
	$(BIN_DIR)/test_flc_player
	$(BIN_DIR)/test_render_equivalence

$(BIN_DIR)/test_render_equivalence: tests/sdl/test_render_equivalence.c tests/sdl/ground_scalar_reference.c tests/sdl/flc_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_render_equivalence.c tests/sdl/ground_scalar_reference.c tests/sdl/flc_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-flc-playback
test-flc-playback: stage-sdl-runtime $(BIN_DIR)/test_flc_playback
	$(BIN_DIR)/test_flc_playback

$(BIN_DIR)/test_flc_playback: tests/sdl/test_flc_playback.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_flc_playback.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

test-miyoo-input: $(BIN_DIR)/test_miyoo_input
	$(BIN_DIR)/test_miyoo_input

test-unit: test-miyoo-input

$(BIN_DIR)/test_miyoo_input: tests/unit/test_miyoo_input.c $(BUILD_DIR)/sdl1_miyoo_input.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^

stage-sdl-runtime: check-sdl
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(SDL_PREFIX)),)
	@for %%f in ("$(subst /,\\,$(SDL_PREFIX))\\bin\\*.dll") do copy /Y "%%f" "$(BIN_DIR)\\" >NUL
endif
ifneq ($(strip $(LOCAL_TOOLCHAIN_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\\,$(LOCAL_TOOLCHAIN_BIN))\\%%f" copy /Y "$(subst /,\\,$(LOCAL_TOOLCHAIN_BIN))\\%%f" "$(BIN_DIR)\\%%f" >NUL
endif
endif

test: $(BIN_DIR)/mkf_dump $(BIN_DIR)/mkf_make_sample test-ledger test-bank test-property test-facility test-insurance test-card test-tool test-special-event test-stock test-unit test-resource test-characterization test-replay
	$(BIN_DIR)/mkf_make_sample $(BUILD_DIR)/sample.mkf
	$(BIN_DIR)/mkf_dump --summary --limit 1 $(BUILD_DIR)/sample.mkf
	$(BIN_DIR)/mkf_dump $(BUILD_DIR)/sample.mkf

test-ledger: $(BIN_DIR)/test_ledger
	$(BIN_DIR)/test_ledger

test-bank: $(BIN_DIR)/test_bank
	$(BIN_DIR)/test_bank

.PHONY: test-ai-landing
.PHONY: test-magic-house
test-magic-house: $(BIN_DIR)/test_magic_house_flow $(BIN_DIR)/test_route_event_executor $(BIN_DIR)/test_game_event_payload $(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_magic_house_flow
	$(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_game_event_payload
	$(BIN_DIR)/test_route_event_flow

test-ai-landing: test-bank test-property test-facility test-card $(BIN_DIR)/test_ai $(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_ai
	$(BIN_DIR)/test_route_event_executor

test-property: $(BIN_DIR)/test_property
	$(BIN_DIR)/test_property

test-facility: $(BIN_DIR)/test_facility
	$(BIN_DIR)/test_facility

test-insurance: $(BIN_DIR)/test_insurance $(BIN_DIR)/test_insurance_alignment
	$(BIN_DIR)/test_insurance
	$(BIN_DIR)/test_insurance_alignment

test-card: $(BIN_DIR)/test_card
	$(BIN_DIR)/test_card

test-auction: $(BIN_DIR)/test_auction
	$(BIN_DIR)/test_auction

test-auction-flow: $(BIN_DIR)/test_auction_flow
	$(BIN_DIR)/test_auction_flow

test-tool: $(BIN_DIR)/test_tool
	$(BIN_DIR)/test_tool

.PHONY: test-tool-followup
test-tool-followup: $(BIN_DIR)/test_tool_followup_flow
	$(BIN_DIR)/test_tool_followup_flow

test-special-event: $(BIN_DIR)/test_special_event
	$(BIN_DIR)/test_special_event

$(BIN_DIR)/test_special_event: tests/unit/news_selection_cases.inc

.PHONY: test-news-selection
test-news-selection: $(BIN_DIR)/test_special_event $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_special_event_text
	$(BIN_DIR)/test_special_event
	$(BIN_DIR)/test_special_presentation_flow
	$(BIN_DIR)/test_special_event_text

test-stock: $(BIN_DIR)/test_stock
	$(BIN_DIR)/test_stock

.PHONY: test-context-simplification
test-context-simplification:
	$(PYTHON) tests/architecture/test_context_simplification.py
	$(PYTHON) tests/architecture/check_context_simplification.py

test-architecture: test-context-simplification
	$(PYTHON) tests/architecture/check_architecture.py
	$(PYTHON) tests/architecture/check_sdl_object_manifest.py
	$(PYTHON) tests/architecture/test_architecture_guard.py
	$(PYTHON) tests/architecture/check_renderer_viewmodel_boundary.py

test-unit: test-architecture $(BIN_DIR)/test_tool
test-unit: $(BIN_DIR)/test_app_input_flow
test-unit: $(BIN_DIR)/test_scene_input_flow
test-unit: $(BIN_DIR)/test_menu_flow
test-unit: $(BIN_DIR)/test_options_flow
test-unit: $(BIN_DIR)/test_game_loop_frame_end
test-unit: $(BIN_DIR)/test_game_loop_scene_update
test-unit: $(BIN_DIR)/test_app_map_input_flow
test-unit: $(BIN_DIR)/test_app_dynamic_object_flow
test-unit: $(BIN_DIR)/test_first_turn_flow
test-unit: $(BIN_DIR)/test_game_update_flow
test-unit: $(BIN_DIR)/test_turn_rotation_flow $(BIN_DIR)/test_route_facts $(BIN_DIR)/test_route_geometry_flow $(BIN_DIR)/test_turn_movement_flow $(BIN_DIR)/test_bank_interaction_flow $(BIN_DIR)/test_stock_interaction_flow $(BIN_DIR)/test_card_panel_flow
test-unit: $(BIN_DIR)/test_special_actor_flow
test-unit: test-engineering-attack

.PHONY: test-engineering-attack
test-engineering-attack: $(BIN_DIR)/test_engineering_attack_flow
	$(BIN_DIR)/test_engineering_attack_flow

.PHONY: test-engineering-regression
test-engineering-regression: test-engineering-attack $(BIN_DIR)/test_route_event_flow $(BIN_DIR)/test_turn_rotation_flow $(BIN_DIR)/test_turn_movement_flow $(BIN_DIR)/test_game_view_model $(BIN_DIR)/test_game_view_model_input $(BIN_DIR)/test_game_presentation $(BIN_DIR)/test_app_input_flow
	$(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_turn_rotation_flow
	$(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_game_view_model
	$(BIN_DIR)/test_game_view_model_input
	$(BIN_DIR)/test_game_presentation
	$(BIN_DIR)/test_app_input_flow

$(BIN_DIR)/test_engineering_attack_flow: tests/unit/test_engineering_attack_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_engineering_attack_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o
test-unit: $(BIN_DIR)/test_game_session_map_init
test-unit: $(BIN_DIR)/test_game_session_runtime_init
test-unit: $(BIN_DIR)/test_ai_settings_flow
test-unit: $(BIN_DIR)/test_intro_flow
test-unit: test-god-acquisition

.PHONY: test-god-acquisition
test-god-acquisition: $(BIN_DIR)/test_god_acquisition
	$(BIN_DIR)/test_god_acquisition

$(BIN_DIR)/test_god_acquisition: tests/unit/test_god_acquisition.c $(BUILD_DIR)/god_money_flow.o $(BUILD_DIR)/audio_event.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_god_acquisition.c $(BUILD_DIR)/god_money_flow.o $(BUILD_DIR)/audio_event.o $(CORE_OBJS)

.PHONY: test-god-regression test-god-sdl
test-god-regression: test-god-acquisition $(BIN_DIR)/test_confinement_god $(BIN_DIR)/test_app_dynamic_object_flow $(BIN_DIR)/test_turn_movement_flow $(BIN_DIR)/test_game_update_flow $(BIN_DIR)/test_game_presentation $(BIN_DIR)/test_audio_event $(BIN_DIR)/test_dice_flow
	$(BIN_DIR)/test_confinement_god
	$(BIN_DIR)/test_app_dynamic_object_flow
	$(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_game_update_flow
	$(BIN_DIR)/test_game_presentation
	$(BIN_DIR)/test_audio_event
	$(BIN_DIR)/test_dice_flow

test-god-sdl: stage-sdl-runtime $(BIN_DIR)/test_god_sdl
	$(BIN_DIR)/test_god_sdl

$(BIN_DIR)/test_god_sdl: tests/sdl/test_god_acquisition.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_god_acquisition.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)
test-unit: $(BIN_DIR)/test_auction
test-unit: $(BIN_DIR)/test_auction_flow

test-unit: check-core-headers $(BIN_DIR)/test_rng $(BIN_DIR)/test_flc_format_names $(BIN_DIR)/test_map_object_ref $(BIN_DIR)/test_resource_catalog $(BIN_DIR)/test_character_speech $(BIN_DIR)/test_game_speech_flow $(BIN_DIR)/test_insurance_flow $(BIN_DIR)/test_bank_flow $(BIN_DIR)/test_monster_card_flow $(BIN_DIR)/test_lottery_flow $(BIN_DIR)/test_roulette_flow $(BIN_DIR)/test_point_minigame_flow $(BIN_DIR)/test_magic_house_flow $(BIN_DIR)/test_confinement_flow $(BIN_DIR)/test_shop_flow $(BIN_DIR)/test_dice_flow $(BIN_DIR)/test_property_flow $(BIN_DIR)/test_tool_animation_flow $(BIN_DIR)/test_tool_followup_flow $(BIN_DIR)/test_tool_panel_flow $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_ai_flow $(BIN_DIR)/test_turn_start_flow $(BIN_DIR)/test_turn_status_flow $(BIN_DIR)/test_special_event_text $(BIN_DIR)/test_music $(BIN_DIR)/test_special_actor_action $(BIN_DIR)/test_new_game_init $(BIN_DIR)/test_new_game_setup_flow $(BIN_DIR)/test_game_domain $(BIN_DIR)/test_game_command $(BIN_DIR)/test_bank $(BIN_DIR)/test_game_dispatch $(BIN_DIR)/test_game_event_payload $(BIN_DIR)/test_ai $(BIN_DIR)/test_first_turn_dispatch $(BIN_DIR)/test_route_movement $(BIN_DIR)/test_route_event_queue $(BIN_DIR)/test_route_event_executor $(BIN_DIR)/test_route_flow $(BIN_DIR)/test_route_event_flow $(BIN_DIR)/test_player_status $(BIN_DIR)/test_status_countdown $(BIN_DIR)/test_confinement_god $(BIN_DIR)/test_special_event $(BIN_DIR)/test_stock $(BIN_DIR)/test_turn_rotation $(BIN_DIR)/test_turn_flow $(BIN_DIR)/test_victory $(BIN_DIR)/test_game_view_model $(BIN_DIR)/test_game_view_model_input $(BIN_DIR)/test_game_state_purity $(BIN_DIR)/test_renderer_viewmodel_boundary $(BIN_DIR)/test_behavior_traceability $(BIN_DIR)/test_map_renderer $(BIN_DIR)/test_scene $(BIN_DIR)/test_game_presentation $(BIN_DIR)/test_sdl1_input_adapter $(BIN_DIR)/test_sdl1_session_header $(BIN_DIR)/test_sdl1_text_rules $(BIN_DIR)/test_text_backend $(BIN_DIR)/test_big5_utf8 $(BIN_DIR)/test_font_atlas $(BIN_DIR)/test_sdl1_blit $(BIN_DIR)/test_file_line_budget $(BIN_DIR)/test_no_debug_ui $(BIN_DIR)/test_audio_event $(BIN_DIR)/test_public_api_contract $(BIN_DIR)/test_byte_reader $(BIN_DIR)/test_text_backend_selector $(BIN_DIR)/test_text_backend_selector_handheld
	$(BIN_DIR)/test_rng
	$(BIN_DIR)/test_flc_format_names
	$(BIN_DIR)/test_map_object_ref
	$(BIN_DIR)/test_resource_catalog
	$(BIN_DIR)/test_character_speech
	$(BIN_DIR)/test_game_speech_flow
	$(BIN_DIR)/test_insurance_flow
	$(BIN_DIR)/test_bank_flow
	$(BIN_DIR)/test_monster_card_flow
	$(BIN_DIR)/test_lottery_flow
	$(BIN_DIR)/test_roulette_flow
	$(BIN_DIR)/test_point_minigame_flow
	$(BIN_DIR)/test_magic_house_flow
	$(BIN_DIR)/test_confinement_flow
	$(BIN_DIR)/test_shop_flow
	$(BIN_DIR)/test_dice_flow
	$(BIN_DIR)/test_property_flow
	$(BIN_DIR)/test_tool_animation_flow
	$(BIN_DIR)/test_tool_followup_flow
	$(BIN_DIR)/test_tool_panel_flow
	$(BIN_DIR)/test_ai_flow
	$(BIN_DIR)/test_ai_settings_flow
	$(BIN_DIR)/test_intro_flow
	$(BIN_DIR)/test_auction
	$(BIN_DIR)/test_auction_flow
	$(BIN_DIR)/test_turn_start_flow
	$(BIN_DIR)/test_turn_status_flow
	$(BIN_DIR)/test_game_loop_frame_end
	$(BIN_DIR)/test_game_loop_scene_update
	$(BIN_DIR)/test_app_input_flow
	$(BIN_DIR)/test_scene_input_flow
	$(BIN_DIR)/test_app_map_input_flow
	$(BIN_DIR)/test_app_dynamic_object_flow
	$(BIN_DIR)/test_stock_interaction_flow
	$(BIN_DIR)/test_card_panel_flow
	$(BIN_DIR)/test_route_geometry_flow
	$(BIN_DIR)/test_game_session_map_init
	$(BIN_DIR)/test_game_session_runtime_init
	$(BIN_DIR)/test_special_event_text
	$(BIN_DIR)/test_music
	$(BIN_DIR)/test_special_actor_action
	$(BIN_DIR)/test_new_game_init
	$(BIN_DIR)/test_new_game_setup_flow
	$(BIN_DIR)/test_game_domain
	$(BIN_DIR)/test_game_command
	$(BIN_DIR)/test_bank
	$(BIN_DIR)/test_game_dispatch
	$(BIN_DIR)/test_game_event_payload
	$(BIN_DIR)/test_ai
	$(BIN_DIR)/test_tool
	$(BIN_DIR)/test_first_turn_dispatch
	$(BIN_DIR)/test_route_movement
	$(BIN_DIR)/test_route_event_queue
	$(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_route_flow
	$(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_player_status
	$(BIN_DIR)/test_status_countdown
	$(BIN_DIR)/test_confinement_god
	$(BIN_DIR)/test_special_event
	$(BIN_DIR)/test_stock
	$(BIN_DIR)/test_turn_rotation
	$(BIN_DIR)/test_turn_flow
	$(BIN_DIR)/test_victory
	$(BIN_DIR)/test_game_view_model
	$(BIN_DIR)/test_game_view_model_input
	$(BIN_DIR)/test_game_state_purity
	$(BIN_DIR)/test_renderer_viewmodel_boundary
	$(BIN_DIR)/test_behavior_traceability
	$(BIN_DIR)/test_map_renderer
	$(BIN_DIR)/test_scene
	$(BIN_DIR)/test_game_presentation
	$(BIN_DIR)/test_sdl1_input_adapter
	$(BIN_DIR)/test_sdl1_session_header
	$(BIN_DIR)/test_sdl1_text_rules
	$(BIN_DIR)/test_text_backend
	$(BIN_DIR)/test_big5_utf8
	$(BIN_DIR)/test_font_atlas
	$(BIN_DIR)/test_sdl1_blit
	$(BIN_DIR)/test_file_line_budget
	$(BIN_DIR)/test_no_debug_ui
	$(BIN_DIR)/test_audio_event
	$(BIN_DIR)/test_public_api_contract
	$(BIN_DIR)/test_text_backend_selector
	$(BIN_DIR)/test_text_backend_selector_handheld

include tools/frame_diff/targets.mk

test-resource: $(BIN_DIR)/test_spr_smp $(BIN_DIR)/test_flc_animation $(BIN_DIR)/test_flc_player $(BIN_DIR)/test_resource_store $(BIN_DIR)/test_resource_store_cache $(BIN_DIR)/test_preview_asset_lifetime $(BIN_DIR)/test_resource_store_fault_injection $(BIN_DIR)/test_map_data $(BIN_DIR)/test_map_data_fault_injection $(BIN_DIR)/test_mkf_failures $(BIN_DIR)/test_mkf_fault_injection
	$(BIN_DIR)/test_spr_smp
	$(BIN_DIR)/test_flc_animation
	$(BIN_DIR)/test_flc_player
	$(BIN_DIR)/test_resource_store
	$(BIN_DIR)/test_resource_store_cache
	$(BIN_DIR)/test_preview_asset_lifetime
	$(BIN_DIR)/test_resource_store_fault_injection
	$(BIN_DIR)/test_map_data
	$(BIN_DIR)/test_map_data_fault_injection
	$(BIN_DIR)/test_mkf_failures
	$(BIN_DIR)/test_mkf_fault_injection

test-characterization: $(BIN_DIR)/test_trace
	$(BIN_DIR)/test_trace

.PHONY: test-real-map-bootstrap
test-real-map-bootstrap: $(BIN_DIR)/test_real_map_bootstrap
	$(BIN_DIR)/test_real_map_bootstrap

test-replay: $(BIN_DIR)/test_first_turn_replay $(BIN_DIR)/test_route_event_executor_replay $(BIN_DIR)/test_full_game_replay $(BIN_DIR)/test_real_map_bootstrap $(BIN_DIR)/test_save_load_replay
	$(BIN_DIR)/test_first_turn_replay
	$(BIN_DIR)/test_route_event_executor_replay
	$(BIN_DIR)/test_full_game_replay
	$(BIN_DIR)/test_real_map_bootstrap
	$(BIN_DIR)/test_save_load_replay

test-save-load-replay: $(BIN_DIR)/test_save_load_replay
	$(BIN_DIR)/test_save_load_replay

test-render: $(BIN_DIR)/test_map_renderer $(BIN_DIR)/test_flc_player
	$(BIN_DIR)/test_map_renderer
	$(BIN_DIR)/test_flc_player

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
ifneq ($(strip $(LOCAL_TOOLCHAIN_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" copy /Y "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
endif
endif

$(BIN_DIR)/mkf_dump_flc_frames: $(MKF_OBJS) $(FLC_OBJS) $(FLC_TOOL_OBJS) | dirs
	$(CC) $(CFLAGS) $(WARNFLAGS) -o $@ $(FLC_TOOL_OBJS) $(MKF_OBJS) $(FLC_OBJS)
ifeq ($(OS),Windows_NT)
ifneq ($(strip $(LOCAL_TOOLCHAIN_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" copy /Y "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
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
ifneq ($(strip $(LOCAL_TOOLCHAIN_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" copy /Y "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
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
ifneq ($(strip $(LOCAL_TOOLCHAIN_BIN)),)
	@for %%f in ($(RUNTIME_DLLS)) do if exist "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" copy /Y "$(subst /,\,$(LOCAL_TOOLCHAIN_BIN))\%%f" "$(BIN_DIR)\%%f" >NUL
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

$(BUILD_DIR)/special_event_magic.o: src/core/special_event_magic.c $(SPECIAL_EVENT_INTERNAL_HEADERS) | dirs
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

$(BUILD_DIR)/card_command.o: src/core/card_command.c src/core/card_internal.h src/core/card.h src/core/god.h src/core/stock.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/card_command.c

$(BUILD_DIR)/tool.o: src/core/tool.c src/core/tool.h src/core/tool_internal.h src/core/game_state.h src/core/game_event.h src/core/insurance.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/tool.c

$(BUILD_DIR)/tool_flow.o: src/core/tool_flow.c src/core/tool_internal.h src/core/tool.h src/core/insurance.h src/core/confinement.h | dirs
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

$(BUILD_DIR)/special_actor_action.o: src/core/special_actor_action.c src/core/special_actor_action.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/special_actor_action.c

$(BUILD_DIR)/turn_flow.o: src/core/turn_flow.c src/core/turn_flow.h src/core/game_state.h src/core/game_flow_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/turn_flow.c

$(BUILD_DIR)/victory.o: src/core/victory.c src/core/victory.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/victory.c

$(BUILD_DIR)/game_view_model.o: src/view/game_view_model.c src/view/game_view_model.h src/core/bank.h src/core/game_flow_state.h src/core/game_state.h src/data/map_data.h src/data/rich4_map_tables.inc | dirs
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

$(BUILD_DIR)/bankruptcy_flow.o: src/app/bankruptcy_flow.c src/app/bankruptcy_flow.h src/app/game_session.h src/app/game_presentation.h src/core/game_event_payload.h | dirs
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

$(BUILD_DIR)/command_event_flow.o: src/app/command_event_flow.c src/app/command_event_flow.h src/app/game_session.h src/app/dice_flow.h src/app/game_speech_flow.h src/app/insurance_flow.h src/app/special_presentation_flow.h src/app/tool_animation_flow.h src/app/tool_followup_flow.h src/app/turn_start_flow.h src/core/game_constants.h src/core/game_event_payload.h src/core/property.h src/core/turn_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/command_event_flow.c

$(BUILD_DIR)/bank_flow.o: src/app/bank_flow.c src/app/bank_flow.h src/app/game_session.h src/core/game_command.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/bank_flow.c

$(BUILD_DIR)/bank_interaction_flow.o: src/app/bank_interaction_flow.c src/app/bank_interaction_flow.h src/app/stock_interaction_flow.h src/app/app_command.h src/app/game_session.h src/core/bank.h src/core/game_command.h src/core/route_event.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/bank_interaction_flow.c

$(BUILD_DIR)/stock_interaction_flow.o: src/app/stock_interaction_flow.c src/app/stock_interaction_flow.h src/app/app_command.h src/app/game_session.h src/core/game_command.h src/core/stock.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/stock_interaction_flow.c

$(BUILD_DIR)/monster_card_flow.o: src/app/monster_card_flow.c src/app/monster_card_flow.h src/app/game_session.h src/app/game_speech_flow.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/monster_card_flow.c

$(BUILD_DIR)/lottery_flow.o: src/app/lottery_flow.c src/app/lottery_flow.h src/app/game_session.h src/core/character_speech_rules.h src/core/game_event.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/lottery_flow.c

$(BUILD_DIR)/roulette_flow.o: src/app/roulette_flow.c src/app/roulette_flow.h src/app/game_session.h src/app/audio_event.h src/app/insurance_flow.h src/core/game_event_payload.h src/core/route_event_executor.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/roulette_flow.c

.PHONY: test-catcher-minigame
test-catcher-minigame: stage-sdl-runtime $(BIN_DIR)/test_catcher_minigame $(BIN_DIR)/test_point_minigame_flow $(BIN_DIR)/test_sdl1_blit
	$(BIN_DIR)/test_point_minigame_flow
	$(BIN_DIR)/test_sdl1_blit
	$(BIN_DIR)/test_catcher_minigame

$(BIN_DIR)/test_catcher_minigame: tests/sdl/test_catcher_minigame.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_catcher_minigame.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

$(BIN_DIR)/test_point_minigame_flow: tests/unit/point_minigame_catcher_cases.inc

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

.PHONY: test-special-place-alignment
test-special-place-alignment: $(BIN_DIR)/test_special_place_alignment $(BIN_DIR)/test_route_event_flow $(BIN_DIR)/test_route_event_executor $(BIN_DIR)/test_game_event_payload $(BIN_DIR)/test_game_presentation $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_confinement_flow $(BIN_DIR)/test_shop_flow $(BIN_DIR)/test_point_minigame_flow $(BIN_DIR)/test_magic_house_flow $(BIN_DIR)/test_lottery_flow $(BIN_DIR)/test_bank_flow
	$(BIN_DIR)/test_special_place_alignment
	$(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_game_event_payload
	$(BIN_DIR)/test_game_presentation
	$(BIN_DIR)/test_special_presentation_flow
	$(BIN_DIR)/test_confinement_flow
	$(BIN_DIR)/test_shop_flow
	$(BIN_DIR)/test_point_minigame_flow
	$(BIN_DIR)/test_magic_house_flow
	$(BIN_DIR)/test_lottery_flow
	$(BIN_DIR)/test_bank_flow

$(BIN_DIR)/test_special_place_alignment: tests/unit/test_special_place_alignment.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_place_alignment.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

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

$(BUILD_DIR)/turn_rotation_flow.o: src/app/turn_rotation_flow.c src/app/turn_rotation_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/confinement.h src/core/game_event.h src/core/game_event_payload.h src/core/rng.h src/core/special_actor_action.h src/core/turn_rotation.h src/data/character_speech.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/turn_rotation_flow.c

$(BUILD_DIR)/special_actor_flow.o: src/app/special_actor_flow.c src/app/special_actor_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/game_constants.h src/core/game_event.h src/core/special_actor_action.h src/core/turn_rotation.h src/data/map_data.h src/view/game_view_model.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/special_actor_flow.c

$(BIN_DIR)/test_game_update_flow: tests/unit/test_game_update_flow.c $(BUILD_DIR)/game_update_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_update_flow.c $(BUILD_DIR)/game_update_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_turn_start_flow: tests/unit/test_turn_start_flow.c $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_start_flow.c $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_turn_status_flow: tests/unit/test_turn_status_flow.c $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_status_flow.c $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_turn_rotation_flow: tests/unit/test_turn_rotation_flow.c $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/turn_rotation_flow.o $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o $(BUILD_DIR)/hotel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_rotation_flow.c $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/turn_rotation_flow.o $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o $(BUILD_DIR)/hotel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_special_actor_flow: tests/unit/test_special_actor_flow.c $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/special_actor_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_actor_flow.c $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/special_actor_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS)

$(BIN_DIR)/test_ai_settings_flow: tests/unit/test_ai_settings_flow.c $(BUILD_DIR)/ai_settings_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_ai_settings_flow.c $(BUILD_DIR)/ai_settings_flow.o

$(BIN_DIR)/test_intro_flow: tests/unit/test_intro_flow.c $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_intro_flow.c $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS)

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

$(BUILD_DIR)/character_speech.o: src/data/character_speech.c src/data/character_speech.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/character_speech.c

$(BUILD_DIR)/character_speech_rules.o: src/core/character_speech_rules.c src/core/character_speech_rules.h src/data/character_speech.h src/core/game_state.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/core/character_speech_rules.c

$(BUILD_DIR)/special_event_text.o: src/data/special_event_text.c src/data/special_event_text.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/data/special_event_text.c

$(BIN_DIR)/test_rng: tests/unit/test_rng.c $(BUILD_DIR)/rng.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_rng.c $(BUILD_DIR)/rng.o

$(BIN_DIR)/test_core_headers: tests/unit/test_core_headers.c src/core/ai.h src/core/first_turn_dispatch.h src/core/game_command.h src/core/game_domain.h src/core/game_flow_state.h src/core/game_constants.h src/core/game_types.h src/core/game_state.h src/core/new_game_init.h src/core/player_status.h src/core/rng.h src/core/route_movement.h src/core/route_event_queue.h src/core/status_countdown.h src/core/stock.h src/core/turn_rotation.h src/core/victory.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_core_headers.c

$(BIN_DIR)/test_core_headers: src/core/tool.h

$(BIN_DIR)/test_flc_format_names: tests/unit/test_flc_format_names.c src/resource/flc_animation.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_flc_format_names.c

$(BIN_DIR)/test_map_object_ref: tests/unit/test_map_object_ref.c $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_map_object_ref.c $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_resource_catalog: tests/unit/test_resource_catalog.c $(BUILD_DIR)/resource_catalog.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_resource_catalog.c $(BUILD_DIR)/resource_catalog.o

$(BIN_DIR)/test_character_speech: tests/unit/test_character_speech.c $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_character_speech.c $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o

$(BIN_DIR)/test_special_event_text: tests/unit/test_special_event_text.c $(BUILD_DIR)/special_event_text.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_event_text.c $(BUILD_DIR)/special_event_text.o

$(BIN_DIR)/test_new_game_init: tests/unit/test_new_game_init.c $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_new_game_init.c $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o

$(BIN_DIR)/test_new_game_setup_flow: tests/unit/test_new_game_setup_flow.c $(BUILD_DIR)/new_game_scene_flow.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/scene.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/character_template.o $(BUILD_DIR)/rng.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_new_game_setup_flow.c $(BUILD_DIR)/new_game_scene_flow.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/scene.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/character_template.o $(BUILD_DIR)/rng.o

$(BIN_DIR)/test_insurance_alignment: tests/unit/test_insurance_alignment.c $(CORE_OBJS) $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_insurance_alignment.c $(CORE_OBJS) $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o

$(BIN_DIR)/test_insurance_flow: tests/unit/test_insurance_flow.c $(CORE_OBJS) $(BUILD_DIR)/insurance_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_insurance_flow.c $(CORE_OBJS) $(BUILD_DIR)/insurance_flow.o

$(BIN_DIR)/test_bank_flow: tests/unit/test_bank_flow.c $(BUILD_DIR)/bank_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/character_speech.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_bank_flow.c $(BUILD_DIR)/bank_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/character_speech.o

$(BIN_DIR)/test_monster_card_flow: tests/unit/test_monster_card_flow.c $(BUILD_DIR)/monster_card_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_monster_card_flow.c $(BUILD_DIR)/monster_card_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o

$(BIN_DIR)/test_lottery_flow: tests/unit/test_lottery_flow.c $(BUILD_DIR)/lottery_flow.o $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_lottery_flow.c $(BUILD_DIR)/lottery_flow.o $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_roulette_flow: tests/unit/test_roulette_flow.c $(BUILD_DIR)/roulette_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_roulette_flow.c $(BUILD_DIR)/roulette_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/insurance_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_point_minigame_flow: tests/unit/test_point_minigame_flow.c $(BUILD_DIR)/point_minigame_flow.o $(BUILD_DIR)/point_minigame_input.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_point_minigame_flow.c $(BUILD_DIR)/point_minigame_flow.o $(BUILD_DIR)/point_minigame_input.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_magic_house_flow: tests/unit/test_magic_house_flow.c $(BUILD_DIR)/magic_house_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_magic_house_flow.c $(BUILD_DIR)/magic_house_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_confinement_flow: tests/unit/test_confinement_flow.c $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_confinement_flow.c $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_shop_flow: tests/unit/test_shop_flow.c $(BUILD_DIR)/shop_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_shop_flow.c $(BUILD_DIR)/shop_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_dice_flow: tests/unit/test_dice_flow.c $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_dice_flow.c $(BUILD_DIR)/dice_flow.o $(BUILD_DIR)/turn_start_flow.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_property_flow: tests/unit/test_property_flow.c $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_property_flow.c $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_tool_animation_flow: tests/unit/test_tool_animation_flow.c $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_tool_animation_flow.c $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_tool_followup_flow: tests/unit/test_tool_followup_flow.c $(BUILD_DIR)/tool_followup_flow.o $(BUILD_DIR)/turn_rotation_flow.o $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o $(BUILD_DIR)/hotel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_tool_followup_flow.c $(BUILD_DIR)/tool_followup_flow.o $(BUILD_DIR)/turn_rotation_flow.o $(BUILD_DIR)/monthly_finance_flow.o $(BUILD_DIR)/monthly_finance_banker.o $(BUILD_DIR)/hotel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_tool_panel_flow: tests/unit/test_tool_panel_flow.c $(BUILD_DIR)/tool_panel_flow.o $(BUILD_DIR)/map_edge_scroll.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_tool_panel_flow.c $(BUILD_DIR)/tool_panel_flow.o $(BUILD_DIR)/map_edge_scroll.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS)

$(BIN_DIR)/test_special_presentation_flow: tests/unit/test_special_presentation_flow.c $(BUILD_DIR)/special_presentation_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/special_presentation_update_flow.o $(BUILD_DIR)/special_presentation_support_flow.o $(BUILD_DIR)/special_event_timeline.o $(BUILD_DIR)/special_event_text.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/insurance_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_presentation_flow.c $(BUILD_DIR)/special_presentation_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/special_presentation_update_flow.o $(BUILD_DIR)/special_presentation_support_flow.o $(BUILD_DIR)/special_event_timeline.o $(BUILD_DIR)/special_event_text.o $(BUILD_DIR)/tool_animation_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/turn_status_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/insurance_flow.o

$(BIN_DIR)/test_game_domain: tests/unit/test_game_domain.c $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_domain.c $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_game_command: tests/unit/test_game_command.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_command.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o

$(BIN_DIR)/test_ledger: tests/unit/test_ledger.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_ledger.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_bank: tests/unit/test_bank.c $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_bank.c $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_property: tests/unit/test_property.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_property.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_facility: tests/unit/test_facility.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_facility.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_insurance: tests/unit/test_insurance.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_insurance.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o

$(BIN_DIR)/test_card: tests/unit/test_card.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/card.o $(BUILD_DIR)/card_command.o $(BUILD_DIR)/god.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_card.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/card.o $(BUILD_DIR)/card_command.o $(BUILD_DIR)/god.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_auction: tests/unit/test_auction.c $(BUILD_DIR)/auction.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_auction.c $(BUILD_DIR)/auction.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_auction_flow: tests/unit/test_auction_flow.c $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/auction.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_auction_flow.c $(BUILD_DIR)/auction_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/auction.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_tool: tests/unit/test_tool.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_tool.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_ai: tests/unit/test_ai.c $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/god.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_ai.c $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/god.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_ai_flow: tests/unit/test_ai_flow.c $(BUILD_DIR)/ai_flow.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/god.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_ai_flow.c $(BUILD_DIR)/ai_flow.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/god.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_special_event: tests/unit/test_special_event.c $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_event.c $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o

$(BIN_DIR)/test_stock: tests/unit/test_stock.c $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_stock.c $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o

$(BIN_DIR)/test_game_dispatch: tests/unit/test_game_dispatch.c $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/card.o $(BUILD_DIR)/card_command.o $(BUILD_DIR)/god.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/game_event_payload_request_accessors.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_dispatch.c $(BUILD_DIR)/game_command_flow.o $(BUILD_DIR)/property_flow.o $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/bankruptcy_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/company.o $(BUILD_DIR)/card.o $(BUILD_DIR)/card_command.o $(BUILD_DIR)/god.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/special_event_news.o $(BUILD_DIR)/special_event_magic.o $(BUILD_DIR)/special_event_commit.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/stock_snapshot.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o $(BUILD_DIR)/game_event_payload_request_accessors.o

$(BIN_DIR)/test_first_turn_dispatch: tests/unit/test_first_turn_dispatch.c $(BUILD_DIR)/first_turn_dispatch.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_first_turn_dispatch.c $(BUILD_DIR)/first_turn_dispatch.o

$(BIN_DIR)/test_first_turn_flow: tests/unit/test_first_turn_flow.c $(BUILD_DIR)/first_turn_flow.o $(BUILD_DIR)/first_turn_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/trace.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_first_turn_flow.c $(BUILD_DIR)/first_turn_flow.o $(BUILD_DIR)/first_turn_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/trace.o

$(BIN_DIR)/test_route_movement: tests/unit/test_route_movement.c $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_movement.c $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_route_event_queue: tests/unit/test_route_event_queue.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_event_queue.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/ai_stock_policy.o

$(BIN_DIR)/test_route_event_executor: tests/unit/test_route_event_executor.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_payload_request_accessors.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/ai.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_event_executor.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_payload_request_accessors.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/ai.o

$(BIN_DIR)/test_route_event_executor: ROUTE_EXECUTOR_OBJS += $(ROUTE_FLOW_TEST_OBJS)
$(BIN_DIR)/test_route_event_executor: $(ROUTE_FLOW_TEST_OBJS)

$(BIN_DIR)/test_route_flow: tests/unit/test_route_flow.c $(ROUTE_FLOW_LINK_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_flow.c $(ROUTE_FLOW_LINK_OBJS)

$(BIN_DIR)/test_route_facts: tests/unit/test_route_facts.c $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_facts.c $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_route_geometry_flow: tests/unit/test_route_geometry_flow.c $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_geometry_flow.c $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/game_view_model.o $(CORE_OBJS)

$(BIN_DIR)/test_turn_movement_flow: tests/unit/test_turn_movement_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_movement_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_bank_interaction_flow: tests/unit/test_bank_interaction_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_bank_interaction_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_card_panel_flow: tests/unit/test_card_panel_flow.c $(BUILD_DIR)/card_panel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/map_edge_scroll.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_card_panel_flow.c $(BUILD_DIR)/card_panel_flow.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/map_edge_scroll.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_stock_interaction_flow: tests/unit/test_stock_interaction_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_stock_interaction_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_game_session_map_init: tests/unit/test_game_session_map_init.c $(BUILD_DIR)/game_session_map_init.o $(CORE_OBJS) $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_session_map_init.c $(BUILD_DIR)/game_session_map_init.o $(CORE_OBJS) $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_game_session_runtime_init: tests/unit/test_game_session_runtime_init.c $(BUILD_DIR)/game_session_runtime_init.o $(BUILD_DIR)/game_session_map_init.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/input_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/route_flow.o $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_session_runtime_init.c $(BUILD_DIR)/game_session_runtime_init.o $(BUILD_DIR)/game_session_map_init.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/input_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/route_flow.o $(BUILD_DIR)/route_facts.o $(BUILD_DIR)/confinement_flow.o $(BUILD_DIR)/route_geometry_flow.o $(BUILD_DIR)/intro_flow.o $(BUILD_DIR)/audio_event.o $(BUILD_DIR)/character_speech.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_route_event_flow: tests/unit/test_route_event_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_route_event_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_player_status: tests/unit/test_player_status.c $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_player_status.c $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_status_countdown: tests/unit/test_status_countdown.c $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_status_countdown.c $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_confinement_god: tests/unit/test_confinement_god.c $(BUILD_DIR)/confinement.o $(BUILD_DIR)/god.o $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_confinement_god.c $(BUILD_DIR)/confinement.o $(BUILD_DIR)/god.o $(BUILD_DIR)/status_countdown.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o

$(BIN_DIR)/test_turn_rotation: tests/unit/test_turn_rotation.c $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_rotation.c $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_turn_flow: tests/unit/test_turn_flow.c $(BUILD_DIR)/turn_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_turn_flow.c $(BUILD_DIR)/turn_flow.o

$(BIN_DIR)/test_victory: tests/unit/test_victory.c $(BUILD_DIR)/victory.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_victory.c $(BUILD_DIR)/victory.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_game_view_model: tests/unit/test_game_view_model.c $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_view_model.c $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_game_view_model_input: tests/unit/test_game_view_model_input.c $(BUILD_DIR)/sdl1_game_view_model_input.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_view_model_input.c $(BUILD_DIR)/sdl1_game_view_model_input.o

$(BIN_DIR)/test_game_state_purity: tests/unit/test_game_state_purity.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_state_purity.c

$(BIN_DIR)/test_renderer_viewmodel_boundary: tests/unit/test_renderer_viewmodel_boundary.c tests/architecture/check_renderer_viewmodel_boundary.py | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_renderer_viewmodel_boundary.c

$(BIN_DIR)/test_behavior_traceability: tests/unit/test_behavior_traceability.c docs/evidence/behavior-traceability.tsv | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_behavior_traceability.c

$(BIN_DIR)/test_map_renderer: tests/unit/test_map_renderer.c $(VIEW_OBJS) $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_map_renderer.c $(VIEW_OBJS) $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/dynamic_object_flow.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/god.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o

$(BIN_DIR)/test_scene: tests/unit/test_scene.c $(BUILD_DIR)/scene.o $(BUILD_DIR)/scene_frame_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_scene.c $(BUILD_DIR)/scene.o $(BUILD_DIR)/scene_frame_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_app_input_flow: tests/unit/test_app_input_flow.c $(BUILD_DIR)/input_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_app_input_flow.c $(BUILD_DIR)/input_flow.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_scene_input_flow: tests/unit/test_scene_input_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/trace.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_scene_input_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/trace.o

$(BIN_DIR)/test_menu_flow: tests/unit/test_menu_flow.c $(BUILD_DIR)/menu_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/character_template.o $(BUILD_DIR)/rng.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_menu_flow.c $(BUILD_DIR)/menu_flow.o $(BUILD_DIR)/game_session.o $(BUILD_DIR)/new_game_setup_flow.o $(BUILD_DIR)/character_template.o $(BUILD_DIR)/rng.o

$(BIN_DIR)/test_options_flow: tests/unit/test_options_flow.c $(BUILD_DIR)/options_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_options_flow.c $(BUILD_DIR)/options_flow.o

$(BIN_DIR)/test_game_loop_frame_end: tests/unit/test_game_loop_frame_end.c $(BUILD_DIR)/sdl1_game_loop_frame_end.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_loop_frame_end.c $(BUILD_DIR)/sdl1_game_loop_frame_end.o

$(BIN_DIR)/test_game_loop_scene_update: tests/unit/test_game_loop_scene_update.c $(BUILD_DIR)/sdl1_game_loop_scene_update.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_loop_scene_update.c $(BUILD_DIR)/sdl1_game_loop_scene_update.o

$(BIN_DIR)/test_app_map_input_flow: tests/unit/test_app_map_input_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/trace.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_app_map_input_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/game_view_model.o $(BUILD_DIR)/trace.o

$(BIN_DIR)/test_app_dynamic_object_flow: tests/unit/test_app_dynamic_object_flow.c $(BUILD_DIR)/dynamic_object_flow.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_app_dynamic_object_flow.c $(BUILD_DIR)/dynamic_object_flow.o $(CORE_OBJS) $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_game_presentation: tests/unit/test_game_presentation.c $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/game_event_presentation_snapshot.o $(BUILD_DIR)/character_speech.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_presentation.c $(BUILD_DIR)/game_presentation.o $(BUILD_DIR)/game_event_presentation_snapshot.o $(BUILD_DIR)/character_speech.o

$(BIN_DIR)/test_game_speech_flow: tests/unit/test_game_speech_flow.c $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_speech_flow.c $(BUILD_DIR)/game_speech_flow.o $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/character_speech_rules.o $(BUILD_DIR)/turn_flow.o

$(BIN_DIR)/test_game_event_payload: tests/unit/test_game_event_payload.c $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_payload_request_accessors.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_game_event_payload.c $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_payload_request_accessors.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_sdl1_input_adapter: tests/unit/test_sdl1_input_adapter.c $(BUILD_DIR)/sdl1_input_adapter.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_sdl1_input_adapter.c $(BUILD_DIR)/sdl1_input_adapter.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_domain.o

$(BIN_DIR)/test_sdl1_session_header: tests/unit/test_sdl1_session_header.c src/app/game_session.h src/platform/sdl1/render/game_render_cache.h | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_sdl1_session_header.c

$(BIN_DIR)/test_sdl1_blit: stage-sdl-runtime tests/unit/test_sdl1_blit.c $(BUILD_DIR)/sdl1_blit.o $(BUILD_DIR)/spr_smp.o $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_sdl1_blit.c $(BUILD_DIR)/sdl1_blit.o $(BUILD_DIR)/spr_smp.o $(BUILD_DIR)/byte_reader.o $(SDL_LIBS)

$(BIN_DIR)/test_sdl1_text_rules: tests/unit/test_sdl1_text_rules.c $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_sdl1_text_rules.c $(BUILD_DIR)/sdl1_text_rules.o

$(BIN_DIR)/test_text_backend: tests/unit/test_text_backend.c $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_text_backend.c $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o

$(BIN_DIR)/test_big5_utf8: tests/unit/test_big5_utf8.c $(BUILD_DIR)/sdl1_big5_utf8.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_big5_utf8.c $(BUILD_DIR)/sdl1_big5_utf8.o

$(BIN_DIR)/test_text_backend_selector: stage-sdl-runtime tests/unit/test_text_backend_selector.c $(BUILD_DIR)/sdl1_text_backend_selector.o $(BUILD_DIR)/sdl1_text_backend_freetype.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o $(BUILD_DIR)/sdl1_big5_utf8.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_text_backend_selector.c $(BUILD_DIR)/sdl1_text_backend_selector.o $(BUILD_DIR)/sdl1_text_backend_freetype.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o $(BUILD_DIR)/sdl1_big5_utf8.o $(TTF_LIBS) $(SDL_LIBS)

$(BIN_DIR)/test_text_backend_selector_handheld: tests/unit/test_text_backend_selector.c src/platform/sdl1/text/text_backend_selector.c src/platform/sdl1/text/text_backend_selector.h src/platform/platform_profile.h $(BUILD_DIR)/sdl1_text_backend_atlas.o $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) -DRICH4_PLATFORM_HANDHELD $(CFLAGS) $(WARNFLAGS) -c -o $(BUILD_DIR)/sdl1_text_backend_selector_handheld.o src/platform/sdl1/text/text_backend_selector.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -DRICH4_PLATFORM_HANDHELD -o $@ tests/unit/test_text_backend_selector.c $(BUILD_DIR)/sdl1_text_backend_selector_handheld.o $(BUILD_DIR)/sdl1_text_backend_atlas.o $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o

$(BIN_DIR)/test_font_atlas: tests/unit/test_font_atlas.c $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_text_backend_atlas.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_font_atlas.c $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_text_backend_atlas.o $(BUILD_DIR)/sdl1_text_backend.o $(BUILD_DIR)/sdl1_text_rules.o

$(BIN_DIR)/test_byte_reader: tests/resource/test_byte_reader.c $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_byte_reader.c $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_file_line_budget: tests/unit/test_file_line_budget.c tests/architecture/check_context_simplification.py | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_file_line_budget.c

$(BIN_DIR)/test_no_debug_ui: tests/unit/test_no_debug_ui.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_no_debug_ui.c

$(BIN_DIR)/font_bake: tools/font_bake.c $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_big5_utf8.o | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(TTF_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tools/font_bake.c $(BUILD_DIR)/sdl1_font_atlas.o $(BUILD_DIR)/sdl1_big5_utf8.o $(TTF_LIBS) $(SDL_LIBS)

.PHONY: font-bake
font-bake: $(BIN_DIR)/font_bake

$(BIN_DIR)/test_audio_event: tests/unit/test_audio_event.c $(BUILD_DIR)/audio_event.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_audio_event.c $(BUILD_DIR)/audio_event.o

$(BIN_DIR)/test_music: tests/unit/test_music.c $(BUILD_DIR)/music.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_music.c $(BUILD_DIR)/music.o

$(BIN_DIR)/test_special_actor_action: tests/unit/test_special_actor_action.c $(BUILD_DIR)/special_actor_action.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/confinement.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_special_actor_action.c $(BUILD_DIR)/special_actor_action.o $(BUILD_DIR)/card.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/bank.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/tool_flow.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/game_event_typed.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/confinement.o

$(BIN_DIR)/test_public_api_contract: tests/unit/test_public_api_contract.c $(BUILD_DIR)/resource_store.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_public_api_contract.c $(BUILD_DIR)/resource_store.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(MKF_OBJS)

$(BIN_DIR)/test_spr_smp: tests/resource/test_spr_smp.c $(SPR_SMP_OBJS) $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_spr_smp.c $(SPR_SMP_OBJS) $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_flc_animation: tests/resource/test_flc_animation.c $(FLC_OBJS) $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_flc_animation.c $(FLC_OBJS) $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_flc_player: tests/resource/test_flc_player.c $(FLC_OBJS) $(FLC_PLAYER_OBJS) $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_flc_player.c $(FLC_OBJS) $(FLC_PLAYER_OBJS) $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_resource_store: tests/resource/test_resource_store.c $(RESOURCE_STORE_OBJS) $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_resource_store.c $(RESOURCE_STORE_OBJS) $(MKF_OBJS)

$(BIN_DIR)/test_resource_store_cache: tests/resource/test_resource_store_cache.c $(RESOURCE_STORE_OBJS) $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_resource_store_cache.c $(RESOURCE_STORE_OBJS) $(MKF_OBJS)

$(BIN_DIR)/test_preview_asset_lifetime: tests/resource/test_preview_asset_lifetime.c $(BUILD_DIR)/sdl1_game_preview_assets.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(RESOURCE_STORE_OBJS) $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_preview_asset_lifetime.c $(BUILD_DIR)/sdl1_game_preview_assets.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(RESOURCE_STORE_OBJS) $(MKF_OBJS)

test-preview-assets-lifetime: $(BIN_DIR)/test_preview_asset_lifetime
	$(BIN_DIR)/test_preview_asset_lifetime

$(BIN_DIR)/test_resource_store_fault_injection: tests/resource/test_resource_store_fault_injection.c $(BUILD_DIR)/resource_store_fault.o $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) -DRICH4_RESOURCE_STORE_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_resource_store_fault_injection.c $(BUILD_DIR)/resource_store_fault.o $(MKF_OBJS)

$(BIN_DIR)/test_mkf_failures: tests/resource/test_mkf_failures.c $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_mkf_failures.c $(MKF_OBJS)

$(BIN_DIR)/test_mkf_fault_injection: tests/resource/test_mkf_fault_injection.c $(BUILD_DIR)/mkf_fault.o $(BUILD_DIR)/mkf_decompress.o $(BUILD_DIR)/byte_reader.o | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MKF_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_mkf_fault_injection.c $(BUILD_DIR)/mkf_fault.o $(BUILD_DIR)/mkf_decompress.o $(BUILD_DIR)/byte_reader.o

$(BIN_DIR)/test_map_data: tests/resource/test_map_data.c $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_map_data.c $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_map_data_fault_injection: tests/resource/test_map_data_fault_injection.c $(BUILD_DIR)/map_data_fault.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) -DRICH4_MAP_DATA_TESTING $(CFLAGS) $(WARNFLAGS) -o $@ tests/resource/test_map_data_fault_injection.c $(BUILD_DIR)/map_data_fault.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_trace: tests/characterization/test_trace.c $(BUILD_DIR)/trace.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/characterization/test_trace.c $(BUILD_DIR)/trace.o

$(BIN_DIR)/test_first_turn_replay: tests/replay/test_first_turn_replay.c $(BUILD_DIR)/rng.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/scene.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/turn_flow.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/replay/test_first_turn_replay.c $(BUILD_DIR)/rng.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/scene.o $(BUILD_DIR)/app_command.o $(BUILD_DIR)/new_game_init.o $(BUILD_DIR)/route_movement.o $(BUILD_DIR)/turn_rotation.o $(BUILD_DIR)/turn_flow.o

$(BIN_DIR)/test_route_event_executor_replay: tests/replay/test_route_event_executor_replay.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/replay/test_route_event_executor_replay.c $(BUILD_DIR)/game_domain.o $(BUILD_DIR)/game_command.o $(BUILD_DIR)/game_event.o $(BUILD_DIR)/game_event_payload.o $(BUILD_DIR)/insurance.o $(BUILD_DIR)/route_event.o $(BUILD_DIR)/route_event_queue.o $(ROUTE_EXECUTOR_OBJS) $(BUILD_DIR)/bank.o $(BUILD_DIR)/game_dispatch.o $(BUILD_DIR)/ai.o $(BUILD_DIR)/player_status.o $(BUILD_DIR)/ledger.o $(BUILD_DIR)/property.o $(BUILD_DIR)/facility.o $(BUILD_DIR)/card.o $(BUILD_DIR)/tool.o $(BUILD_DIR)/special_event.o $(BUILD_DIR)/stock.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/confinement.o $(BUILD_DIR)/victory.o $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_full_game_replay: tests/replay/test_full_game_replay.c $(CORE_OBJS) $(BUILD_DIR)/map_object_ref.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/replay/test_full_game_replay.c $(CORE_OBJS) $(BUILD_DIR)/map_object_ref.o

$(BIN_DIR)/test_real_map_bootstrap: tests/replay/test_real_map_bootstrap.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/replay/test_real_map_bootstrap.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o $(MKF_OBJS)

$(BIN_DIR)/test_save_load_replay: tests/replay/test_save_load_replay.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/replay/test_save_load_replay.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

$(BIN_DIR)/test_first_turn_replay: ROUTE_EXECUTOR_OBJS += $(ROUTE_FLOW_TEST_OBJS)
$(BIN_DIR)/test_first_turn_replay: $(ROUTE_FLOW_TEST_OBJS)
$(BIN_DIR)/test_first_turn_replay: ROUTE_EXECUTOR_OBJS += $(BUILD_DIR)/game_event_typed.o
$(BIN_DIR)/test_first_turn_replay: $(BUILD_DIR)/game_event_typed.o
$(BIN_DIR)/test_route_event_executor_replay: ROUTE_EXECUTOR_OBJS += $(ROUTE_FLOW_TEST_OBJS)
$(BIN_DIR)/test_route_event_executor_replay: $(ROUTE_FLOW_TEST_OBJS)
$(BIN_DIR)/test_route_event_executor_replay: ROUTE_EXECUTOR_OBJS += $(BUILD_DIR)/game_event_typed.o
$(BIN_DIR)/test_route_event_executor_replay: $(BUILD_DIR)/game_event_typed.o

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"
else
	rm -rf $(BUILD_DIR) $(BIN_DIR)
endif

.PHONY: test-card-presentation test-card-panel-flow
.PHONY: test-property-map-presentation
test-property-map-presentation: stage-sdl-runtime $(BIN_DIR)/test_property_map_presentation
	$(BIN_DIR)/test_property_map_presentation

$(BIN_DIR)/test_property_map_presentation: tests/sdl/test_property_map_presentation.c tests/sdl/property_flash_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_property_map_presentation.c tests/sdl/property_flash_scalar_reference.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

test-card-panel-flow: $(BIN_DIR)/test_card_panel_flow
	$(BIN_DIR)/test_card_panel_flow

test-card-presentation: stage-sdl-runtime $(BIN_DIR)/test_card_presentation
	$(BIN_DIR)/test_card_presentation

$(BIN_DIR)/test_card_presentation: tests/sdl/test_card_presentation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_card_presentation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-tool-prompts
test-tool-prompts: $(BIN_DIR)/test_game_speech_flow $(BIN_DIR)/test_tool_panel_flow $(BIN_DIR)/test_ai_flow $(BIN_DIR)/test_tool_followup_flow $(BIN_DIR)/test_game_presentation
	$(BIN_DIR)/test_game_speech_flow
	$(BIN_DIR)/test_tool_panel_flow
	$(BIN_DIR)/test_ai_flow
	$(BIN_DIR)/test_tool_followup_flow
	$(BIN_DIR)/test_game_presentation

.PHONY: test-tool-prompts-sdl
.PHONY: test-engineering-sdl
test-engineering-sdl: $(BIN_DIR)/test_engineering_sdl
	$(BIN_DIR)/test_engineering_sdl

$(BIN_DIR)/test_engineering_sdl: tests/sdl/test_engineering_attack.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_engineering_attack.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

test-tool-prompts-sdl: $(BIN_DIR)/test_tool_prompts_sdl
	$(BIN_DIR)/test_tool_prompts_sdl

$(BIN_DIR)/test_tool_prompts_sdl: tests/sdl/test_tool_prompts.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_tool_prompts.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-options-sale
.PHONY: test-audio-loops
test-audio-loops: stage-sdl-runtime $(BIN_DIR)/test_audio_loops $(BIN_DIR)/test_audio_event $(BIN_DIR)/test_magic_house_flow $(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_audio_event
	$(BIN_DIR)/test_magic_house_flow
	$(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_audio_loops

$(BIN_DIR)/test_audio_loops: tests/sdl/test_audio_loops.c $(BUILD_DIR)/audio.o $(BUILD_DIR)/sdl1_audio_sdl_mixer_playback.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_audio_loops.c $(BUILD_DIR)/audio.o $(BUILD_DIR)/sdl1_audio_sdl_mixer_playback.o $(BUILD_DIR)/sdl1_runtime_trace.o $(BUILD_DIR)/resource_store.o $(MKF_OBJS) $(MIXER_LIBS) $(SDL_LIBS)

# These non-SDL rules do not emit transitive header depfiles. All consumers
# must agree on GameState/GameSession layout after hosting-state changes.
$(CORE_OBJS) $(APP_OBJS) $(VIEW_OBJS) $(DEBUG_OBJS): src/core/game_state.h
$(APP_OBJS) $(VIEW_OBJS) $(DEBUG_OBJS): src/app/ai_settings_state.h

.PHONY: test-ai-settings
test-ai-pixel-candidates: stage-sdl-runtime $(BIN_DIR)/test_ai_pixel_candidates
	$(BIN_DIR)/test_ai_pixel_candidates

$(BIN_DIR)/test_ai_pixel_candidates: tests/sdl/test_ai_pixel_candidates.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ $^ $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

test-ai-settings: stage-sdl-runtime $(BIN_DIR)/test_ai_settings_flow $(BIN_DIR)/test_turn_rotation $(BIN_DIR)/test_ai_settings_input
	$(BIN_DIR)/test_ai_settings_flow
	$(BIN_DIR)/test_turn_rotation
	$(BIN_DIR)/test_ai_settings_input

$(BIN_DIR)/test_ai_settings_input: tests/sdl/test_ai_settings_input.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_ai_settings_input.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

test-options-sale: stage-sdl-runtime $(BIN_DIR)/test_options_sale
	$(BIN_DIR)/test_options_sale

.PHONY: test-hud-layout
test-hud-layout: stage-sdl-runtime $(BIN_DIR)/test_hud_layout
	$(BIN_DIR)/test_hud_layout

$(BIN_DIR)/test_hud_layout: tests/sdl/test_hud_layout.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_hud_layout.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-worker-animation
test-worker-animation: stage-sdl-runtime $(BIN_DIR)/test_worker_animation $(BIN_DIR)/test_tool_animation_flow $(BIN_DIR)/test_game_view_model $(BIN_DIR)/test_game_view_model_input
	$(BIN_DIR)/test_tool_animation_flow
	$(BIN_DIR)/test_game_view_model
	$(BIN_DIR)/test_game_view_model_input
	$(BIN_DIR)/test_worker_animation

$(BIN_DIR)/test_worker_animation: tests/sdl/test_worker_animation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_worker_animation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

$(BIN_DIR)/test_options_sale: tests/sdl/test_options_sale.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_options_sale.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-machine-doll-unit test-machine-doll
test-machine-doll-unit: $(BIN_DIR)/test_tool_followup_flow $(BIN_DIR)/test_app_dynamic_object_flow $(BIN_DIR)/test_game_view_model $(BIN_DIR)/test_game_view_model_input $(BIN_DIR)/test_tool
	$(BIN_DIR)/test_tool_followup_flow
	$(BIN_DIR)/test_app_dynamic_object_flow
	$(BIN_DIR)/test_game_view_model
	$(BIN_DIR)/test_game_view_model_input
	$(BIN_DIR)/test_tool

test-machine-doll: test-machine-doll-unit $(BIN_DIR)/test_machine_doll
	$(BIN_DIR)/test_machine_doll

$(BIN_DIR)/test_machine_doll: tests/sdl/test_machine_doll.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_machine_doll.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-god-resident
test-god-resident: $(BIN_DIR)/test_property_flow $(BIN_DIR)/test_confinement_god $(BIN_DIR)/test_turn_rotation_flow $(BIN_DIR)/test_route_event_executor $(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_property_flow
	$(BIN_DIR)/test_confinement_god
	$(BIN_DIR)/test_turn_rotation_flow
	$(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_route_event_flow

.PHONY: test-dog-encounter
test-dog-encounter: stage-sdl-runtime $(BIN_DIR)/test_dog_encounter $(BIN_DIR)/test_app_dynamic_object_flow $(BIN_DIR)/test_special_actor_flow $(BIN_DIR)/test_insurance_flow $(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_app_dynamic_object_flow
	$(BIN_DIR)/test_special_actor_flow
	$(BIN_DIR)/test_insurance_flow
	$(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_dog_encounter

$(BIN_DIR)/test_dog_encounter: tests/sdl/test_dog_encounter.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_dog_encounter.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-road-gift
test-road-gift: $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_turn_movement_flow $(BIN_DIR)/test_audio_event
	$(BIN_DIR)/test_special_presentation_flow
	$(BIN_DIR)/test_turn_movement_flow
	$(BIN_DIR)/test_audio_event

# Original company/insurance landing order and its presentation continuations.
.PHONY: test-insurance-building-sdl
test-insurance-building-sdl: stage-sdl-runtime $(BIN_DIR)/test_insurance_building_sdl
	$(BIN_DIR)/test_insurance_building_sdl

.PHONY: test-fortune-selection-sdl
.PHONY: test-fortune-alignment
test-fortune-alignment: test-special-event test-special-place-alignment test-fortune-selection-sdl $(BIN_DIR)/test_character_speech $(BIN_DIR)/test_special_event_text
	$(BIN_DIR)/test_character_speech
	$(BIN_DIR)/test_special_event_text

test-fortune-selection-sdl: stage-sdl-runtime $(BIN_DIR)/test_fortune_selection_sdl
	$(BIN_DIR)/test_fortune_selection_sdl

$(BIN_DIR)/test_fortune_selection_sdl: tests/sdl/test_fortune_selection.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_fortune_selection.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

$(BIN_DIR)/test_insurance_building_sdl: tests/sdl/test_insurance_building.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_insurance_building.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

.PHONY: test-company-landing
test-company-landing: $(BIN_DIR)/test_route_event_executor $(BIN_DIR)/test_roulette_flow $(BIN_DIR)/test_insurance_flow $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_property_flow $(BIN_DIR)/test_map_renderer
	$(BIN_DIR)/test_route_event_executor
	$(BIN_DIR)/test_roulette_flow
	$(BIN_DIR)/test_insurance_flow
	$(BIN_DIR)/test_special_presentation_flow
	$(BIN_DIR)/test_property_flow
	$(BIN_DIR)/test_map_renderer

.PHONY: test-auction-presentation
test-auction-presentation: stage-sdl-runtime $(BIN_DIR)/test_auction_presentation
	$(BIN_DIR)/test_auction_presentation

$(BIN_DIR)/test_auction_presentation: tests/sdl/test_auction_presentation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) | dirs
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/sdl/test_auction_presentation.c $(MKF_OBJS) $(SPR_SMP_OBJS) $(FLC_OBJS) $(filter-out $(BUILD_DIR)/main_sdl.o,$(SDL_OBJS)) $(MIXER_LIBS) $(TTF_LIBS) $(SDL_LIBS)

$(BUILD_DIR)/hotel_flow.o: src/app/hotel_flow.c src/app/hotel_flow.h src/app/game_session.h src/app/route_geometry_flow.h src/core/game_event_payload.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ src/app/hotel_flow.c

.PHONY: test-hotel
test-hotel: $(BIN_DIR)/test_hotel_flow
	$(BIN_DIR)/test_hotel_flow

$(BIN_DIR)/test_hotel_flow: tests/unit/test_hotel_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ tests/unit/test_hotel_flow.c $(APP_OBJS) $(CORE_OBJS) $(BUILD_DIR)/character_speech.o $(BUILD_DIR)/map_data.o $(BUILD_DIR)/map_object_ref.o $(BUILD_DIR)/trace.o $(BUILD_DIR)/game_view_model.o

test-unit: test-hotel

$(BUILD_DIR)/route_event_flow.o $(BUILD_DIR)/turn_rotation_flow.o: src/app/hotel_flow.h

.PHONY: test-hotel-regression
test-hotel-regression: test-hotel test-facility test-insurance $(BIN_DIR)/test_turn_rotation_flow $(BIN_DIR)/test_route_event_flow $(BIN_DIR)/test_game_update_flow $(BIN_DIR)/test_turn_start_flow $(BIN_DIR)/test_turn_status_flow $(BIN_DIR)/test_audio_event $(BIN_DIR)/test_roulette_flow $(BIN_DIR)/test_game_view_model $(BIN_DIR)/test_route_event_executor test-save-load-replay
	$(BIN_DIR)/test_turn_rotation_flow
	$(BIN_DIR)/test_route_event_flow
	$(BIN_DIR)/test_game_update_flow
	$(BIN_DIR)/test_turn_start_flow
	$(BIN_DIR)/test_turn_status_flow
	$(BIN_DIR)/test_audio_event
	$(BIN_DIR)/test_roulette_flow
	$(BIN_DIR)/test_game_view_model
	$(BIN_DIR)/test_route_event_executor

$(APP_OBJS) $(VIEW_OBJS) $(SDL1_SESSION_DEPENDENCY_OBJECTS): src/data/confinement_presentation.h

# Presentation clocks and card animation fields change shared struct layout.
$(filter %.o,$(SDL_OBJS)): src/app/animation_clock.h src/app/game_presentation.h src/app/card_panel_state.h

FLC_REGRESSION_BINS := $(BIN_DIR)/test_intro_flow $(BIN_DIR)/test_tool_animation_flow $(BIN_DIR)/test_monster_card_flow $(BIN_DIR)/test_special_presentation_flow $(BIN_DIR)/test_property_flow $(BIN_DIR)/test_dice_flow $(BIN_DIR)/test_point_minigame_flow $(BIN_DIR)/test_magic_house_flow $(BIN_DIR)/test_monthly_finance $(BIN_DIR)/test_app_dynamic_object_flow $(BIN_DIR)/test_card_panel_flow $(BIN_DIR)/test_flc_playback $(BIN_DIR)/test_audio_loops $(BIN_DIR)/test_render_equivalence $(BIN_DIR)/test_miyoo_asset_scope $(BIN_DIR)/test_lottery_flow $(BIN_DIR)/test_god_sdl $(BIN_DIR)/test_options_sale
.PHONY: test-flc-regression
test-flc-regression: stage-sdl-runtime $(FLC_REGRESSION_BINS)
	$(PYTHON) scripts/run_flc_regression.py $(FLC_REGRESSION_BINS)

.PHONY: test-text-backends
test-text-backends: $(BIN_DIR)/test_text_backend_selector $(BIN_DIR)/test_text_backend_selector_handheld
	$(BIN_DIR)/test_text_backend_selector
	$(BIN_DIR)/test_text_backend_selector_handheld
