BUILD_DIR = Build
BOOT_DIR = boot

all: $(BUILD_DIR)/kernel.elf

CC = i686-elf-gcc
CFLAGS = -ffreestanding -m32 -fno-pic -Wall -Wextra -Iinclude
LD = i686-elf-ld

C_SRCS        := $(shell find . -name "*.c")
ASM_SRCS      := $(shell find . -name "*.asm")
HEADERS	   	  := $(shell find include -name "*.h")
KERNEL_ASM_SRCS := $(filter-out ./$(BOOT_DIR)/%,$(ASM_SRCS))

# Derive object paths from source files
C_OBJS   := $(patsubst ./%.c,   $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst ./%.asm, $(BUILD_DIR)/%.asm.o, $(KERNEL_ASM_SRCS))
OBJS     := $(C_OBJS) $(ASM_OBJS)


# $(BUILD_DIR)/boot.bin: $(BOOT_DIR)/boot.asm
# 	nasm -f bin $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	nasm -f elf32 -Iinclude $< -o $@

$(BUILD_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS)
	$(LD) -m elf_i386 -nostdlib -T linker.ld -o $(BUILD_DIR)/kernel.elf $^

# $(BUILD_DIR)/os-image.bin: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin
# 	cat $^ > $@
# 	truncate -s 1M $@

run:
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf

clear:
	rm -rf $(BUILD_DIR)/*
