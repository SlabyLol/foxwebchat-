# Target settings
TARGET      := foxwebchat
BUILD       := build
SOURCES     := source
INCLUDES    := include
ICON        := resources/icon.png

APP_TITLE   := FoxWebChat
APP_AUTHOR  := DarkFox Co.
APP_VERSION := 1.0.0

# DevkitPro environment setup
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=/opt/devkitpro/devkitARM")
endif

include $(DEVKITARM)/3ds_rules

PORTLIBS_PATH ?= $(DEVKITPRO)/portlibs

# Flags & Libraries
ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -Wall -O2 -mword-relocations $(ARCH)
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions

LIBS        := -lcurl -lmbedtls -lmbedcrypto -lmbedx509 -lz -lctru -lm
LIBDIRS     := $(CTRULIB) $(PORTLIBS_PATH)/3ds $(PORTLIBS_PATH)/armv6k

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

.PHONY: all clean 3dsx

all: 3dsx

3dsx: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	@echo "Building $(TARGET).3dsx using resources icon..."
	@if [ -f $(ICON) ]; then \
		bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_TITLE)" -p "$(APP_AUTHOR)" -i $(ICON) -o icon.bin; \
	else \
		bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_TITLE)" -p "$(APP_AUTHOR)" -o icon.bin; \
	fi
	@3dsxtool $(TARGET).elf $(TARGET).3dsx --smdh=icon.bin
	@rm -f icon.bin

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf icon.bin

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

$(TARGET).elf: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR:/=)/Makefile
	@cp $(BUILD)/$(TARGET).elf $(TARGET).elf 2>/dev/null || true

else

DEPENDS := $(OFILES:.o=.d)

CFLAGS   += $(INCLUDE) -D__3DS__
CXXFLAGS += $(INCLUDE) -D__3DS__

$(OUTPUT).elf: $(OFILES)
	@echo linking $(notdir $@)
	@$(CXX) $(ARCH) $(OFILES) $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(LIBS) -o $@

-include $(DEPENDS)

endif
