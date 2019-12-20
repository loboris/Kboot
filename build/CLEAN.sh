#!/bin/bash

make clean > /dev/null 2>&1

rm -rf __pycache__/* > /dev/null 2>&1
rm -rf CMakeFiles/* > /dev/null 2>&1
rm -rf lib/* > /dev/null 2>&1
rmdir __pycache__ > /dev/null 2>&1
rmdir CMakeFiles > /dev/null 2>&1
rmdir lib > /dev/null 2>&1

rm -f *.cmake > /dev/null 2>&1
rm -f *.txt > /dev/null 2>&1
rm -f *.kfpkg > /dev/null 2>&1
rm -f *.json > /dev/null 2>&1
rm -f bootloader* > /dev/null 2>&1
rm -f Makefile > /dev/null 2>&1

