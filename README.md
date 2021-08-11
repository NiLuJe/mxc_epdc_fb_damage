# Introduction

This module provides userspace damage tracking for the i.MX framebuffers used on Kobo devices.
Support for the e-Ink sunxi display driver used on Kobo Mk. 8 is also provided, but, due to technical limitations,
only applies to clients with a disp handle opened *after* module insertion (i.e., if you want to track nickel's damage,
the module needs to be inserted very early in the boot process).

For some applications, damage-tracking information is useful, and the lack of efficient damage-tracking can be an issue when using the Linux framebuffer.
Since, with e-Ink displays, the kernel actually has the necessary information available, this module exports that information to userspace for programmatical use.
The reference consumer of the information from this module is [rM-vnc-server](https://github.com/peter-sa/rM-vnc-server), an efficient VNC server for the reMarkable tablet.

And as far as this Kobo port is concerned, that would be [NanoClock](https://github.com/NiLuJe/NanoClock), a persistent clock.

# Interface

When the module is loaded, it will inject damage recording infrastructure to a framebuffer device `/dev/fbn` specified by the `fbnode` module parameter (which defaults to `0`).
NOTE: On sunxi, this distinction is irrelevant as the Mk. 8 implementation only supports a single screen.
It will then create a `/dev/fbdamage` device, on which `read`s will block until damage is created.
Nonblocking `read`s and `poll` are *also* supported.

Each call to `read` will return a single [`mxcfb_damage_update` struct](./mxc_epdc_fb_damage.h).

The `data` member will be set to a custom [`mxcfb_damage_data` struct](./mxc_epdc_fb_damage.h), one that (mostly) matches the original `mxcfb_update_data` passed to the kernel in an `MXCFB_SEND_UPDATE` ioctl, but is defined entirely in the header, allowing you to *not* have to rely on kernel headers.
This is also done in order to be able to handle the various different ioctl & struct layouts across Kobo generations.

Speaking of, on sunxi, we split the native `update_mode` bitmask in three: `waveform_mode` is set to the `EINK_*_MODE` waveform mode value *only*; `update_mode` is 0 if the `EINK_RECT_MODE` bit is set, 1 otherwise (i.e., requesting a flash, although not every waveform mode can flash, and the rules differ slightly on that front compared to mxcfb); and `flags` is set to the remaining bits (e.g., `GET_UPDATE_INFO()`).

The `overflow_notify` member will be set if any updates have been discarded, and the `queue_size` member will be set to the amount of damage events in the kernel queue at `read` time (current one included, i.e., this will never be lower than 1).
Both should help dealing sanely with late reads and ioctl storms.

On sunxi, a couple of device attributes are also exposed via sysfs:
* `/sys/devices/virtual/fbdamage/fbdamage/rotate` reports the G2D rotation angle of the latest refresh (e.g., the value the `rotate` field points to in a `sunxi_disp_eink_update2` struct passed to the `DISP_EINK_UPDATE2` ioctl). This is extremely useful when you're attempting to cohabitate with an existing application, because rotation mismatches force a full layer blending and refresh, a process which incurs visible graphical artifacts when it implies a layout swap, too.
* `/sys/devices/virtual/fbdamage/fbdamage/pen_mode` reports whether the pen drawing mode is currently enabled (that information is also attached to each damage event).

# Building

This should build like any other Linux kernel module being cross-built for your target device, e.g.,
```
make -j8 CROSS_COMPILE=${CROSS_PREFIX} ARCH=arm INSTALL_MOD_PATH=/var/tmp/niluje/kobo/modules KDIR=/var/tmp/niluje/kobo/kernel
```

# Usage

Copy `mxc_epdc_fb_damage.ko` to your device and run `insmod` on it to load it.
If your platform has an mxc framebuffer numbered other than zero, pass `fbnode=n` to insmod (this should never be the case on Kobo).
