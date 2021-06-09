#ifndef __MXCFB_DAMAGE_H
#define __MXCFB_DAMAGE_H

#include <linux/mxcfb.h>

struct mxcfb_damage_update
{
	int                      overflow_notify;
	struct mxcfb_update_data data;
};

#endif
