#---------------------------------------------------------------------------------
# Target Configuration
#---------------------------------------------------------------------------------
TARGET          := FoxWebChat
BUILD           := build
SOURCES         := source
DATA            := data
INCLUDES        := include

APP_TITLE       := FoxWebChat 3DS
APP_DESCRIPTION := Firebase Chat App for Nintendo 3DS
APP_AUTHOR      := DarkFox Co.

LIBS            := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lctru -lm

#---------------------------------------------------------------------------------
# Include devkitARM Rules
#---------------------------------------------------------------------------------
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Custom Build Targets
#---------------------------------------------------------------------------------
.PHONY: all clean cia

all: $(TARGET).3dsx $(TARGET).cia

cia: $(TARGET).cia

$(TARGET).cia: $(TARGET).elf
	@bannertool makebanner -i resources/banner.png -o banner.bin || true
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i resources/icon.png -o icon.smdh || true
	@makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf app.rsf -icon icon.smdh -banner banner.bin || true
	@echo "CIA build finished successfully."
