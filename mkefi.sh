set +x
dd if=/dev/zero of=test.img bs=512 count=4096
#mkfs.msdos test.img
mformat -i test.img
echo here
mcopy -i test.img apxh/efi/apxh.efi ::apxh.efi
mcopy -i test.img example/kern/example ::kernel.elf
mcopy -i test.img example/user/exuser ::user.elf
