# fusion os build system

# compiler and tools
cc = gcc
as = as
ld = ld
objcopy = objcopy
iso = grub-mkrescue

# flags
cflags = -m64 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -std=c11 -Wall -Wextra
asflags = --64
ldflags = -nostdlib -z noexecstack

# directories
srcdir = .
geckodir = gecko
dolphindir = dolphin
bootdir = boot
commondir = common

# source files
c_sources = $(shell find $(srcdir) -name "*.c" -not -path "$(bootdir)/*")
boot_asm = $(bootdir)/boot.asm

# object files
c_objects = $(c_sources:.c=.o)
boot_object = $(boot_asm:.asm=.o)

kernel = build/fusion_os.elf
kernel_bin = build/fusion_os.bin
iso_output = build/fusion_os.iso

all: $(c_objects) $(kernel) $(kernel_bin) $(iso_output)

# compile c sources
%.o: %.c
	@mkdir -p $(dir $@)
	$(cc) $(cflags) -c $< -o $@

# compile asm sources
$(boot_object): $(boot_asm)
	@mkdir -p $(dir $@)
	$(as) $(asflags) $< -o $@

$(kernel): $(c_objects) $(boot_object) linker.ld
	@mkdir -p $(dir $@)
	$(ld) $(ldflags) -T linker.ld $(boot_object) $(c_objects) -o $@

$(kernel_bin): $(kernel)
	@mkdir -p $(dir $@)
	$(objcopy) -O binary $< $@

# create iso
$(iso_output): $(kernel) grub.cfg
	@mkdir -p $(dir $@)/boot/grub
	cp $(kernel) $(dir $@)/boot/
	cp grub.cfg $(dir $@)/boot/grub/
	$(iso) -o $@ $(dir $@)

qemu: $(kernel)
	qemu-system-x86_64 -kernel $(kernel) -m 512M -smp 2 -membaudit

qemu-cd: $(iso_output)
	qemu-system-x86_64 -cdrom $(iso_output) -m 512M -smp 2 -membaudit

# clean build artifacts
clean:
	rm -rf build/
	rm -f *.o
	find . -name "*.o" -delete

# install tools
install-tools:
	@echo "installing build tools..."
	apt-get update && apt-get install -y grub-common grub-pc-bin xorriso nasm

help:
	@echo "fusion os build system"
	@echo "make all       - build kernel, bin and iso"
	@echo "make kernel    - build kernel only"
	@echo "make bin       - build binary kernel"
	@echo "make iso       - build iso only"
	@echo "make qemu      - run in qemu"
	@echo "make qemu-cd   - run in qemu from cdrom"
	@echo "make clean     - clean build artifacts"
	@echo "make install-tools - install required tools"

.PHONY: all kernel bin iso qemu qemu-cd clean install-tools help
