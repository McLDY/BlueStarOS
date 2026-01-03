# ============================
# Linux Kernel 风格输出控制
# ============================

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

CC_MSG      = echo "  CC          $@"
AS_MSG      = echo "  AS          $@"
LD_MSG      = echo "  LD          $@"
OBJCOPY_MSG = echo "  OBJCOPY     $@"
EFI_MSG     = echo "  EFI         $@"

# ============================
# 工具链配置
# ============================

CC = clang
CFLAGS = -target x86_64-pc-win32-coff -mno-red-zone -fno-stack-protector -fshort-wchar -Wall -Wextra -Iinclude/ \
         -Iinclude/freestnd-c-hdrs/ -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable \
         -Wno-incompatible-library-redeclaration
AS = nasm
LD = lld-link

# ============================
# 目录配置
# ============================

SRCDIR = .
BOOTDIR = $(SRCDIR)/boot
EFIDIR = $(BOOTDIR)
KERNELDIR = $(SRCDIR)/kernel
BUILDDIR ?= $(SRCDIR)/build
EFIBUILDDIR = $(BUILDDIR)/efi
KERNELBUILDDIR = $(BUILDDIR)/kernel

KERNEL_SUBDIRS := $(shell find $(KERNELDIR) -type d)

EFI_SOURCES = $(wildcard $(EFIDIR)/*.c)
KERNEL_ALL_C_SOURCES = $(shell find $(KERNELDIR) -type f -name '*.c')
KERNEL_ALL_ASM_SOURCES = $(shell find $(KERNELDIR) -type f -name '*.asm')

EFI_OBJECTS = $(patsubst $(EFIDIR)/%.c, $(EFIBUILDDIR)/%.o, $(EFI_SOURCES))

KERNEL_C_OBJECTS = $(patsubst $(KERNELDIR)/%.c, $(KERNELBUILDDIR)/%.o, $(KERNEL_ALL_C_SOURCES))
KERNEL_ASM_OBJECTS = $(patsubst $(KERNELDIR)/%.asm, $(KERNELBUILDDIR)/%.o, $(KERNEL_ALL_ASM_SOURCES))
KERNEL_OBJECTS = $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS)

# ============================
# 入口文件自动检测
# ============================

ifneq ($(wildcard $(KERNELDIR)/kmain.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/kmain.o
else ifneq ($(wildcard $(KERNELDIR)/kernel.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/kernel.o
else ifneq ($(wildcard $(KERNELDIR)/main.c),)
    ENTRY_OBJECT = $(KERNELBUILDDIR)/main.o
else
    ENTRY_OBJECT = $(firstword $(KERNEL_OBJECTS))
endif

OTHER_OBJECTS = $(filter-out $(ENTRY_OBJECT), $(KERNEL_OBJECTS))

KERNEL_INCLUDE_DIRS = $(addprefix -I, $(KERNEL_SUBDIRS))

KERNEL_CFLAGS = -target x86_64-linux-gnu -ffreestanding -fno-builtin \
                -fno-stack-protector -mno-red-zone -Wall -Wextra -O2 $(INCLUDE) \
                -mgeneral-regs-only -mno-sse -mno-mmx -Iinclude/ -Iinclude/freestnd-c-hdrs/ \
                $(KERNEL_INCLUDE_DIRS) -Wno-unused-variable -Wno-unused-parameter \
                -Wno-unused-but-set-variable -Wno-unused-function -Wno-comment \
                -Wno-pragma-pack -Wno-self-assign -Wno-incompatible-library-redeclaration \
                -Wno-sign-compare -Wno-tautological-constant-out-of-range-compare \
                -Wno-incompatible-library-redeclaration

.PHONY: all clean uefi kernel disk run debug list-sources list-dirs

# ============================
# 默认目标
# ============================

all: uefi kernel disk

# ============================
# UEFI 构建
# ============================

uefi: $(EFIBUILDDIR)/bootx64.efi

$(EFIBUILDDIR)/bootx64.efi: $(EFI_OBJECTS)
	$(Q)mkdir -p $(EFIBUILDDIR)
	$(Q)$(EFI_MSG)
	$(Q)$(LD) /subsystem:efi_application /entry:efi_main /out:$@ $^

$(EFIBUILDDIR)/%.o: $(EFIDIR)/%.c
	$(Q)mkdir -p $(EFIBUILDDIR)
	$(Q)$(CC_MSG)
	$(Q)$(CC) $(CFLAGS) -I$(EFIDIR) -c $< -o $@


# ============================
# Kernel 构建
# ============================

kernel: $(KERNELBUILDDIR)/kernel.bin

$(KERNELBUILDDIR)/kernel.bin: $(KERNELBUILDDIR)/kernel.elf
	$(Q)$(OBJCOPY_MSG)
	$(Q)llvm-objcopy -O binary $< $@

$(KERNELBUILDDIR)/kernel.elf: $(KERNEL_OBJECTS) $(KERNELDIR)/kernel.ld
	$(Q)mkdir -p $(KERNELBUILDDIR)
	$(Q)$(LD_MSG)
	$(Q)ld.lld -nostdlib -T $(KERNELDIR)/kernel.ld -o $@ $(ENTRY_OBJECT) $(OTHER_OBJECTS)

$(KERNELBUILDDIR)/%.o: $(KERNELDIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC_MSG)
	$(Q)$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(KERNELBUILDDIR)/%.o: $(KERNELDIR)/%.asm
	$(Q)mkdir -p $(dir $@)
	$(Q)$(AS_MSG)
	$(Q)$(AS) -f elf64 -o $@ $<

# ============================
# Disk Image
# ============================

disk: $(BUILDDIR)/disk.img

$(BUILDDIR)/disk.img: uefi kernel
	$(Q)mkdir -p $(BUILDDIR)/EFI/BOOT
	$(Q)dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	$(Q)mkfs.fat -F 16 $@ >/dev/null 2>&1
	$(Q)mmd -i $@ ::EFI
	$(Q)mmd -i $@ ::EFI/BOOT
	$(Q)mcopy -i $@ $(EFIBUILDDIR)/bootx64.efi ::EFI/BOOT/bootx64.efi
	$(Q)mcopy -i $@ $(KERNELBUILDDIR)/kernel.bin ::kernel.bin

# ============================
# Run & Debug
# ============================

run: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio

debug: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio -s -S

# ============================
# Clean
# ============================

clean:
	rm -rf $(BUILDDIR)

# ============================
# Info
# ============================

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
