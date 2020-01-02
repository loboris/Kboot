#!/bin/bash

TOOLCHAIN_PATH="${PWD}/../kendryte-toolchain/bin"

# ==== Build BOOTLOADER0 (runs from low SRAM address) ====

echo
echo " ==========================="
echo " === Building bootloader ==="
echo " ==========================="

# === Clean the build ===

make clean > /dev/null 2>&1

rm -rf CMakeFiles/* > /dev/null 2>&1
rm -rf lib/* > /dev/null 2>&1
rm -f *.cmake > /dev/null 2>&1
rm -f *.txt > /dev/null 2>&1
rm -f *.kfpkg > /dev/null 2>&1
rm -f *.json > /dev/null 2>&1
rm -f bootloader*.* > /dev/null 2>&1
rm -f Makefile > /dev/null 2>&1


cp -f ../lds/bootloader_lo.ld ../lds/kendryte.ld
echo
echo "=== Running 'cmake'"
cmake .. -DPROJ=bootloader_lo -DTOOLCHAIN=${TOOLCHAIN_PATH} > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "cmake ERROR"
    exit 1
fi

echo "=== Running 'make'"
make -j8
if [ $? -ne 0 ]; then
    echo "make ERROR"
    exit 2
fi

echo ""
echo "=== Finished"
echo "---------------------------------------------------"
${TOOLCHAIN_PATH}/riscv64-unknown-elf-size bootloader_lo
echo "---------------------------------------------------"
${TOOLCHAIN_PATH}/riscv64-unknown-elf-objdump -S bootloader_lo > bootloader_lo.dump
mv -f bootloader_lo bootloader_lo.elf

# Pad to 1K size
#FILESIZE=$(stat --printf '%s' bootloader_lo.bin)
#MINSIZE=1024
#if [ ${FILESIZE} -lt ${MINSIZE} ]; then
#     truncate -s $MINSIZE bootloader_lo.bin
#fi


# ===========================================================
# ==== Build BOOTLOADER_hi (runs from high SRAM address) ====
# ===========================================================

make clean > /dev/null 2>&1

rm -rf CMakeFiles/* > /dev/null 2>&1
rm -rf lib/* > /dev/null 2>&1
rm -f *.cmake > /dev/null 2>&1
rm -f *.txt > /dev/null 2>&1
rm -f Makefile > /dev/null 2>&1


cp -f ../lds/bootloader_hi.ld ../lds/kendryte.ld
echo
echo "=== Running 'cmake'"
cmake .. -DPROJ=bootloader_hi -DTOOLCHAIN=${TOOLCHAIN_PATH} > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "cmake ERROR"
    exit 1
fi

echo "=== Running 'make'"
make -j8
if [ $? -ne 0 ]; then
    echo "make ERROR"
    exit 2
fi

echo ""
echo "=== Finished"
echo "--------------------------------------------------"
${TOOLCHAIN_PATH}/riscv64-unknown-elf-size bootloader_hi
echo "--------------------------------------------------"
${TOOLCHAIN_PATH}/riscv64-unknown-elf-objdump -S bootloader_hi > bootloader_hi.dump
mv -f bootloader_hi bootloader_hi.elf


# Pad to 51K size
#FILESIZE=$(stat --printf '%s' bootloader_hi.bin)
#MINSIZE=52224
#if [ ${FILESIZE} -lt ${MINSIZE} ]; then
#     truncate -s $MINSIZE bootloader_hi.bin
#fi

# merge bootloaders and default config sectors into one file
#cat bootloader_lo.bin bootloader_hi.bin config.bin > bootloader.bin


# =========================================
# === Create kfpkg package ================
# =========================================
echo ""
echo "=== Creating 'kboot.kfpkg'"
echo "{" > flash-list.json
echo "    \"version\": \"0.1.1\"," >> flash-list.json
echo "    \"files\": [">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 0,">> flash-list.json
echo "            \"bin\": \"bootloader_lo.bin\",">> flash-list.json
echo "            \"sha256Prefix\": true">> flash-list.json
echo "        },">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 4096,">> flash-list.json
echo "            \"bin\": \"bootloader_hi.bin\",">> flash-list.json
echo "            \"sha256Prefix\": true">> flash-list.json
echo "        },">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 16384,">> flash-list.json
echo "            \"bin\": \"config.bin\",">> flash-list.json
echo "            \"sha256Prefix\": false">> flash-list.json
echo "        },">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 20480,">> flash-list.json
echo "            \"bin\": \"config.bin\",">> flash-list.json
echo "            \"sha256Prefix\": false">> flash-list.json
echo "        }">> flash-list.json
echo "    ]">> flash-list.json
echo "}">> flash-list.json

zip kboot.kfpkg -9 flash-list.json bootloader_lo.bin bootloader_hi.bin config.bin > /dev/null
rm -f flash-list.json

if [ $? -ne 0 ]; then
echo "ERROR creating kfpkg"
exit 1
fi


# === CLEAN ===
make clean > /dev/null 2>&1

rm -rf __pycache__/* > /dev/null 2>&1
rm -rf CMakeFiles/* > /dev/null 2>&1
rm -rf lib/* > /dev/null 2>&1
rmdir __pycache__ > /dev/null 2>&1
rmdir CMakeFiles > /dev/null 2>&1
rmdir lib > /dev/null 2>&1

rm -f *.cmake > /dev/null 2>&1
rm -f *.txt > /dev/null 2>&1
rm -f *.json > /dev/null 2>&1
rm -f *.bak > /dev/null 2>&1
rm -f Makefile > /dev/null 2>&1


echo ""
echo "--------------------------------------------------------------------"
echo "To flash the kboot package to K210 run:"
echo "./ktool.py -p /dev/ttyUSB0 -b 2000000 -t kboot.kfpkg"
echo ""
echo "To flash default application run:"
echo "./ktool.py -p /dev/ttyUSB0 -a 65536 -b 2000000 -t default.bin"
echo ""
echo "Default config can run applications from 512K or 2.5M flash address"
echo "Some applications are provided for testing:"
echo ""
echo "To flash MicroPython (FreeRTOS SDK) at 512K, run:"
echo "./ktool.py -p /dev/ttyUSB0 -a 524288 -b 2000000 -t MicroPython.bin"
echo "To flash dvp_ov (Standalone SDK) at 2.5M, run:"
echo "./ktool.py -p /dev/ttyUSB0 -a 2621440 -b 2000000 -t dvp_ov.bin"
echo "To flash maixpy (Standalone SDK) at 2.5M, run:"
echo "./ktool.py -p /dev/ttyUSB0 -a 2621440 -b 2000000 -t maixpy.bin"
echo ""
echo "If no app is found, default app will be run which blinks the LED(s)."
echo "--------------------------------------------------------------------"
echo ""
