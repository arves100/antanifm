/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *  Copyright (C) 2016, 2023    Max Thomas <mtinc2@gmail.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2009          Andre Heider "dhewg" <dhewg@wiibrew.org>
 *  Copyright (C) 2009          John Kelley <wiidev@kelley.ca>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "main.h"
#include "menu.h"
#include "files.h"
#include "filepicker.h"

#include <gfx.h>
#include <exi.h>
#include <serial.h>
#include <utils.h>
#include <exception.h>
#include <memory.h>
#include <irq.h>
#include <latte.h>
#include <smc.h>
#include <mlc.h>
#include <gpu.h>
#include <minini.h>
#include <elm.h>
#include <crypto.h>
#include <sdcard.h>
#include <isfs.h>
// c runtime
#include <string.h>
#include <stdlib.h>

#define APP_NAME "Antani file manager"

enum e_exit_mode
{
    EXIT_MODE_RESET,
    EXIT_MODE_SHUTDOWN,
    EXIT_MODE_CYCLE,
};

enum e_action_mode
{
    ACTION_COPY,
    ACTION_MOVE,
    ACTION_DELETE,
};

enum e_diskround
{
    DISK_ROUND_COMPLETE,
    DISK_ROUND_FAILED,
    DISK_ROUND_ASK_DEST,
};

static int action_mode = ACTION_COPY;
static int reset_mode = EXIT_MODE_CYCLE;
static char filename_buf[1024] = {0};
static bool display_inited = false;
static int has_no_otp_bin = 0;

static void error_wait(char *message);
static void enable_display(void);
static const char* try_pick(const char *base);
static int disk_round(const char *base);
static void disk_bootstrap(const char *base);

static menu menu_main = {
    APP_NAME, // title
    {
        "Main menu", // subtitles
    },
    1, // number of subtitles
    {
        {"Copy file", &main_copy},
        {"Move file", &main_move},
        {"Delete file", &main_delete}, // options
        {"Hardware reset", &main_reset},
        {"Power off", &main_shutdown},
        {"Credits", &main_credits},
    },
    6, // number of options
    0,
    0
};

static menu menu_devs = {
    APP_NAME,
    {
        "Select a device",
    },
    1,
    {
        {"SLC", &disk_slc},
        {"SDMC", &disk_sdmc},
        {"Abort and return to main menu", &disk_back}
    },
    3,
    0,
    0
};

static void enable_display(void)
{
    gfx_printf_to_display(true);
    if(display_inited)
        return;
    display_inited = true;
    gpu_display_init();
    gfx_init();
}

static void error_wait(char *message)
{
    enable_display();
    if(message)
        printf(message);
    console_power_to_continue();
}

u32 _main(void *base)
{
    int res = 0;

    (void)base;
    gfx_init();
    exi_init();
    printf("minute loading\n");

    memset(&fd_source, 0, sizeof(fd_source));
    memset(&fd_dest, 0, sizeof(fd_dest));

    serial_force_terminate();
    udelay(500);

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    irq_initialize();
    printf("Interrupts initialized\n");

    // Read OTP and SEEPROM
    srand(read32(LT_TIMER));
    crypto_initialize();
    printf("crypto support initialized\n");
    latte_print_hardware_info();

    printf("Initializing SD card...\n");
    sdcard_init();
    printf("sdcard_init finished\n");

    printf("Mounting SD card...\n");
    res = ELM_Mount();
    if(res) {
        printf("Error while mounting SD card (%d).\n", res);
    }

    crypto_check_de_Fused();

    if (crypto_otp_is_de_Fused)
    {
        //console_power_to_continue();

        printf("Console is de_Fused! Loading sdmc:/otp.bin...\n");
        FILE* otp_file = fopen("sdmc:/otp.bin", "rb");
        if (otp_file)
        {
            fread(&otp, sizeof(otp), 1, otp_file);
            fclose(otp_file);
        }
        else {
            error_wait("Failed to load `sdmc:/otp.bin`!\nFirmware will fail to load.\n");
            has_no_otp_bin = 1;
        }
    }

    // Hopefully we have proper keys by this point
    crypto_decrypt_seeprom();

    // init ini file
    minini_init();

    smc_get_events();
    //leave ODD Power on for HDDs
    if (has_no_otp_bin || 
            (seeprom.bc.sata_device != SATA_TYPE_GEN2HDD && 
             seeprom.bc.sata_device != SATA_TYPE_GEN1HDD))
        smc_set_odd_power(false);

    enable_display();
    printf("Showing menu...\n");

    while (reset_mode == EXIT_MODE_CYCLE)
    {
        file_clear();
        menu_init(&menu_main);
    }

    smc_get_events();
    smc_set_odd_power(true);

    gpu_cleanup();

    printf("Unmounting SLC...\n");
    isfs_fini();

    printf("Shutting down MLC...\n");
    mlc_exit();
    
    printf("Shutting down SD card...\n");
    ELM_Unmount();
    sdcard_exit();

    printf("Shutting down interrupts...\n");
    irq_shutdown();

    printf("Shutting down caches and MMU...\n");
    mem_shutdown();

    switch (reset_mode)
    {
        case EXIT_MODE_RESET:
            smc_reset();
            break;
        case EXIT_MODE_SHUTDOWN:
            smc_power_off();
            break;
        default:
            // what?
            break;
    }

    return 0;
}

void main_copy(void)
{
    action_mode = ACTION_COPY;
    menu_init(&menu_devs);
}

void main_move(void)
{
    action_mode = ACTION_MOVE;
    menu_init(&menu_devs);
}

void main_delete(void)
{
    action_mode = ACTION_DELETE;
    menu_init(&menu_devs);
}

void main_reset(void)
{
    gfx_clear(GFX_ALL, BLACK);

    reset_mode = EXIT_MODE_RESET;
    menu_reset();
}

void main_shutdown(void)
{
    gfx_clear(GFX_ALL, BLACK);

    reset_mode = EXIT_MODE_SHUTDOWN;
    menu_reset();
}

void main_credits(void)
{
    gfx_clear(GFX_ALL, BLACK);
    console_init();

    console_add_text("this code was made possible with minute:\n");
    console_add_text("The SALT team: Dazzozo, WulfyStylez, shinyquagsire23 and Relys (in spirit)\n");
    console_add_text("Other Contributers: Jan\n");
    console_add_text("Special thanks to fail0verflow (formerly Team Twiizers) for the original \"mini\", and for the vast\nmajority of Wii research and early Wii U research!\n");
    console_add_text("Thanks to all WiiUBrew contributors, including: Hykem, Marionumber1, smea, yellows8, derrek,\nplutoo, naehrwert...\n");

    console_show();
    console_power_to_continue();
}

static const char* try_pick(const char *base)
{
    const char* new_file = pick_file((char*)base, false, filename_buf);
    if (new_file)
    {
        if (memcmp(base, new_file, strlen(base)) == 0)
        {
            return new_file;
        }
    }

    return NULL;
}

static int disk_round(const char *base)
{
    const char* path;
    int set_ok = 0;
    int ret = DISK_ROUND_FAILED;

    gfx_clear(GFX_ALL, BLACK);

    path = try_pick(base);

    console_init();

    if (path)
    {
        if (file_is_src())
        {
            if (file_set_dst(path))
            {
                set_ok = 1;
            }
            else
            {
                error_wait("Cannot open destination file!\n");
            }
        }
        else
        {
            if (file_set_src(path))
            {
                set_ok = 1;
            }
            else
            {
                error_wait("Cannot open source file!\n");
            }
        }

        if (set_ok)
        {
            switch (action_mode)
            {
            case ACTION_DELETE:
                printf("Are you sure you want to delete file %s?\n", file_get_src());
                if (!console_abort_confirmation_power_no_eject_yes())
                {
                    printf("%s\n", file_action_delete() ? "Success!" : "Failed!");
                    ret = DISK_ROUND_COMPLETE;
                }
                break;
            case ACTION_COPY:
                if (!file_is_dst())
                {
                    ret = DISK_ROUND_ASK_DEST;
                }
                else
                {
                    printf("Are you sure you want to copy file %s to %s?\n", file_get_src(), file_get_dst());
                    if (!console_abort_confirmation_power_no_eject_yes())
                    {
                        printf("%s\n", file_action_copy() ? "Success!" : "Failed!");
                        ret = DISK_ROUND_COMPLETE;
                    }
                }
                break;
            case ACTION_MOVE:
                if (!file_is_dst())
                {
                    ret = DISK_ROUND_ASK_DEST;
                }
                else
                {
                    printf("Are you sure you want to move the file %s to %s?\n", file_get_src(), file_get_dst());
                    if (!console_abort_confirmation_power_no_eject_yes())
                    {
                        printf("%s\n", file_action_move() ? "Success!" : "Failed!"); 
                        ret = DISK_ROUND_COMPLETE;
                    }
                }
                break;
            }    
        }
    }
    else
    {
        printf("Cannot open path %s\n", base);
    }

    return ret;
}

static void disk_bootstrap(const char *base)
{
    int res;
    
    gfx_clear(GFX_ALL, BLACK);
    res = disk_round(base);

    if (res == DISK_ROUND_FAILED)
    {
        disk_back();
    }
    else if (res == DISK_ROUND_ASK_DEST)
    {
        gfx_clear(GFX_ALL, BLACK);
        menu_init(&menu_devs);
    }
    else
    {
        console_power_to_continue();
        disk_back();
    }
}

void disk_slc(void)
{
    disk_bootstrap("slc:/");
}

void disk_sdmc(void)
{
    disk_bootstrap("sdmc:/");
}

void disk_back(void)
{
    gfx_clear(GFX_ALL, BLACK);
    reset_mode = EXIT_MODE_CYCLE;
    menu_reset();
}
