#!/bin/bash

APP_NAME="bootloader"

TOOLCHAIN_PATH="${PWD}/../kendryte-toolchain/bin"

# ==== Build BOOTLOADER0 wit logging enabled ====
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
rm -f bootloader* > /dev/null 2>&1
rm -f Makefile > /dev/null 2>&1

echo
echo "=== Running 'cmake'"
cmake .. -DPROJ=bootloader -DTOOLCHAIN=${TOOLCHAIN_PATH} > /dev/null 2>&1
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
${TOOLCHAIN_PATH}/riscv64-unknown-elf-size bootloader
echo "--------------------------------------------------"
${TOOLCHAIN_PATH}/riscv64-unknown-elf-objdump -S bootloader > bootloader.dump
mv -f bootloader bootloader.elf


# =========================================
# === Create kfpkg package ================
# =========================================
echo ""
echo "=== Creating 'bootloader.kfpkg'"
echo "{" > flash-list.json
echo "    \"version\": \"0.1.1\"," >> flash-list.json
echo "    \"files\": [">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 0,">> flash-list.json
echo "            \"bin\": \"bootloader.bin\",">> flash-list.json
echo "            \"sha256Prefix\": true">> flash-list.json
echo "        },">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 131072,">> flash-list.json
echo "            \"bin\": \"config.bin\",">> flash-list.json
echo "            \"sha256Prefix\": false">> flash-list.json
echo "        },">> flash-list.json
echo "        {">> flash-list.json
echo "            \"address\": 196608,">> flash-list.json
echo "            \"bin\": \"default.bin\",">> flash-list.json
echo "            \"sha256Prefix\": true">> flash-list.json
echo "        }">> flash-list.json
echo "    ]">> flash-list.json
echo "}">> flash-list.json

zip bootloader.kfpkg -9 flash-list.json bootloader.bin config.bin default.bin > /dev/null
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
echo "To flash the bootloader to K210 run:"
echo "./kflash.py -p /dev/ttyUSB0 -b 2000000 -t bootloader.kfpkg"
echo ""
echo "To flash MicroPython (configured to be run), run:"
echo "./kflash.py -p /dev/ttyUSB0 -a 524288 -b 2000000 -t MicroPython.bin"
echo "To flash dvp_ov (configured as inactive), run:"
echo "./kflash.py -p /dev/ttyUSB0 -a 2621440 -b 2000000 -t dvp_ov.bin"
echo "If no app is found, default app will be run which blinks the LED(s)."
echo "--------------------------------------------------------------------"
echo ""
