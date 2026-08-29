BUILD_DIR = Build
USER_DIR = user
KERNEL_DIR = kernel
STAGING_DIR = $(BUILD_DIR)/staging

all: $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/user.elf

CC = i686-elf-gcc
CFLAGS = -ffreestanding -m32 -fno-pic -Wall -Wextra -Iinclude
LD = i686-elf-ld

C_SRCS        := $(shell find . -name "*.c")
ASM_SRCS      := $(shell find . -name "*.asm")
HEADERS	   	  := $(shell find include -name "*.h")

KERNEL_C_SRCS 	:= $(filter ./$(KERNEL_DIR)/%,$(C_SRCS))
KERNEL_ASM_SRCS := $(filter ./$(KERNEL_DIR)/%,$(ASM_SRCS))
USER_C_SRCS     := $(filter ./$(USER_DIR)/%,$(C_SRCS))
USER_ASM_SRCS   := $(filter ./$(USER_DIR)/%,$(ASM_SRCS))

# Derive object paths from source files
KERNEL_C_OBJS   := $(patsubst ./%.c,   $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst ./%.asm, $(BUILD_DIR)/%.asm.o, $(KERNEL_ASM_SRCS))
KERNEL_OBJS     := $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

USER_C_OBJS   := $(patsubst ./%.c,   $(BUILD_DIR)/%.o, $(USER_C_SRCS))
USER_ASM_OBJS := $(patsubst ./%.asm, $(BUILD_DIR)/%.asm.o, $(USER_ASM_SRCS))
USER_OBJS     := $(USER_C_OBJS) $(USER_ASM_OBJS)


$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	nasm -f elf32 -Iinclude $< -o $@

$(BUILD_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(KERNEL_OBJS)
	$(LD) -m elf_i386 -nostdlib -T $(KERNEL_DIR)/linker.ld -o $(BUILD_DIR)/kernel.elf $^

$(BUILD_DIR)/user.elf: $(USER_OBJS)
	$(LD) -m elf_i386 -nostdlib -T $(USER_DIR)/linker.ld -o $(BUILD_DIR)/user.elf $^

$(STAGING_DIR)/user.elf: $(BUILD_DIR)/user.elf
	@mkdir -p $(STAGING_DIR)
	cp $(BUILD_DIR)/user.elf $(STAGING_DIR)/user.elf

$(BUILD_DIR)/disk.img: $(STAGING_DIR)/user.elf
	rm -f $(BUILD_DIR)/disk.img
	dd if=/dev/zero of=$(BUILD_DIR)/disk.img bs=1M count=16 status=none
	mke2fs -F -t ext2 -b 1024 -d $(STAGING_DIR) $(BUILD_DIR)/disk.img

.PHONY: fs_img
fs_img: $(BUILD_DIR)/disk.img


run:
	qemu-system-i386 -drive file=$(BUILD_DIR)/disk.img,format=raw,if=ide -d int,cpu_reset -no-reboot -kernel $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/qemu_log.txt 2>&1

clear:
	rm -rf $(BUILD_DIR)/*
