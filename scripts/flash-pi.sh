#!/bin/bash

# Simple script to build floss, strip the ELF to a binary file, and transfer the
# binary file to an SD card for a Raspberry Pi 5

SD_CARD_DEVICE=/dev/mmcblk0p1

set -e
set -x

# Build the kernel
cmake --build build

# Copy from ELF to bin file
aarch64-linux-gnu-objcopy -O binary build/floss_kernel build/floss_kernel.bin

# Mount SD card
sudo mount $SD_CARD_DEVICE /mnt/bootfs

# Update file
rm /mnt/bootfs/kernel_2712.img
cp build/floss_kernel.bin /mnt/bootfs/kernel_2712.img
sudo sync

# Unmount SD card
sudo umount $SD_CARD_DEVICE
