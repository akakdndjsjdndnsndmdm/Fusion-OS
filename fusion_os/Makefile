# fusion os build system

# compiler and tools
cc = gcc
as = nasm
ld = ld
objcopy = objcopy
iso = grub-mkrescue

# flags
cflags = -m64 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -std=c11 -Wall -Wextra
asflags = -f elf64
ldflags = -nostdlib -z noexecstack

# directories
srcdir = .
geckodir = gecko
dolphindir = dolphin
bootdir = boot
commondir = common

# source files
c_sources = $(shell find $(srcdir) -name "*.c" -not -path "$(bootdir)/*")
asm_sources = $(shell find $(srcdir) -name "*.asm" -not -path "$(bootdir)/*")

# object files
c_objects = $(c_sources:.c=.o)
asm_objects = $(asm_sources:.asm=.o)

# kernel
kernel = build/fusion_os.elf

# iso output
iso_output = build/fusion_os.iso

# build all
all: $(c_objects) $(kernel)
# all: $(kernel) $(iso_output)  # temporarily disable iso creation

# compile c sources
%.o: %.c
	@mkdir -p $(dir $@)
	$(cc) $(cflags) -c $< -o $@

# compile asm sources
%.o: %.asm
	@mkdir -p $(dir $@)
	$(as) $(asflags) $< -o $@

# link kernel
$(kernel): $(c_objects) linker.ld
	@mkdir -p $(dir $@)
	$(ld) $(ldflags) -T linker.ld $(c_objects) -o $@

# create iso
$(iso_output): $(kernel) grub.cfg
	@mkdir -p $(dir $@)/boot/grub
	cp $(kernel) $(dir $@)/boot/
	cp grub.cfg $(dir $@)/boot/grub/
	$(iso) -o $@ $(dir $@)

# run in qemu
qemu: $(kernel)
	qemu-system-x86_64 -kernel $(kernel) -m 512M -smp 2

# clean build artifacts
clean:
	rm -rf build/
	rm -f *.o
	find . -name "*.o" -delete

# install tools
install-tools:
	@echo "installing build tools..."
	apt-get update && apt-get install -y grub-common grub-pc-bin xorriso nasm

# help
help:
	@echo "fusion os build system"
	@echo "make all       - build kernel and iso"
	@echo "make kernel    - build kernel only"
	@echo "make iso       - build iso only"
	@echo "make qemu      - run in qemu"
	@echo "make clean     - clean build artifacts"
	@echo "make install-tools - install required tools"

.PHONY: all kernel iso qemu clean install-tools help