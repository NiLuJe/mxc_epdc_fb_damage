# Introduction

This module provides userspace damage tracking for the i.MX
framebuffers used on Kobo devices.

For some applications, damage-tracking information is useful, and the
lack of efficient damage-tracking can be an issue when using the Linux
framebuffer. Since, for e-paper displays, the kernel actually has the
necessary information available, this module exports that information
to userspace for the use of programs. The reference consumer of the
information from this module is
[rM-vnc-server](https://github.com/peter-sa/rM-vnc-server), an
efficient VNC server for the reMarkable tablet.

# Interface

When the module is loaded, it will inject damage recording
infrastructure to a framebuffer device `/dev/fbn` specified by the
`fbnode` module parameter (which defaults to `0`). It will then create
a `/dev/fbdamage` device, on which `read`s will block until damage is
created. Each call to `read` will return a single [`struct
mxcfb_damage_update`](./mxc_epdc_fb_damage.h). The `overflow_notify`
member will be set if any updates have been discarded, and the `data`
member will be set to the original `mxcfb_update_data` passed to the
kernel in an `MXCFB_SEND_UPDATE` ioctl.

# Building

This should build like any other Linux kernel
module being cross-built for your target device,
e.g.,
```
make -j8 CROSS_COMPILE=${CROSS_PREFIX} ARCH=arm INSTALL_MOD_PATH=/var/tmp/niluje/kobo/modules KDIR=/var/tmp/niluje/kobo/kernel
```

# Usage

Copy `mxc_epdc_fb_damage.ko` to your device and run `insmod` on it
to load. If your platform has an mxc framebuffer numbered other than
zero, pass `fbnode=n` to insmod (this should never be the case on Kobo).
