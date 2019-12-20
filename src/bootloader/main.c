/* Copyright 2019 LoBo
 * 
 * K210 Bootloader
 * ver. 1.2, 11/2019
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * ---------------------------------------------------------------------------
 * SPI Flash layout used with bootloader:
 * ---------------------------------------------------------------------------
 * From         To          Length  Comment
 * ---------------------------------------------------------------------------
 * 0x00000000   0x0000FFFF     64K  bootloader code
 * 0x00010000   0x0001FFFF     64K  reserved, bootloader code for high sram
 * 0x00020000   0x00020FFF      4K  boot configuration sector
 * 0x00021000   0x00021FFF      4K  boot configuration sector BACKUP
 * 0x00022000   0x0002FFFF     56K  reserved, user data, ...
 * 0x00030000   0x0007FFFF    320K  default application, optional, recommended
 * 0x00080000   flash_end           user area, app code, file systems, data...
 * ---------------------------------------------------------------------------
 */


#include "spi.h"
#include "sysctl.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "sha256.h"

// Constants
/*
 * Pin used to enter interactive mode and request the user
 * to select which application to load
 * Any valid and not used K210 gpio can be used (0 ~ 47)
 * If set to -1 this feature will not be used
 */
#define BOOT_PIN            18
#define GPIO_KEY            2

#define DEFAULT_APP_ADDR        0x00030000  // default application in flash at 192K
#define APP_START_ADDR          0x80002000  // application start address in sram (at 8K)
#define APP_SIZE                0x00050000  // default application size 320K
#define BOOT_CONFIG_ADDR        0x00020000  // boot config sector in flash at 128K
#define BOOT_CONFIG_ITEMS       8           // number of handled config entries
#define MAGIC_ID                0x5AA5D0C0  // boot config magic number; bit0 - active flag, bit1 - crc flag, bit2 - SHA flag
#define MAGIC_ID_MASK           0xFFFFFFF0
#define CFG_APP_FLAG_ACTIVE     0x00000001
#define CFG_APP_FLAG_CRC32      0x00000002
#define CFG_APP_FLAG_SHA256     0x00000004

#define MIN_APP_FLASH_ADDR      0x00080000  // minimal application address in Flash: 512K
#define MAX_APP_FLASH_ADDR      0x00800000  // mmaximal application address in Flash: 8M
#define MIN_APP_FLASH_SIZE      0x00004000  // minimal application size in Flash: 16K
#define MAX_APP_FLASH_SIZE      0x00300000  // mmaximal application size in Flash: 3M

// ROM functions
typedef int rom_printf_func(const char * restrict format, ... );
typedef int rom_getchar_func();

rom_printf_func *rom_printf = (rom_printf_func*)0x88001418; /* fixed address in ROM */
rom_printf_func *rom_printk = (rom_printf_func*)0x880016b0; /* fixed address in ROM */
rom_getchar_func *rom_getchar = (rom_getchar_func*)0x88008bd0; /* fixed address in ROM */

// static variables
volatile sysctl_t *const sysctl = (volatile sysctl_t *)SYSCTL_BASE_ADDR;

static volatile spi_t *spi_handle = (volatile spi_t *)SPI3_BASE_ADDR;

static uint32_t cfg_magic = 0;
static uint32_t cfg_address = 0;
static uint32_t cfg_size = 0;
static uint32_t cfg_crc = 0;
static uint8_t  cfg_info[16] = "App name";
static uint8_t  available_apps[BOOT_CONFIG_ITEMS] = {0};

static uint64_t app_start = APP_START_ADDR;
static uint32_t app_flash_start = DEFAULT_APP_ADDR;
static uint32_t app_size = APP_SIZE;
static uint8_t *app_flash_ptr = (uint8_t *)SPI3_BASE_ADDR;
static uint8_t *app_sram_ptr = (uint8_t *)APP_START_ADDR;

static uint32_t *cfg_flash_ptr = (uint32_t *)(SPI3_BASE_ADDR+BOOT_CONFIG_ADDR);
static uint8_t *cfg_flash_bptr = (uint8_t *)(SPI3_BASE_ADDR+BOOT_CONFIG_ADDR);

static uint32_t i = 0;
static uint32_t cfg_offset = 0;
static uint32_t offset = 0;
static uint8_t key = 0;
static uint32_t crc32;

static uint8_t app_hash[SHA256_HASH_LEN] = {0};
static uint8_t hash[SHA256_HASH_LEN] = {0};
static uint8_t buffer[1024] = {0};

static uint32_t boot_pin = 1;
static int char_in = 0;
static uint32_t print_enabled = 0;

// -------------------------------------------------
// Printing messages and interactive mode is enabled
// -------------------------------------------------

#define LOG(format, ...)                        \
    do                                          \
    {                                           \
        if (print_enabled) {                    \
            rom_printk(format, ##__VA_ARGS__);  \
            usleep(200);                        \
        }                                       \
    } while(0)


//-----------------------------------------
static uint32_t flash2uint32(uint32_t addr)
{
    uint32_t val = app_flash_ptr[addr];
    val += app_flash_ptr[addr+1] << 8;
    val += app_flash_ptr[addr+2] << 16;
    val += app_flash_ptr[addr+3] << 24;
    return val;
}

//--------------------------------
static void get_params(uint32_t i)
{
    offset = (i*8) + (cfg_offset / 4); // 32bit offset
    cfg_magic = cfg_flash_ptr[offset + 0];
    cfg_address = cfg_flash_ptr[offset + 1];
    cfg_size = cfg_flash_ptr[offset + 2];
    cfg_crc = cfg_flash_ptr[offset + 3];

    // Get app description
    offset = (i*32) + cfg_offset + 0x10; // 8bit offset
    key = cfg_flash_bptr[offset]; // dummy read needed to switch to 8bit XiP read
    for (int n=0; n<16; n++) {
        cfg_info[n] = cfg_flash_bptr[offset + n];
    }
    cfg_info[15] = 0;

    offset = cfg_address + cfg_size + 5;
    for (int n=0; n<SHA256_HASH_LEN; n++) {
        app_hash[n] = app_flash_ptr[offset + n];
    }
}

//-------------------------
static uint32_t app_check()
{
    uint8_t  key = app_flash_ptr[cfg_address];          // must be 0, SHA256 key NOT specified
    uint32_t sz = flash2uint32(cfg_address+1);          // app size
    uint32_t app_id = flash2uint32(cfg_address+7);      // for app built with Standalone SDK

    if (cfg_size != sz) return 0;
    if (app_id != 0xdeadbeef) {
        app_id = flash2uint32(cfg_address+9);   // for app built with FreeRTOS SDK
    }
    if ((key != 0) || (app_id != 0xdeadbeef)) return 0;
    return 1;
}

//-------------------------
static uint32_t app_crc32()
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t byte, mask;
    // Read from flash and update CRC32 value
    for (uint32_t n = 5; n < (cfg_size+5); n++) {
        byte = app_flash_ptr[cfg_address + n];
        crc = crc ^ byte;
        for (uint32_t j = 0; j < 8; j++) {
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    crc32 = ~crc;
    if (crc32 != cfg_crc) return 0;
    return 1;
}

//--------------------------
static uint32_t app_sha256()
{
    int size = cfg_size+5;
    int sz;
    sha256_context_t context;
    sha256_init(&context, size);
    uint32_t idx = 0;

    while (size > 0) {
        sz = (size >= 1024) ? 1024 : size;
        for (int n=0; n<sz; n++) {
            buffer[n] = app_flash_ptr[cfg_address + idx + n];
        }
        sha256_update(&context, buffer, sz);
        idx += sz;
        size -= sz;
    }

    sha256_final(&context, hash);

    for (idx=0; idx<SHA256_HASH_LEN; idx++) {
        if (hash[idx] != app_hash[idx]) {
            /*
            LOG("\n    SHA calculated: ");
            for (idx=0; idx<SHA256_HASH_LEN; idx++) {
                LOG("%02x", hash[idx]);
            }
            LOG("\n      SHA in Flash: ");
            for (idx=0; idx<SHA256_HASH_LEN; idx++) {
                LOG("%02x", app_hash[idx]);
            }
            LOG("\n    ");
            */
            return 0;
        }
    }
    return 1;
}

//============
int main(void)
{
    // Initialize bss data to 0
    /*extern unsigned int _bss;
    extern unsigned int _ebss;
    unsigned int *dst;
    dst = &_bss;
    while(dst < &_ebss)
        *dst++ = 0;*/

    // Without this command every character is printed twice !?
    rom_printf("");

    // ----------------------------------------------------------------------------------------------
    // === Initialize SPI Flash driver ===
    // spi clock init (SPI3 clock source is PLL0/2)
    sysctl->clk_sel0.spi3_clk_sel = 1;
    sysctl->clk_en_peri.spi3_clk_en = 1;
    sysctl->clk_th1.spi3_clk_threshold = 0;

    // spi3 init
    // sets spi clock to 43.333333 MHz (SPI3 clock is 390 MHz)
    spi_handle->baudr = 9;
    spi_handle->imr = 0x00;
    spi_handle->dmacr = 0x00;
    spi_handle->dmatdlr = 0x10;
    spi_handle->dmardlr = 0x00;
    spi_handle->ser = 0x00;
    spi_handle->ssienr = 0x00;
    spi_handle->ctrlr0 = (SPI_WORK_MODE_0 << 8) | (SPI_FF_QUAD << 22) | (7 << 0);
    spi_handle->spi_ctrlr0 = 0;
    spi_handle->endian = 0;

    // Enable XIP mode
    spi_handle->xip_ctrl = (0x01 << 29) | (0x02 << 26) | (0x01 << 23) | (0x01 << 22) | (0x04 << 13) |
                (0x01 << 12) | (0x02 << 9) | (0x06 << 4) | (0x01 << 2) | 0x02;
    spi_handle->xip_incr_inst = 0xEB;
    spi_handle->xip_mode_bits = 0x00;
    spi_handle->xip_ser = 0x01;
    spi_handle->ssienr = 0x01;
    sysctl->peri.spi3_xip_en = 1;
    usleep(200);
    // ----------------------------------------------------------------------------------------------

    cfg_magic = cfg_flash_ptr[BOOT_CONFIG_ITEMS];
    print_enabled = (cfg_magic == MAGIC_ID) ? 0 : 1;
    if (print_enabled) {
        // Initialize and read boot pin status
        fpioa_set_function(BOOT_PIN, FUNC_GPIOHS2);
        gpiohs_set_drive_mode(GPIO_KEY, GPIO_DM_INPUT_PULL_UP);
        usleep(1000);
        boot_pin = gpiohs_get_pin(GPIO_KEY);
    }
    else {
        boot_pin = 1;
    }

    LOG("\nK210 bootloader by LoBo v.1.2\n\n");

    // === Read boot configuration from flash address 0x20000 ===
    LOG("* Find applications in MAIN parameters\n");

    // Check boot entries
    app_size = APP_SIZE;
    app_flash_start = DEFAULT_APP_ADDR;
    cfg_offset = 0;

check_cfg:
    for (i = 0; i < BOOT_CONFIG_ITEMS; i++) {
        get_params(i);

        if ((cfg_magic & MAGIC_ID_MASK) == MAGIC_ID) {
            // *** Valid configuration found
            LOG("    %u: '%15s', ", i, cfg_info);
            // Check if the Flash address is in range (512K ~ 8MB)
            if ((cfg_address >= MIN_APP_FLASH_ADDR) && (cfg_address <= MAX_APP_FLASH_ADDR)) {
                // Address valid, check if the size is in range (16K ~ 3MB)
                if ((cfg_size >= MIN_APP_FLASH_SIZE) && (cfg_size <= MAX_APP_FLASH_SIZE)) {
                    // Valid size
                    LOG("@ 0x%08X, size=%u, ", cfg_address, cfg_size);
                    // If active application or in interactive mode, check the application
                    if ((cfg_magic & CFG_APP_FLAG_ACTIVE) || (boot_pin == 0)) {
                        // ** Check if valid application
                        if (app_check() == 0) {
                            LOG("App CHECK failed\n");
                            continue;
                        }
                        LOG("App ok, ");
                        // ** Check if crc check is requested (bit #1 set)
                        if (cfg_magic & CFG_APP_FLAG_CRC32) {
                            // CRC check was requested, check flash data
                            if (app_crc32() == 0) {
                                //LOG("CRC32 check failed (%08X <> %08X)\n", crc32, cfg_crc);
                                LOG("CRC32 check failed\n");
                                continue;
                            }
                            LOG("CRC32 ok, ");
                        }
                        // ** Check if SHA256 check is requested (bit #2 set)
                        if (cfg_magic & CFG_APP_FLAG_SHA256) {
                            // SHA256 check was requested, check flash data
                            if (app_sha256() == 0) {
                                LOG("SHA256 check failed\n");
                                continue;
                            }
                            LOG("SHA256 ok, ");
                        }
                        available_apps[i] = 1;
                    }
                    else {
                        LOG("not checked, ");
                    }
                    // ** Check if this is an active config (bit #0 set)
                    if (cfg_magic & CFG_APP_FLAG_ACTIVE) {
                        LOG("ACTIVE");
                        if (app_flash_start == DEFAULT_APP_ADDR) {
                            app_size = cfg_size;
                            app_flash_start = cfg_address;
                            LOG(" *");
                        }
                        LOG("\n");
                        if (boot_pin > 0) {
                            // Active application found and cheched and not in interractive mode
                            break;
                        }
                    }
                    else {
                        LOG("NOT active\n");
                    }
                }
                else {
                    LOG("wrong size\n");
                }
            }
            else {
                LOG("wrong address\n");
            }
        }
    }

    if (app_flash_start == DEFAULT_APP_ADDR) {
        if (cfg_offset == 0) {
            // no valid entry found in main config sector
            LOG("\n* Find applications in BACKUP parameters\n");
            cfg_offset = 4096;
            if (boot_pin == 0) {
                cfg_magic = cfg_flash_ptr[4096+BOOT_CONFIG_ITEMS];
                boot_pin = (cfg_magic == MAGIC_ID) ? 1 : 0;
            }
            goto check_cfg;
        }
        else {
            // No valid application found
            LOG("\n* No app found, loading default\n");
            app_size = flash2uint32(DEFAULT_APP_ADDR+1);
        }
    }
    else if (boot_pin == 0) {
        // ** Interractive mode
        // Check boot pin again
        boot_pin = gpiohs_get_pin(GPIO_KEY);
        if (boot_pin == 0) {
            // ** request user to select the application to run
            LOG("\nSelect the application number to load [");
            for (i = 0; i < BOOT_CONFIG_ITEMS; i++) {
                if (available_apps[i]) {
                    LOG(" %u,", i);
                }
            }
            LOG(" d=default ] ? ");
            while (1) {
                char_in = rom_getchar();
                if ((char_in == 'd') || (char_in == 'D')) {
                    app_size = APP_SIZE;
                    app_flash_start = DEFAULT_APP_ADDR;
                    break;
                }
                else {
                    char_in -= 0x30;
                    if ((char_in >= 0) && (char_in < BOOT_CONFIG_ITEMS)) {
                        if (available_apps[char_in]) {
                            get_params(char_in);
                            app_size = cfg_size;
                            app_flash_start = cfg_address;
                            char_in += 0x30;
                            break;
                        }
                    }
                    char_in += 0x30;
                    LOG("%c? ", char_in);
                }
            }
            LOG("%c\n\n", (char)char_in);
        }
    }

    // === Copy application code from flash to SRAM ===
    LOG("* Loading app from flash at 0x%08X (%u B)\n", app_flash_start, app_size);

    app_size += 37;
    for (i = 5; i < app_size; i++) {
        app_sram_ptr[i-5] = app_flash_ptr[app_flash_start+i];
    }

    // === Start the application ===
    // Disable XIP mode
    sysctl->peri.spi3_xip_en = 0;

    LOG("* Starting ...\n\n", app_start);
    usleep(1000);

    // === Jump to the application start address ===
    asm ("jr %0" : : "r"(app_start));

    // This should newer be reached!
    while (1)
        ;
    return 0;
}
