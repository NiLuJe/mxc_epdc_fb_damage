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

// FIXME: Handle the EPDCv2 ioctls & data format.
//        Either use an union of the two different structs, or flatten everything to a custom full strruct defined here.
//        (an union probably makes the copy easier in the module, but reads slightly more annoying to handle,
//         as we'd have to add a new field describing the data format or something).
typedef struct
{
	int                             overflow_notify;
	struct mxcfb_update_data_v1_ntx data;
} mxcfb_damage_update;

#endif
