# this path needs to be customize.
COMMON = $(CURDIR)/ch32v003
TOOLCHAIN = ~/.riscv_toolchain/bin/riscv-none-embed
OPENOCD = ~/.riscv_toolchain/OpenOCD/bin

# follow code do not need to modify.
NAME = $(notdir $(CURDIR))

CC = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump

INCLUDES = \
	-I$(COMMON)/Core \
	-I$(COMMON)/Peripheral/inc/ \
        -I$(COMMON)/Debug \
	-I$(CURDIR)

SOURCES = \
	$(COMMON)/Startup/startup_ch32v00x.S \
	$(COMMON)/Core/*.c \
	$(COMMON)/Peripheral/src/*.c \
        $(COMMON)/Debug/*.c \
	$(wildcard $(CURDIR)/*.c)
	
LIBRARY = \
	-lc -lm -lnosys

OBJECTS = $(SOURCES:%.c=%.o)

DEFINES = -DSYSCLK_FREQ_48MHZ_HSI=48000000 -DIS31FL3731_COMPATIBLE
	
CFLAGS = \
	-march=rv32ecxw -mabi=ilp32e -msmall-data-limit=8 \
	-msave-restore -Os -fmessage-length=0 -fsigned-char \
	-ffunction-sections -fdata-sections -fno-common \
	-Wunused -Wuninitialized -g -nostartfiles \
        -Xlinker --gc-sections \
        -Xlinker --print-memory-usage \
	-Wl,-Map,"$@.map" \
        --specs=nano.specs --specs=nosys.specs \
	-T "$(COMMON)/Ld/Link.ld" \
	$(INCLUDES) $(DEFINES)

$(NAME): $(SOURCES)
	@echo 'COMPILE $(NAME) ...'
	@$(CC) $(CFLAGS) $^ $(LIBRARY) -o $(CURDIR)/$@.elf
	@$(OBJCOPY) -O ihex $(CURDIR)/$@.elf $(CURDIR)/$@.hex
	@$(OBJCOPY) -O binary $(CURDIR)/$@.elf $(CURDIR)/$@.bin
	@$(OBJDUMP) --all-headers --demangle --disassemble --source $@.elf > $@.lst

flash:
	@-killall openocd
	$(OPENOCD)/openocd -f $(OPENOCD)/wch-riscv.cfg -c init -c halt -c "flash write_image $(CURDIR)/$(NAME).bin" -c exit
	$(OPENOCD)/openocd -f $(OPENOCD)/wch-riscv.cfg -c init -c halt -c "flash verify_image $(CURDIR)/$(NAME).bin" -c exit
	
debug:
	@-killall openocd
	$(OPENOCD)/openocd -f $(OPENOCD)/wch-riscv.cfg &
	$(TOOLCHAIN)-gdb -ex "file $(CURDIR)/$(NAME).elf" -ex "b main" -ex "target remote 127.0.0.1:3333"
	@-killall openocd

test:
	gcc ./misc/test.c -o test -lusb-1.0 $(DEFINES)

clean:
	@rm -f $(CURDIR)/*.elf $(CURDIR)/*.hex $(CURDIR)/*.map $(CURDIR)/*.lst $(CURDIR)/*.bin

