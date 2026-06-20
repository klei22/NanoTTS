CC ?= cc
AR ?= ar
CFLAGS ?= -Os -std=c99 -Wall -Wextra -Wpedantic

# Select text modules at compile time. TEXT_FRONTEND=0 produces an IPA-only
# library; otherwise enable at least one language.
TEXT_FRONTEND ?= 1
LANG_ID ?= 1
LANG_SW ?= 1
LANG_ES ?= 1
LANG_MS ?= 1
LANG_MI ?= 1
LANG_HAW ?= 1
USE_LIBM ?= 1
MAX_EVENTS ?= 512
CONTEXT_BYTES ?= 3072
AUDIO_BLOCK ?= 128
BUILD_PWM ?= 1
BUILD_PCM ?= 1

BUILD ?= build-make
TEST_BUILD ?= build-test

ifeq ($(TEXT_FRONTEND),0)
LANG_ID := 0
LANG_SW := 0
LANG_ES := 0
LANG_MS := 0
LANG_MI := 0
LANG_HAW := 0
endif
ifeq ($(TEXT_FRONTEND),1)
ifeq ($(filter 1,$(LANG_ID) $(LANG_SW) $(LANG_ES) $(LANG_MS) $(LANG_MI) $(LANG_HAW)),)
$(error TEXT_FRONTEND=1 requires at least one LANG_* module)
endif
endif

CPPFLAGS ?=
CPPFLAGS += -Iinclude -Isrc \
	-DNANOTTS_ENABLE_TEXT_FRONTEND=$(TEXT_FRONTEND) \
	-DNANOTTS_ENABLE_LANG_ID=$(LANG_ID) \
	-DNANOTTS_ENABLE_LANG_SW=$(LANG_SW) \
	-DNANOTTS_ENABLE_LANG_ES=$(LANG_ES) \
	-DNANOTTS_ENABLE_LANG_MS=$(LANG_MS) \
	-DNANOTTS_ENABLE_LANG_MI=$(LANG_MI) \
	-DNANOTTS_ENABLE_LANG_HAW=$(LANG_HAW) \
	-DNANOTTS_USE_LIBM=$(USE_LIBM) \
	-DNANOTTS_MAX_EVENTS=$(MAX_EVENTS) \
	-DNANOTTS_CONTEXT_BYTES=$(CONTEXT_BYTES) \
	-DNANOTTS_AUDIO_BLOCK=$(AUDIO_BLOCK) \
	-DNANOTTS_CLI_HAVE_PWM=$(BUILD_PWM) \
	-DNANOTTS_CLI_HAVE_PCM=$(BUILD_PCM)

ifeq ($(USE_LIBM),1)
LDLIBS ?= -lm
else
LDLIBS ?=
endif

LIB_OBJS := \
	$(BUILD)/nanotts.o \
	$(BUILD)/nanotts_language.o \
	$(BUILD)/nanotts_prosody.o \
	$(BUILD)/nanotts_ipa.o \
	$(BUILD)/nanotts_voice.o \
	$(BUILD)/nanotts_synth.o

ifeq ($(TEXT_FRONTEND),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_common.o
ifeq ($(LANG_ID),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_id.o
endif
ifeq ($(LANG_SW),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_sw.o
endif
ifeq ($(LANG_ES),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_es.o
endif
ifeq ($(LANG_MS),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_ms.o
endif
ifneq ($(filter 1,$(LANG_MI) $(LANG_HAW)),)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_polynesian.o
endif
ifeq ($(LANG_MI),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_mi.o
endif
ifeq ($(LANG_HAW),1)
LIB_OBJS += $(BUILD)/lang/nanotts_lang_haw.o
endif
endif


OUTPUT_LIBS :=
ifeq ($(BUILD_PWM),1)
OUTPUT_LIBS += $(BUILD)/libnanotts_pwm.a
endif
ifeq ($(BUILD_PCM),1)
OUTPUT_LIBS += $(BUILD)/libnanotts_pcm.a
endif

.PHONY: all clean test size
all: $(BUILD)/nanotts

$(BUILD):
	mkdir -p $@

$(BUILD)/lang:
	mkdir -p $@

$(BUILD)/output:
	mkdir -p $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/lang/%.o: src/lang/%.c | $(BUILD)/lang
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/output/%.o: src/output/%.c | $(BUILD)/output
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libnanotts.a: $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BUILD)/libnanotts_pwm.a: $(BUILD)/output/nanotts_pwm.o
	$(AR) rcs $@ $^

$(BUILD)/libnanotts_pcm.a: $(BUILD)/output/nanotts_pcm.o
	$(AR) rcs $@ $^

$(BUILD)/nanotts_cli.o: tools/nanotts_cli.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/nanotts: $(BUILD)/nanotts_cli.o $(OUTPUT_LIBS) $(BUILD)/libnanotts.a
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test:
	cmake -S . -B $(TEST_BUILD) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DNANOTTS_ENABLE_TEXT_FRONTEND=$(if $(filter 1,$(TEXT_FRONTEND)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_ID=$(if $(filter 1,$(LANG_ID)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_SW=$(if $(filter 1,$(LANG_SW)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_ES=$(if $(filter 1,$(LANG_ES)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_MS=$(if $(filter 1,$(LANG_MS)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_MI=$(if $(filter 1,$(LANG_MI)),ON,OFF) \
		-DNANOTTS_ENABLE_LANG_HAW=$(if $(filter 1,$(LANG_HAW)),ON,OFF) \
		-DNANOTTS_USE_LIBM=$(if $(filter 1,$(USE_LIBM)),ON,OFF) \
		-DNANOTTS_BUILD_PWM=$(if $(filter 1,$(BUILD_PWM)),ON,OFF) \
		-DNANOTTS_BUILD_PCM=$(if $(filter 1,$(BUILD_PCM)),ON,OFF)
	cmake --build $(TEST_BUILD) --parallel
	ctest --test-dir $(TEST_BUILD) --output-on-failure

size: $(BUILD)/libnanotts.a
	size $(LIB_OBJS)

clean:
	rm -rf build build-*
