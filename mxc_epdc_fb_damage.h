#ifndef __MXCFB_DAMAGE_H
#define __MXCFB_DAMAGE_H

#include "FBInk/eink/mxcfb-kobo.h"
#include <linux/fb.h>

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
