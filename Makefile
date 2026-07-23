ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitpro")
endif

include $(DEVKITPRO)/devkitARM/3ds_rules

TARGET          := FoxWebChat
BUILD           := build
SOURCES         := source

INCLUDES        := -I$(DEVKITPRO)/libctru/include -I$(DEVKITPRO)/portlibs/3ds/include
LIBDIRS         := -L$(DEVKITPRO)/libctru/lib -L$(DEVKITPRO)/portlibs/3ds/lib

ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS          := -g -Wall -O2 -mword-relocations $(ARCH) $(INCLUDES)
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

# LDFLAGS mit 3DSX Specs ist der Schlüssel gegen den Linker-Fehler!
LDFLAGS         := -specs=3dsx.specs $(ARCH) $(LIBDIRS)
LIBS            := -lcurl -lmbedtls -lmbedcrypto -lmbedx509 -lz -lctru -lm

.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	@3dsxtool $(BUILD)/$(TARGET).elf $(TARGET).3dsx
	@echo "Build complete: $(TARGET).3dsx"

$(TARGET).elf:
	@mkdir -p $(BUILD)
	@$(CXX) $(CXXFLAGS) $(SOURCES)/main.cpp $(LDFLAGS) $(LIBS) -o $(BUILD)/$(TARGET).elf

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx
