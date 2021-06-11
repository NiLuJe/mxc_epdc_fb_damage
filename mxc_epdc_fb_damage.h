/*
	mxc_epdc_fb_damage: Userspace access to framebuffer damage information on i.MX EPDCs
	Based on original work by @pl-semiotics <https://github.com/pl-semiotics/mxc_epdc_fb_damage>.
	Kobo port copyright (C) 2021 NiLuJe <ninuje@gmail.com>
	SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef __MXCFB_DAMAGE_H
#define __MXCFB_DAMAGE_H

#include <linux/fb.h>

#include "FBInk/eink/mxcfb-kobo.h"

typedef enum
{
	DAMAGE_UPDATE_DATA_V1_NTX = 0,
	DAMAGE_UPDATE_DATA_V1,
	DAMAGE_UPDATE_DATA_V2,
	DAMAGE_UPDATE_DATA_UNKNOWN = 0xFF,
} mxcfb_damage_data_format;

typedef struct
{
	int                      overflow_notify;
	mxcfb_damage_data_format format;
	union
	{
		struct mxcfb_update_data_v1_ntx v1_ntx;
		struct mxcfb_update_data_v1     v1;
		struct mxcfb_update_data        v2;
	} data;
} mxcfb_damage_update;

#endif
