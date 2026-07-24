#---------------------------------------------------------------------------------
# TARGET & APP METADATA
#---------------------------------------------------------------------------------
TARGET          := FoxWebChat
BUILD           := build
SOURCES         := source
DATA            := data
INCLUDES        := include

APP_TITLE       := FoxWebChat 3DS
APP_DESCRIPTION := Firebase Chat App for Nintendo 3DS
APP_AUTHOR      := DarkFox Co.

#---------------------------------------------------------------------------------
# DEVKITPRO PATHS
#---------------------------------------------------------------------------------
PORTLIBS        := $(DEVKITPRO)/portlibs/3ds
CTRULIB         := $(DEVKITPRO)/libctru

# Architecture parameters
ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS          := -Wall -O2 -mword-relocations $(ARCH) -I$(PORTLIBS)/include -I$(CTRULIB)/include
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

LDFLAGS         := -specs=3dsx.specs $(ARCH) -L$(PORTLIBS)/lib -L$(CTRULIB)/lib
LIBS            := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lctru -lm

# Include standard devkitARM rules FIRST
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# OVERRIDE LINK RULE (Fixes 'ld: unrecognized option -specs')
#---------------------------------------------------------------------------------
# Force GCC driver to handle linking instead of raw 'ld'
$(TARGET).elf: $(OFILES)
	@echo linking $(@)
	$(CXX) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

.PHONY: all clean cia

all: $(TARGET).3dsx $(TARGET).cia

cia: $(TARGET).cia

$(TARGET).cia: $(TARGET).elf
	@bannertool makebanner -i resources/banner.png -o banner.bin || true
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i resources/icon.png -o icon.smdh || true
	@makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf app.rsf -icon icon.smdh -banner banner.bin || true
	@echo "CIA build completed successfully!"
