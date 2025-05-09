/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _MAIN_H
#define _MAIN_H

#include "types.h"

// menu: main options
void main_copy(void);
void main_move(void);
void main_delete(void);
void main_reset(void);
void main_shutdown(void);
void main_credits(void);

// menu: disk options
void disk_slc(void);
void disk_sdmc(void);
void disk_back(void);

#endif
