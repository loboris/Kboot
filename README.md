# Kboot

**Kboot** is a small applications which enables loading and executing applications from any K210 SPI Flash location.

It can be used to implement firmware upgrade (OTA) or to load different (multiple) applications based on some criteria.

Interactive mode is also provided which enables the user to select which application to load and execute from the list of stored applications.

More details about **Kboot** are available in the **README.md** in `src/bootloader` directory.

<br>

## How to build

Clone the repository or download and unpack the repository zip file.

**The repository contains all the tools and sources needed to build `Kboot`.**

The same build prerequisities must be satisfied as for building any other K210 application.<br>

Two example applications are provided as well as the appropriate configuration sector (**`config.bin`**):<br>
**MicroPython.bin**, built with **_FreeRTOS SDK_** is expected to be found at Flash address **`0x00080000`** (512 KB),<br>
**dvp_ov.bin**, **_Standalone SDK_** example, is expected to be found at Flash address **`0x00280000`** (2.5 MB)<br>
You can flash them (after flashing the bootloader package) using:

```console
./kflash.py -p /dev/ttyUSB0 -a 524288 -b 2000000 -t MicroPython.bin
./kflash.py -p /dev/ttyUSB0 -a 2621440 -b 2000000 -t dvp_ov.bin
```

### Build from source

* Change the working directory to the **`build`** directory.
* A simple build script **`BUILD.sh`** is provided which builds the **`Kboot`** application.
* Simply execute `BUILD.sh`
* **`bootloader.kfpkg`** package will be created which includes `bootloader.bin` binary, default configuration and the default application.
* `bootloader.kfpkg` can be flashed to the K210 using `kflash.py`. It is recommended to use `kflash.py` provided in `build` directory, but any K210 flashing program should work.
<br>

Build the `Kboot`
```console
boris@UbuntuMate:/home/k210_bootloader/build$ ./BUILD.sh

 ===========================
 === Building bootloader ===
 ===========================

=== Running 'cmake'
=== Running 'make'
Scanning dependencies of target kendryte
[ 14%] Linking C static library libkendryte.a
[ 14%] Built target kendryte
Scanning dependencies of target bootloader
[ 28%] Building C object CMakeFiles/bootloader.dir/src/bootloader/gpiohs.c.obj
[ 42%] Building C object CMakeFiles/bootloader.dir/src/bootloader/fpioa.c.obj
[ 85%] Building ASM object CMakeFiles/bootloader.dir/src/bootloader/crt.S.obj
[ 85%] Building C object CMakeFiles/bootloader.dir/src/bootloader/sha256.c.obj
[ 85%] Building C object CMakeFiles/bootloader.dir/src/bootloader/main.c.obj
[100%] Linking C executable bootloader
Generating .bin file ...
[100%] Built target bootloader

=== Finished
--------------------------------------------------
   text	   data	    bss	    dec	    hex	filename
   6856	   1264	      8	   8128	   1fc0	bootloader
--------------------------------------------------

=== Creating 'bootloader.kfpkg'

--------------------------------------------------------------------
To flash the bootloader to K210 run:
./kflash.py -p /dev/ttyUSB0 -b 2000000 -t bootloader.kfpkg

To flash MicroPython (configured to be run), run:
./kflash.py -p /dev/ttyUSB0 -a 524288 -b 2000000 -t MicroPython.bin
To flash dvp_ov (configured as inactive), run:
./kflash.py -p /dev/ttyUSB0 -a 2621440 -b 2000000 -t dvp_ov.bin
If no app is found, default app will be run which blinks the LED(s).
--------------------------------------------------------------------

boris@UbuntuMate:/home/k210_bootloader/build$ 
```

Flash to K210 board:
```console
boris@UbuntuMate:/home/k210_bootloader/build$ ./kflash.py -p /dev/ttyUSB0 -b 2000000 -t bootloader.kfpkg
[INFO] COM Port Selected Manually:  /dev/ttyUSB0 
[INFO] Default baudrate is 115200 , later it may be changed to the value you set. 
[INFO] Trying to Enter the ISP Mode... 
.
[INFO] Greeting Message Detected, Start Downloading ISP or User Firmware 
[INFO] CH340 mode 
Downloading ISP: |================================================================| 100.0% 10kiB/s
[INFO] Booting From 0x80000000 
[INFO] Wait For 0.1 second for ISP to Boot 
[INFO] Booted to Flashmode Successfully 
[INFO] Selected Baudrate:  2000000 
[INFO] Baudrate changed, greeting with ISP again ...  
[INFO] Booted to Flashmode Successfully 
[INFO] Selected Flash:  On-Board 
[INFO] Initialized flash Successfully 
[INFO] Extracting KFPKG ...  
[INFO]   Writing bootloader.bin to Flash address 0x00000000 
[INFO] Flashing data at Flash address: 0x00000000 size: 8056+37 
[INFO] Flashing with SHA suffix  84a3fbfd57fa63f77234ee1e9ebb7a52415a4eca9b5730f734b7a100c89f6df1 
Programming BIN: |=================================================================| 100.0% 
[INFO]   Writing config.bin to Flash address 0x00020000 
[INFO] Flashing data at Flash address: 0x00020000 size: 8192+37 
Programming BIN: |=================================================================| 100.0% 
[INFO]   Writing default.bin to Flash address 0x00030000 
[INFO] Flashing data at Flash address: 0x00030000 size: 60288+37 
[INFO] Flashing with SHA suffix  ff886073cb5f6081933c0d5dcb5ad53ade383672b618dbb2bd7fef43e35696b4 
Programming BIN: |=================================================================| 100.0% 
[INFO] Rebooting... 

--[ MicroPython terminal  ver. 5.1.3 ]-- 
--[ Press ESC twice for command mode ]-- 


K210 bootloader by LoBo v.1.2

* Find applications in MAIN parameters
    0: ' dvp_ov example', @ 0x00280000, size=76416, App ok, CRC32 ok, SHA256 ok, NOT active
    1: '    MicroPython', @ 0x00080000, size=1744896, App ok, CRC32 ok, SHA256 ok, ACTIVE *

Select the application number to load [ 0, 1, d=default ] ? d

* Loading app from flash at 0x00030000 (327680 B)
* Starting ...


-----------------------------------
K210 Bootloader default application
-----------------------------------

No valid application was found by Kboot!
Please, check the Kboot configuration.

```
