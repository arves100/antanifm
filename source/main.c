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
#include "filepicker.h"
#include "dump.h"

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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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
    ACTION_COPY_DIR,
    ACTION_MOVE_DIR,
    ACTION_DELETE_DIR,
};

enum e_diskround
{
    DISK_ROUND_EXIT,
    DISK_ROUND_ASK_DEST,
    DISK_ROUND_EXIT_NO_WAIT,
};

typedef struct select_context
{
    char source_filename[_MAX_LFN + 1];
    char dest_filename[_MAX_LFN + 1];
    int action_mode;
    bool dirpick_source;
    bool dirpick_dest;
} select_context;

static int reset_mode = EXIT_MODE_CYCLE;
static char filename_buf[1024] = {0};
static bool display_inited = false;
static int has_no_otp_bin = 0;
static select_context global_context = {0};

static void error_wait(char *message);
static void enable_display(void);
static int disk_round(const char *base, bool select_dir, select_context *ctx);
static void disk_bootstrap(const char *base, select_context *ctx);

static menu menu_main = {
    APP_NAME, // title
    {
        "Main menu", // subtitles
    },
    1, // number of subtitles
    {
        {"Copy file", &main_copy},
        {"Move file", &main_move},
        {"Delete file", &main_delete},
        //{"Copy folder", &main_copyfolder}, // TODO: enable and test...
        //{"Move folder", &main_movefolder},
        //{"Delete folder", &main_deletefolder},
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

    write32(LT_SRNPROT, 0x7BF);
    exi_init();

    printf("minute loading\n");

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
        error_wait(NULL);
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

    if(isfs_init(ISFSVOL_SLC) < 0)
    {
        error_wait("Error mounting SLC\n");
    }

    enable_display();
    printf("Showing menu...\n");

    while (reset_mode == EXIT_MODE_CYCLE)
    {
        memset(&global_context, 0, sizeof(global_context));
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
    global_context.action_mode = ACTION_COPY;
    global_context.dirpick_source = false;
    global_context.dirpick_dest = true;
    menu_init(&menu_devs);
}

void main_move(void)
{
    global_context.action_mode = ACTION_MOVE;
    global_context.dirpick_source = false;
    global_context.dirpick_dest = true;
    menu_init(&menu_devs);
}

void main_delete(void)
{
    global_context.action_mode = ACTION_DELETE;
    global_context.dirpick_source = false;
    global_context.dirpick_dest = false;
    menu_init(&menu_devs);
}

void main_copyfolder(void)
{
    global_context.action_mode = ACTION_COPY_DIR;
    global_context.dirpick_source = true;
    global_context.dirpick_dest = true;
    menu_init(&menu_devs);
}

void main_movefolder(void)
{
    global_context.action_mode = ACTION_MOVE_DIR;
    global_context.dirpick_source = true;
    global_context.dirpick_dest = true;
    menu_init(&menu_devs);
}

void main_deletefolder(void)
{
    global_context.action_mode = ACTION_DELETE_DIR;
    global_context.dirpick_source = true;
    global_context.dirpick_dest = false;
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

static int disk_round(const char *base, bool select_dir, select_context *ctx)
{
    const char *path, *path2;
    int ret = DISK_ROUND_EXIT;
    int is_ok = 0;

    gfx_clear(GFX_ALL, BLACK);

    path = pick_file((char*)base, select_dir, filename_buf);

    console_init();

    if (path)
    {
        if (ctx->source_filename[0] != '\0')
        {
            path2 = get_file_name(ctx->source_filename);

            if (path2 != NULL)
            {
                strncpy(ctx->dest_filename, path, _MAX_LFN);
                strcat(ctx->dest_filename, path2);
                is_ok = 1;
            }
            else
            {
                printf("Cannot get destination path!\n");
            }
        }
        else
        {
            strncpy(ctx->source_filename, path, _MAX_LFN);
            is_ok = 1;
        }
    }
    else
    {
        printf("Cannot open path %s\n", base);
    }

    if (is_ok)
    {
        switch (ctx->action_mode)
        {
        case ACTION_DELETE:
            printf("Are you sure you want to delete file %s?\n", ctx->source_filename);
            if (!console_abort_confirmation_power_no_eject_yes())
            {
                if (delete_file(ctx->source_filename) >= 0)
                {
                    printf("Success!\n");
                }
                else
                {
                    printf("Failed: %s!\n", strerror(errno));
                }
            }
            else
            {
                ret = DISK_ROUND_EXIT_NO_WAIT;
            }
            break;
        case ACTION_COPY:
            if (ctx->dest_filename[0] == '\0')
            {
                ret = DISK_ROUND_ASK_DEST;
            }
            else
            {
                if (strcmp(ctx->source_filename, ctx->dest_filename) == 0)
                {
                    printf("You cannot copy a file to the same directory where it exists!\n");
                }
                else
                {
                    ret = DISK_ROUND_EXIT_NO_WAIT;
                    printf("Are you sure you want to copy file %s to %s?\n", ctx->source_filename, ctx->dest_filename);
                    if (!console_abort_confirmation_power_no_eject_yes())
                    {
                        if (exist_file(ctx->dest_filename))
                        {
                            printf("The file %s already exists, do you want to replace it?\n", ctx->dest_filename);
                            if (console_abort_confirmation_power_no_eject_yes())
                            {
                                is_ok = 0;
                            }
                        }
    
                        if (is_ok)
                        {
                            ret = DISK_ROUND_EXIT;
                            if (copy_file(ctx->source_filename, ctx->dest_filename) >= 0)
                            {
                                printf("Success!\n");
                            }
                            else
                            {
                                printf("Failed: %s!\n", strerror(errno));
                            }    
                        }
                    }    
                }
            }
            break;
        case ACTION_MOVE:
            if (ctx->dest_filename[0] == '\0')
            {
                ret = DISK_ROUND_ASK_DEST;
            }
            else
            {
                if (strcmp(ctx->source_filename, ctx->dest_filename) == 0)
                {
                    printf("You cannot move a file to the same directory where it exists!\n");
                }
                else
                {
                    ret = DISK_ROUND_EXIT_NO_WAIT;
                    printf("Are you sure you want to move the file %s to %s?\n", ctx->source_filename, ctx->dest_filename);
                    if (!console_abort_confirmation_power_no_eject_yes())
                    {
                        if (exist_file(ctx->dest_filename))
                        {
                            printf("The file %s already exists, do you want to replace it?\n", ctx->dest_filename);
                            if (console_abort_confirmation_power_no_eject_yes())
                            {
                                is_ok = 0;
                            }
                        }

                        if (is_ok)
                        {
                            ret = DISK_ROUND_EXIT;
                            if (copy_file(ctx->source_filename, ctx->dest_filename) >= 0)
                            {
                                if (delete_file(ctx->source_filename) >= 0)
                                {
                                    printf("Success!\n");
                                }
                                else
                                {
                                    printf("Deletion fail: %s!\n", strerror(errno));
                                }
                            }
                            else
                            {
                                printf("Copy fail: %s!\n", strerror(errno));
                            }
                        }
                    }
                }
            }
            break;
        default:
            break;
        }    
    }

    return ret;
}

static void disk_bootstrap(const char *base, select_context *ctx)
{
    int res;
    
    gfx_clear(GFX_ALL, BLACK);
    res = disk_round(base, (ctx->source_filename[0] != '\0') ? ctx->dirpick_dest : ctx->dirpick_source, ctx);

    if (res == DISK_ROUND_ASK_DEST)
    {
        gfx_clear(GFX_ALL, BLACK);
        menu_init(&menu_devs);
    }
    else if (res == DISK_ROUND_EXIT_NO_WAIT)
    {
        disk_back();
    }
    else
    {
        console_power_to_continue();
        disk_back();
    }
}

void disk_slc(void)
{
    disk_bootstrap("slc:/", &global_context);
}

void disk_sdmc(void)
{
    disk_bootstrap("sdmc:/", &global_context);
}

void disk_back(void)
{
    gfx_clear(GFX_ALL, BLACK);
    reset_mode = EXIT_MODE_CYCLE;
    menu_reset();
}
