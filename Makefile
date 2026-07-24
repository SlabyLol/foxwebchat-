#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET & METADATA
#---------------------------------------------------------------------------------
TARGET          := FoxWebChat
BUILD           := build
SOURCES         := source
DATA            := data
INCLUDES        := include
ROMFS           := romfs

APP_TITLE       := FoxWebChat 3DS
APP_DESCRIPTION := Firebase Chat App for Nintendo 3DS
APP_AUTHOR      := DarkFox Co.

#---------------------------------------------------------------------------------
# ARCHITECTURE & COMPILER FLAGS
#---------------------------------------------------------------------------------
ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS          := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17 -Wno-psabi

ASFLAGS         := -g $(ARCH)
LDFLAGS         = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS            := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# LIBRARIES & DIRECTORIES
#---------------------------------------------------------------------------------
CTRULIB         ?= $(DEVKITPRO)/libctru
PORTLIBS        ?= $(DEVKITPRO)/portlibs/3ds
LIBDIRS         := $(CTRULIB) $(PORTLIBS)

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(TOPDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(TOPDIR)/$(BUILD)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES        := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES        := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD       := $(CXX)

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean cia bootstrap

all: bootstrap $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

cia: bootstrap $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $@

bootstrap:
	@mkdir -p $(BUILD)

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia icon.icn banner.bnr

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

.PHONY: all cia

all: $(OUTPUT).3dsx $(OUTPUT).cia

cia: $(OUTPUT).cia

$(OUTPUT).3dsx : $(OUTPUT).elf

$(OUTPUT).elf  : $(OFILES)

$(OUTPUT).cia  : $(OUTPUT).elf
	@bannertool makebanner -i $(TOPDIR)/resources/banner.png -o banner.bnr || true
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i $(TOPDIR)/resources/icon.png -o icon.icn || true
	@makerom -f cia -o $(OUTPUT).cia -elf $(OUTPUT).elf -rsf $(TOPDIR)/app.rsf -icon icon.icn -banner banner.bnr || true
	@echo "built ... $(notdir $@)"

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
