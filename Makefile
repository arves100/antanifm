#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

MAKEFILE_ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(MAKEFILE_ROOT_DIR)/elf_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# SPECS is the directory containing the important build and link files
#---------------------------------------------------------------------------------
export TARGET		:=	antanifm
export BUILD		?=	debug

R_SOURCES			:=	
SOURCES				:=	source source/lib source/lib/fatfs externals/inih

R_INCLUDES			:=	
INCLUDES 			:=	source source/lib source/lib/fatfs externals/inih

DATA				:=	

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH			:=	-march=armv5te -mcpu=arm926ej-s -marm -mthumb-interwork -mbig-endian -mfloat-abi=soft

CFLAGS			:=	-g -std=c11 -Os \
					-fomit-frame-pointer -fdata-sections -ffunction-sections \
					$(ARCH) -nostartfiles

CFLAGS			+=	$(INCLUDE) -D_GNU_SOURCE -DCAN_HAZ_IRQ -fno-builtin-printf -Wno-nonnull -Werror=implicit -DNAND_WRITE_ENABLED

CXXFLAGS		:=	$(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS			:=	-g $(ARCH)
LDFLAGS			 =	-g --specs=../stub.specs $(ARCH) -Wl,--gc-sections,-Map,$(TARGET).map \
					-L$(DEVKITARM)/lib/gcc/arm-none-eabi/$(GCC_VERSION)/be -L$(DEVKITARM)/arm-none-eabi/lib/be \
					-z max-page-size=4096 -nostartfiles

LIBS			:=	

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS			:=	

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export ROOTDIR	:=	$(CURDIR)
export OUTPUT	:=	$(CURDIR)/$(TARGET)

SOURCES         := $(SOURCES) $(foreach dir,$(R_SOURCES), $(dir) $(filter %/, $(wildcard $(dir)/*/)))
INCLUDES        := $(INCLUDES) $(foreach dir,$(R_INCLUDES), $(dir) $(filter %/, $(wildcard $(dir)/*/)))

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.S=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@$(MAKE) -C $(ROOTDIR)/elfloader clean
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT)-strip.elf


#---------------------------------------------------------------------------------
else

DEPENDS		:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ELFLOADER = $(ROOTDIR)/elfloader/elfloader.bin

$(ROOTDIR)/ios.img: $(OUTPUT)-strip.elf $(ELFLOADER)
	@python3 $(ROOTDIR)/castify.py $(ELFLOADER) $< $@ false

$(OUTPUT)-strip.elf: $(OUTPUT).elf
	$(STRIP) $< -o $@

$(OUTPUT).elf: $(OFILES)

$(ELFLOADER):
	@$(MAKE) -C $(ROOTDIR)/elfloader

-include $(DEPENDS)


#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
