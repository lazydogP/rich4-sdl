.PHONY: frame-diff test-frame-diff
frame-diff:
	$(PYTHON) tools/frame_diff/build.py
test-frame-diff:
	$(PYTHON) -m unittest discover -s tools/frame_diff -p "test_*.py"

.PHONY: frame-diff-sdl
frame-diff-sdl:
	$(MAKE) sdl R4DIFF=1 BUILD_DIR=.local/frame-diff-build BIN_DIR=.local/frame-diff-bin
.PHONY: test-frame-diff-inactive
test-frame-diff-inactive: sdl frame-diff-sdl
	$(PYTHON) tools/frame_diff/check_inactive.py
ifeq ($(R4DIFF),1)
CPPFLAGS += -DR4DIFF_TEST -Itools/frame_diff
SDL_OBJS += $(BUILD_DIR)/lockstep_wire.o
$(BUILD_DIR)/lockstep_wire.o: tools/frame_diff/lockstep_wire.c tools/frame_diff/lockstep_wire.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c $< -o $@
endif
