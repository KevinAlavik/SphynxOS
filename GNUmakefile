override MAKEFLAGS += -rR

override IMAGE_NAME := Sphynx
override RAMFS_DIR := ramfs
override APPS_DIR := apps

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run
run: $(IMAGE_NAME).iso
	qemu-system-x86_64 -M q35 -m 2G -smp 4 -cdrom $(IMAGE_NAME).iso -boot d -debugcon stdio

.PHONY: run-no-display
run-no-display: $(IMAGE_NAME).iso
	qemu-system-x86_64 -M q35 -m 2G -smp 4 -cdrom $(IMAGE_NAME).iso -boot d -debugcon stdio -display none

.PHONY: run-debug
run-debug: $(IMAGE_NAME).iso
	qemu-system-x86_64 -M q35 -m 2G -smp 4 -cdrom $(IMAGE_NAME).iso -boot d -debugcon stdio -d int -no-reboot -no-shutdown

.PHONY: run-uefi
run-uefi: ovmf $(IMAGE_NAME).iso
	qemu-system-x86_64 -M q35 -m 2G -smp 4 -bios ovmf/OVMF.fd -cdrom $(IMAGE_NAME).iso -boot d -debugcon stdio

.PHONY: run-hdd
run-hdd: $(IMAGE_NAME).hdd
	qemu-system-x86_64 -m 2G -smp 4 -hda $(IMAGE_NAME).hdd -debugcon stdio

.PHONY: run-hdd-uefi
run-hdd-uefi: ovmf $(IMAGE_NAME).hdd
	qemu-system-x86_64 -M q35 -smp 4 -m 2G -bios ovmf/OVMF.fd -hda $(IMAGE_NAME).hdd -debugcon stdio

.PHONY: ramfs
ramfs:
	$(MAKE) -C $(APPS_DIR)
	mkdir -p $(RAMFS_DIR)/Applications/
	mv $(APPS_DIR)/bin/** $(RAMFS_DIR)/Applications/
	cd $(RAMFS_DIR); tar -cvf ../ramfs.img *

ovmf:
	mkdir -p ovmf
	cd ovmf && curl -Lo OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd

limine/limine:
	rm -rf limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-binary --depth=1
	$(MAKE) -C limine

.PHONY: kernel
kernel:
	$(MAKE) -C kernel

$(IMAGE_NAME).iso: limine/limine kernel ramfs
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v kernel/bin/sphynxkrnl.sys iso_root/boot/
	mkdir -p iso_root/boot/limine
	cp -v limine.cfg limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	cp -v ramfs.img iso_root/boot/ramfs
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root

$(IMAGE_NAME).hdd: limine/limine kernel ramfs
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
	./limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.cfg limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M ramfs.img ::/boot/ramfs

.PHONY: clean
clean:
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd ramfs.img
	$(MAKE) -C kernel clean
	$(MAKE) -C $(APPS_DIR) clean
	$(MAKE) -C libc clean

.PHONY: distclean
distclean: clean
	rm -rf limine ovmf
	rm -rf $(RAMFS_DIR)/Applications
	$(MAKE) -C kernel distclean
