# DIRPATH is an absolute path to the directory containing this script.
DIRPATH="$(
    cd -- "$(dirname "$0")" >/dev/null 2>&1
    pwd -P
)"
PARPATH="$(dirname "$DIRPATH")"

# echo "Compiling OVMF.fd and Bootloader..."
# . $PARPATH/BootloaderPkg/setup.sh

# Replace ZIG_PATH with the appropriate path
# to the Zig compiler here.
ZIG_PATH=$HOME/Downloads/zig-macos-x86_64-0.11.0-dev.3395+1e7dcaa3a/zig

rm -rf kernel.o 2> /dev/null
rm -rf hda-contents/kernel.bin 2> /dev/null

# Compile
$ZIG_PATH cc \
    --target=x86_64-uefi-msvc \
    -Wl,-e,KernMain \
    -Wl,--subsystem,efi_application \
    -fuse-ld=lld-link \
    -ffreestanding \
    -nostdlib \
    -fmacro-prefix-map=src/Kernel/= \
    -fmacro-prefix-map=src/Kernel/Drivers/= \
    -fmacro-prefix-map=src/Kernel/Drivers/IO/= \
    -fmacro-prefix-map=src/Kernel/Drivers/USB/= \
    -fmacro-prefix-map=src/Kernel/Graphics/= \
    -fmacro-prefix-map=src/Kernel/Memory/= \
    -fmacro-prefix-map=src/Kernel/Util/= \
    -fmacro-prefix-map=src/Kernel/Math/= \
    -DDEBUG_MEMORY \
    -DDEBUG_GRAPHICS \
    \
    -o kernel.o \
    src/Kernel/Kernel.c \
    src/Kernel/Memory/KernMem.c \
    src/Kernel/Memory/KernMemoryManager.c \
    src/Kernel/Graphics/KernGraphics.c \
    src/Kernel/Graphics/KernFontParser.c \
    src/Kernel/Graphics/KernText.c \
    src/Kernel/Drivers/IO/io.c \
    src/Kernel/Drivers/IO/serial.c \
    src/Kernel/Util/KernString.c \
    src/Kernel/Util/KernRuntimeValues.c \
    \
    -Wall -Wextra -pedantic \
    -I$PARPATH/MdePkg/Include/ \
    -I$PARPATH/MdePkg/Include/X64 \
    -I$PARPATH/KernelOSPkg/src/Common \
    -L$PARPATH/MdePkg/Library/ \
    \
    -fshort-wchar \
    || exit 1 # Exit if something goes wrong

cp kernel.o hda-contents/kernel.bin 2>/dev/null

# Run the OVMF.fd inside of QEMU
qemu-system-x86_64 \
    -serial stdio \
    -bios bootloader/bios.bin \
    -drive file=fat:rw:hda-contents,format=raw,media=disk \
    -m 1024M \
    -s
