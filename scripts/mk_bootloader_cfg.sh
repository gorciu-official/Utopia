#!/usr/bin/env bash 

GRUB_CFG=src/build/grub.cfg
LIMINE_CFG=src/build/limine.conf

COLOR_GREEN=$'\x1b[32m'
COLOR_YELLOW=$'\x1b[33m'
COLOR_GRAY=$'\x1b[90m'
COLOR_RESET=$'\x1b[0m'

if [[ "$1" = "-e" && -f $GRUB_CFG && -f $LIMINE_CFG ]]; then 
    printf "%sThere is nothing to do%s\n" "$COLOR_GRAY" "$COLOR_RESET"
    exit 0
fi

echo Creating bootloader configuration for GRUB and Limine

printf ' Starting %s[Limine bootloader configuration generation]%s\n' "$COLOR_YELLOW" "$COLOR_RESET"

echo 'set default=0'                                                                        > $GRUB_CFG
echo 'set timeout=5'                                                                       >> $GRUB_CFG
echo 'menuentry "Utopia" {'                                                                >> $GRUB_CFG 
echo '    set gfxpayload=800x600x32'                                                       >> $GRUB_CFG
echo '    multiboot /boot/kernel.bin'                                                      >> $GRUB_CFG
if [[ -d initramfs || "$MKBC_INITRAMFS_SUPPORT" = "true" ]]; then
    printf '  %sFound initramfs, adding module initramfs support.%s\n'                     "$COLOR_GRAY" "$COLOR_RESET"
    echo '    module /initramfs.tar initramfs.tar'                                         >> $GRUB_CFG
fi
if [[ -f font.psf1 || "$MKBC_CUSTOM_FONT_SUPPORT" = "true" ]]; then
    printf '  %sFound custom font, adding module font loading.%s\n'                        "$COLOR_GRAY" "$COLOR_RESET"
    echo '    module /font.psf1 font.psf1'                                                 >> $GRUB_CFG
fi
echo '    boot'                                                                            >> $GRUB_CFG
echo '}'                                                                                   >> $GRUB_CFG

printf ' Finished %s[GRUB bootloader configuration generation]%s\n' "$COLOR_YELLOW" "$COLOR_RESET"

printf ' Starting %s[Limine bootloader configuration generation]%s\n' "$COLOR_YELLOW" "$COLOR_RESET"

echo 'timeout: 3'                                                                           > $LIMINE_CFG 
echo '/Utopia'                                                                             >> $LIMINE_CFG
echo '    protocol: limine'                                                                >> $LIMINE_CFG
echo '    kernel_path: boot():/kernel.bin'                                                 >> $LIMINE_CFG
if [[ -d initramfs || "$MKBC_INITRAMFS_SUPPORT" = "true" ]]; then
    printf '  %sFound initramfs, adding module initramfs support.%s\n'                     "$COLOR_GRAY" "$COLOR_RESET"
    echo '    module_path: boot():/initramfs.tar'                                          >> $LIMINE_CFG
fi
if [[ -f font.psf1 || "$MKBC_CUSTOM_FONT_SUPPORT" = "true" ]]; then
    printf '  %sFound custom font, adding module font loading.%s\n'                        "$COLOR_GRAY" "$COLOR_RESET"
    echo '    module_path: boot():/font.psf1'                                              >> $LIMINE_CFG
fi

printf ' Finished %s[Limine bootloader configuration generation]%s\n' "$COLOR_YELLOW" "$COLOR_RESET"

printf '%sDone.%s Finished 2 of 2 tasks.\n' "$COLOR_GREEN" "$COLOR_RESET"
