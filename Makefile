CC ?= cc
AR ?= ar
CFLAGS ?= -Os -std=c99 -Wall -Wextra -Wpedantic

TEXT_FRONTEND ?= 1
USE_LIBM ?= 1
MAX_EVENTS ?= 512
CONTEXT_BYTES ?= 3072
AUDIO_BLOCK ?= 128

CPPFLAGS ?=
CPPFLAGS += -Iinclude -Isrc \
	-DIDTTS_ENABLE_TEXT_FRONTEND=$(TEXT_FRONTEND) \
	-DIDTTS_USE_LIBM=$(USE_LIBM) \
	-DIDTTS_MAX_EVENTS=$(MAX_EVENTS) \
	-DIDTTS_CONTEXT_BYTES=$(CONTEXT_BYTES) \
	-DIDTTS_AUDIO_BLOCK=$(AUDIO_BLOCK)

ifeq ($(USE_LIBM),1)
LDLIBS ?= -lm
else
LDLIBS ?=
endif

BUILD := build-make
LIB_OBJS := \
	$(BUILD)/idtts.o \
	$(BUILD)/idtts_ipa.o \
	$(BUILD)/idtts_voice.o \
	$(BUILD)/idtts_synth.o
ifeq ($(TEXT_FRONTEND),1)
LIB_OBJS += $(BUILD)/idtts_g2p.o
endif

.PHONY: all clean test size
all: $(BUILD)/idtts

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libidtts.a: $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BUILD)/idtts_cli.o: tools/idtts_cli.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/idtts: $(BUILD)/idtts_cli.o $(BUILD)/libidtts.a
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake --build build --parallel
	ctest --test-dir build --output-on-failure

size: $(BUILD)/libidtts.a
	size $(LIB_OBJS)

clean:
	rm -rf build build-* 
