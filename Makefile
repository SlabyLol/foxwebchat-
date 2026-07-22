FE3DS_PATH := $(DEVKITPRO)/libctru

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

TARGET          := FoxWebChat
BUILD           := build
SOURCES         := source
INCLUDES        := include

ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS          := -g -Wall -O2 -mword-relocations $(ARCH)
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
LIBS            := -lcurl -lmbedtls -lmbedcrypto -lmbedx509 -lctru -lm

LIBDIRS         := $(PORTLIBS)/3ds $(CTRULIB)

export APP_TITLE        := FoxWebChat 3DS
export APP_DESCRIPTION  := Chat app connected to FoxWebChat Firebase
export APP_AUTHOR       := DarkFox Co.

.PHONY: all clean

all: $(TARGET).cia $(TARGET).3dsx

$(TARGET).cia: $(TARGET).elf
$(TARGET).3dsx: $(TARGET).elf

$(TARGET).elf:
	@mkdir -p $(BUILD)
	@$(CXX) $(CXXFLAGS) -I$(SOURCES) $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(SOURCES)/main.cpp $(LIBS) -o $(BUILD)/$(TARGET).elf
	@3dsxtool $(BUILD)/$(TARGET).elf $(TARGET).3dsx
	@bannertool makebanner -i $(DEVKITPRO)/libctru/default_banner.png -a $(DEVKITPRO)/libctru/default_audio.wav -o $(BUILD)/banner.bin
	@bannertool makesmdh -i icon.png -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -o $(BUILD)/icon.smdh
	@makerom -f cia -o $(TARGET).cia -elf $(BUILD)/$(TARGET).elf -rsf $(DEVKITPRO)/libctru/default.rsf -banner $(BUILD)/banner.bin -smdh $(BUILD)/icon.smdh

clean:
	@rm -rf $(BUILD) $(TARGET).cia $(TARGET).3dsx
