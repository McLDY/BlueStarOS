# 工具链配置
CC = clang
CFLAGS = -target x86_64-pc-win32-coff -mno-red-zone -fno-stack-protector -fshort-wchar -Wall -Wextra -Iinclude/ \
         -Iinclude/freestnd-c-hdrs/ -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable
AS = nasm
LD = lld-link

# 目录配置
SRCDIR = .
BOOTDIR = $(SRCDIR)/boot
EFIDIR = $(BOOTDIR)
KERNELDIR = $(SRCDIR)/kernel
BUILDDIR ?= $(SRCDIR)/build
EFIBUILDDIR = $(BUILDDIR)/efi
KERNELBUILDDIR = $(BUILDDIR)/kernel

# 递归查找所有kernel子目录（排除一些特殊目录）
KERNEL_SUBDIRS := $(shell find $(KERNELDIR) -type d)

# 自动发现源文件（递归查找所有kernel目录下的文件）
EFI_SOURCES = $(wildcard $(EFIDIR)/*.c)
KERNEL_ALL_C_SOURCES = $(shell find $(KERNELDIR) -type f -name '*.c')
KERNEL_ALL_ASM_SOURCES = $(shell find $(KERNELDIR) -type f -name '*.asm')

# 生成对象文件列表（维护完整的目录结构）
EFI_OBJECTS = $(patsubst $(EFIDIR)/%.c, $(EFIBUILDDIR)/%.o, $(EFI_SOURCES))

# Kernel对象文件（保持相对路径结构）
KERNEL_C_OBJECTS = $(patsubst $(KERNELDIR)/%.c, $(KERNELBUILDDIR)/%.o, $(KERNEL_ALL_C_SOURCES))
KERNEL_ASM_OBJECTS = $(patsubst $(KERNELDIR)/%.asm, $(KERNELBUILDDIR)/%.o, $(KERNEL_ALL_ASM_SOURCES))
KERNEL_OBJECTS = $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS)

# 【核心修复：定义入口文件】
# 找到入口对象文件（通常为kmain.o或kernel.o）
ifneq ($(wildcard $(KERNELDIR)/kmain.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/kmain.o
else ifneq ($(wildcard $(KERNELDIR)/kernel.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/kernel.o
else ifneq ($(wildcard $(KERNELDIR)/main.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/main.o
else
    # 如果没有找到默认入口，取第一个对象文件
    ENTRY_OBJECT = $(firstword $(KERNEL_OBJECTS))
endif

OTHER_OBJECTS = $(filter-out $(ENTRY_OBJECT), $(KERNEL_OBJECTS))

# 为所有kernel子目录生成include路径（正确格式）
KERNEL_INCLUDE_DIRS = $(addprefix -I, $(KERNEL_SUBDIRS))

# 编译标志
KERNEL_CFLAGS = -target x86_64-linux-gnu -ffreestanding -fno-builtin \
                -fno-stack-protector -mno-red-zone -Wall -Wextra -O2 $(INCLUDE) \
				-mgeneral-regs-only -mno-sse -mno-mmx -Iinclude/ -Iinclude/freestnd-c-hdrs/ \
                $(KERNEL_INCLUDE_DIRS) -Wno-unused-variable -Wno-unused-parameter \
                -Wno-unused-but-set-variable -Wno-unused-function -Wno-comment

.PHONY: all clean uefi kernel disk run debug list-sources list-dirs

# 默认目标
all: uefi kernel disk

# 主要目标
uefi: $(EFIBUILDDIR)/bootx64.efi
kernel: $(KERNELBUILDDIR)/kernel.bin

# UEFI引导程序链接
$(EFIBUILDDIR)/bootx64.efi: $(EFI_OBJECTS)
	@mkdir -p $(EFIBUILDDIR)
	$(LD) /subsystem:efi_application /entry:efi_main /out:$@ $^

# UEFI C文件编译规则
$(EFIBUILDDIR)/%.o: $(EFIDIR)/%.c
	@mkdir -p $(EFIBUILDDIR)
	$(CC) $(CFLAGS) -I$(EFIDIR) -c $< -o $@

# 内核最终二进制文件
$(KERNELBUILDDIR)/kernel.bin: $(KERNELBUILDDIR)/kernel.elf
	llvm-objcopy -O binary $< $@

# 内核ELF链接（使用固定的 ENTRY_OBJECT 顺序）
$(KERNELBUILDDIR)/kernel.elf: $(KERNEL_OBJECTS) $(KERNELDIR)/kernel.ld
	@mkdir -p $(KERNELBUILDDIR)
	ld.lld -nostdlib -T $(KERNELDIR)/kernel.ld -o $@ $(ENTRY_OBJECT) $(OTHER_OBJECTS)

# 通用编译规则：Kernel C 文件（适用于所有子目录）
$(KERNELBUILDDIR)/%.o: $(KERNELDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# 通用编译规则：Kernel ASM 文件（适用于所有子目录）
$(KERNELBUILDDIR)/%.o: $(KERNELDIR)/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 -o $@ $<

# 磁盘映像目标
disk: $(BUILDDIR)/disk.img

$(BUILDDIR)/disk.img: uefi kernel
	@mkdir -p $(BUILDDIR)/EFI/BOOT
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 16 $@ 2>/dev/null
	mmd -i $@ ::EFI
	mmd -i $@ ::EFI/BOOT
	mcopy -i $@ $(EFIBUILDDIR)/bootx64.efi ::EFI/BOOT/bootx64.efi
	mcopy -i $@ $(KERNELBUILDDIR)/kernel.bin ::kernel.bin

# 运行
run: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio

# 调试运行
debug: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio -s -S

# 清理
clean:
	rm -rf $(BUILDDIR)

# 辅助显示发现的文件
list-sources:
	@echo "Entry Object: $(ENTRY_OBJECT)"
	@echo "Kernel C Sources: $(words $(KERNEL_ALL_C_SOURCES)) files"
	@echo "Kernel ASM Sources: $(words $(KERNEL_ALL_ASM_SOURCES)) files"
	@echo "Total Objects: $(words $(KERNEL_OBJECTS))"
	@echo ""
	@echo "Build directory will contain:"
	@for obj in $(sort $(KERNEL_OBJECTS)); do \
		echo "  $$obj"; \
	done

list-dirs:
	@echo "Kernel subdirectories:"
	@for dir in $(sort $(KERNEL_SUBDIRS)); do \
		echo "  $$dir"; \
	done