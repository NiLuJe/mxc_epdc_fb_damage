/*
	mxc_epdc_fb_damage: Userspace access to framebuffer damage information on i.MX EPDCs
	Based on original work by @pl-semiotics <https://github.com/pl-semiotics/mxc_epdc_fb_damage>.
	Kobo port copyright (C) 2021 NiLuJe <ninuje@gmail.com>
	SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef __MXCFB_DAMAGE_H
#define __MXCFB_DAMAGE_H

#ifndef __KERNEL__
#	include <stdint.h>
#endif

// Indicates the ioctl variant used (per damage event).
typedef enum
{
	DAMAGE_UPDATE_DATA_V1_NTX = 0,
	DAMAGE_UPDATE_DATA_V1,
	DAMAGE_UPDATE_DATA_V2,
	DAMAGE_UPDATE_DATA_ERROR,
	DAMAGE_UPDATE_DATA_UNKNOWN = 0xFF,
} mxcfb_damage_data_format;

// NOTE: We mimic these mxcfb structs because there are minor variants depending on the exact ioctl being used,
//       and this also allows us to entirely avoid a dependency on kernel headers.
//       Otherwise, we'd specifically need FBInk's frankenstein'ed headers,
//       like the kernel module itself has to in order to handle all the variants...

// Maps to an mxcfb_rect
typedef struct
{
	uint32_t top;
	uint32_t left;
	uint32_t width;
	uint32_t height;
} mxcfb_damage_rect;

// Maps to an mxcfb_alt_buffer_data
typedef struct
{
	void*             virt_addr;
	uint32_t          phys_addr;
	uint32_t          width;
	uint32_t          height;
	mxcfb_damage_rect alt_update_region;
} mxcfb_damage_alt_data;

// Maps to an mxcfb_update_data
typedef struct
{
	mxcfb_damage_rect     update_region;
	uint32_t              waveform_mode;
	uint32_t              update_mode;
	uint32_t              update_marker;
	int                   temp;
	unsigned int          flags;
	int                   dither_mode;
	int                   quant_bit;
	mxcfb_damage_alt_data alt_buffer_data;
} mxcfb_damage_data;

// And, finally, this is what read() will spit out :).
typedef struct
{
	int                      overflow_notify;
	mxcfb_damage_data_format format;
	mxcfb_damage_data        data;
} mxcfb_damage_update;

#endif
