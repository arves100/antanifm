/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef MINUTE_BOOT1

#include "filepicker.h"

#include "smc.h"
#include "gfx.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

char *_filename;
char _directory[_MAX_LFN + 1];

picker* __picker;
picker *picker_chain[100];

int opened_pickers = 0;

void picker_init(picker* new_picker);
void picker_print_filenames();
void picker_update();
void picker_next_selection();
void picker_prev_selection();
void picker_next_jump();
void picker_prev_jump();

int pick_sprintf(char *out, const char* fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vsprintf(out, fmt, va);
    va_end(va);

    return 0;
}

int pick_snprintf(char *out, unsigned int len, const char* fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vsnprintf(out, len, fmt, va);
    va_end(va);

    return 0;
}

char *pick_strcpy(char *dest, char *src)
{
    int count = 0;
    while(1)
    {
        dest[count] = src[count];
        if(dest[count] == '\0') break;

        count++;
    }
    return dest;
}

char* pick_file(char* path, bool folderpick, char* filename_buf)
{
    _filename = filename_buf;

    picker _picker = {{0}};
    pick_strcpy(_picker.path, path);
    _picker.folderpick = folderpick;

    DIR* dir;
    struct dirent* info;

    dir = opendir(path);

    if (dir == NULL) {
        return NULL;
    }

    pick_strcpy(_picker.directory[0], "..");
    _picker.directories = 1;

    if (folderpick)
    {
        pick_strcpy(_picker.directory[1], ".");
        _picker.directories++;
    }

    /*
     * TODO: More than 256 files/directories, if needed.
     * Currently any more than that sucks up quite a bit
     * of RAM and isn't strictly needed.
     *
     * At the moment though it at least caps it so it
     * doesn't crash.
     */
    while(true)
    {
        info = readdir(dir);

        if (info == NULL || info->d_name[0] == 0) break;
        if (info->d_name[0] == '.' && info->d_name[1] == '\0') continue;
        if (info->d_name[0] == '.' && info->d_name[1] == '.' && info->d_name[2] == '\0') continue;

        if ((info->d_type == DT_DIR) && _picker.directories < 255) // directory
        {
            pick_strcpy(_picker.directory[_picker.directories], info->d_name);
            _picker.directories++;

        }
        else if(_picker.files < 255 && !folderpick) // file
        {
            pick_strcpy(_picker.file[_picker.files], info->d_name);
            _picker.files++;
        }
    }

    closedir(dir);

    picker_init(&_picker);

    return _filename;
}

void picker_init(picker* new_picker)
{
    if(opened_pickers < 0)
    {
        opened_pickers = 0;
        return;
    }

    __picker = new_picker;
    __picker->selected = 0;
    __picker->update_needed = true;

    picker_update();

    while(true)
    {
        u8 input = smc_get_events();

        if(input & SMC_EJECT_BUTTON)
        {
            if(__picker->selected > __picker->directories - 1) // file
            {
                pick_sprintf(_filename, "%s/%s", __picker->path, __picker->file[__picker->selected - (__picker->directories)]);
            }
            else // directory
            {
                if (strcmp(__picker->directory[__picker->selected], ".") == 0 && __picker->folderpick)
                {
                    pick_sprintf(_filename, "%s/", __picker->path);
                }
                else if (strcmp(__picker->directory[__picker->selected], "..") == 0)
                {
                    if (opened_pickers < 1) // root picker
                    {
                        _filename = "";
                        break;
                    }

                    picker_init(picker_chain[--opened_pickers]);
                }
                else
                {
                    // Throw the current directory on to our stack.
                    picker_chain[opened_pickers++] = __picker;

                    pick_sprintf(_directory, "%s/%s", __picker->path, __picker->directory[__picker->selected]);
                    pick_file(_directory, __picker->folderpick, _filename);

                    // Has a file been selected yet? If not, we have to show the previous directory, user probably pressed B.
                    if(strlen(_filename) == 0)
                        picker_init(picker_chain[--opened_pickers]);
                }
            }

            break;
        }

        if(input & SMC_POWER_BUTTON) picker_next_selection();

        picker_update();
    }
}

void picker_print_filenames()
{
    int i = 0;
    char item_buffer[100] = {0};

    console_add_text(__picker->folderpick ? "Select a directory..." : "Select a file...");
    console_add_text("");

    for(i = __picker->show_y; i < (MAX_LINES - 6) + __picker->show_y; i++)
    {
        if(i < __picker->directories)
        {
            pick_snprintf(item_buffer, MAX_LINE_LENGTH, "  %s/ ", __picker->directory[i]);
        }
        else
        {
            pick_snprintf(item_buffer, MAX_LINE_LENGTH, "  %s ", __picker->file[i - __picker->directories]);
        }
        console_add_text(item_buffer);
    }
}

void picker_update()
{
    int i = 0, x = 0, y = 0;
    console_get_xy(&x, &y);
    if(__picker->update_needed)
    {
        console_init();
        picker_print_filenames();
        console_show();
        __picker->update_needed = false;
    }

    int header_lines_skipped = 2;

    // Update cursor.
    for(i = 0; i < (MAX_LINES - 6); i++)
    {
        gfx_draw_string(GFX_DRC, i == __picker->selected - __picker->show_y ? ">" : " ", x + CHAR_WIDTH, (i+header_lines_skipped) * CHAR_WIDTH + y + CHAR_WIDTH * 2, GREEN);
        gfx_draw_string(GFX_TV, i == __picker->selected - __picker->show_y ? ">" : " ", x + CHAR_WIDTH, (i+header_lines_skipped) * CHAR_WIDTH + y + CHAR_WIDTH * 2, GREEN);
    }
}

void picker_next_selection()
{
    if(__picker->selected + 1 < (__picker->files + __picker->directories))
        __picker->selected++;
    else
    {
        __picker->selected = 0;
        if(__picker->show_y)
        {
            __picker->show_y = 0;
            __picker->update_needed = true;
        }
    }

    if(__picker->selected > (MAX_LINES - 7) + __picker->show_y)
    {
        __picker->show_y++;
        __picker->update_needed = true;
    }

    picker_update();
}

void picker_prev_selection()
{
    if(__picker->selected > 0)
        __picker->selected--;

    if(__picker->selected < __picker->show_y)
    {
        __picker->show_y--;
        __picker->update_needed = true;
    }

    picker_update();
}
void picker_next_jump()
{
    int jump_num = 1;
    if(__picker->selected + 5 < (__picker->files + __picker->directories))
        jump_num = 5;
    else
        jump_num = (__picker->files + __picker->directories - 1) - __picker->selected;
    __picker->selected += jump_num;

    if(__picker->selected > (MAX_LINES - 7) + __picker->show_y)
    {
        __picker->show_y += jump_num;
        __picker->update_needed = true;
    }

    picker_update();
}

void picker_prev_jump()
{
    int jump_num = 1;
    if(__picker->selected > 5)
        jump_num = 5;
    else
        jump_num = __picker->selected;
    __picker->selected -= jump_num;

    if(__picker->selected < __picker->show_y)
    {
        __picker->show_y -= jump_num;
        __picker->update_needed = true;
    }

    picker_update();
}

#endif