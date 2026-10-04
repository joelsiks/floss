#!/bin/bash

# Simple script to toggle OCD 
# Simple script to build floss, strip the ELF to a binary file, and transfer the
# binary file to an SD card for a Raspberry Pi 5.

SD_CARD_DEVICE=/dev/mmcblk0p1
SD_CARD_MOUNT=/mnt/bootfs

CONFIG=/mnt/bootfs/config.txt

set -e
set -x
# Mount SD card
sudo mount $SD_CARD_DEVICE $SD_CARD_MOUNT

# Update file
if grep -q '^enable_jtag_gpio=1$' "$CONFIG"; then
    sed -i 's/^enable_jtag_gpio=1$/#enable_jtag_gpio=1/' "$CONFIG"
    echo "Disabling OCD via commenting out enable_jtag_gpi"
else
    sed -i 's/^#enable_jtag_gpio=1$/enable_jtag_gpio=1/' "$CONFIG"
    echo "Enabling OCD via removing commented enable_jtag_gpi"
fi

sudo sync

# Unmount SD card
sudo umount $SD_CARD_DEVICE
