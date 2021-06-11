/*
	mxc_epdc_fb_damage: Userspace access to framebuffer damage information on i.MX EPDCs
	Based on original work by @pl-semiotics <https://github.com/pl-semiotics/mxc_epdc_fb_damage>.
	Kobo port copyright (C) 2021 NiLuJe <ninuje@gmail.com>
	SPDX-License-Identifier: GPL-2.0-only
*/

// For KDevelop's sake...
#ifndef __KERNEL__
#	define __KERNEL__
#endif

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "mxc_epdc_fb_damage.h"

static int fbnode = 0;
module_param(fbnode, int, 0);

static atomic_t overflows = ATOMIC_INIT(0);

// Matches EPDC_V2_MAX_NUM_UPDATES
#define DMG_BUF_SIZE 64
/* For simplicity in read(), we store mxcfb_damage_update rather than mxcfb_update_data.
 *
 * The overflow_notify field is populated by read() and hence usually useless.
 */
typedef struct
{
	mxcfb_damage_update* buffer;
	int                  head;
	int                  tail;
} mxcfb_damage_circ_buf;

static mxcfb_damage_update   damage_buffer[DMG_BUF_SIZE];
static mxcfb_damage_circ_buf damage_circ = { .buffer = damage_buffer, .head = 0, .tail = 0 };
static DECLARE_WAIT_QUEUE_HEAD(listen_queue);
static int (*orig_fb_ioctl)(struct fb_info* info, unsigned int cmd, unsigned long arg);

static int
    fb_ioctl(struct fb_info* info, unsigned int cmd, unsigned long arg)
{
	int ret = orig_fb_ioctl(info, cmd, arg);
	if (cmd == MXCFB_SEND_UPDATE_V1_NTX) {
		/* The fb_ioctl() is called with the fb_info mutex held, so there is no need for additional locking here */
		int head = damage_circ.head;
		/* Said locking provide the needed ordering. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
		int tail = READ_ONCE(damage_circ.tail);
#else
		int tail = ACCESS_ONCE(damage_circ.tail);
#endif
		if (CIRC_SPACE(head, tail, DMG_BUF_SIZE) >= 1) {
			/* insert one item into the buffer */
			(void) !copy_from_user(
			    &damage_circ.buffer[head].data, (void __user*) arg, sizeof(damage_circ.buffer[head].data));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			smp_store_release(&damage_circ.head, (head + 1) & (DMG_BUF_SIZE - 1));
#else
			smp_wmb(); /* commit the item before incrementing the head */
			ACCESS_ONCE(damage_circ.head) = (head + 1) & (DMG_BUF_SIZE - 1);
#endif
		} else {
			atomic_inc(&overflows);
		}
		/* wake_up() will make sure that the head is committed before waking anyone up */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
		wake_up_interruptible_poll(&listen_queue, EPOLLIN | EPOLLRDNORM);
#else
		wake_up_interruptible_poll(&listen_queue, POLLIN | POLLRDNORM);
#endif
	}
	return ret;
}

static atomic_t readers = ATOMIC_INIT(0);

static int
    fbdamage_open(struct inode* inode, struct file* file)
{
	if (atomic_xchg(&readers, 1)) {
		// We're already open'ed by something!
		return -EBUSY;
	}
	return 0;
}

static int
    fbdamage_release(struct inode* inode, struct file* file)
{
	atomic_xchg(&readers, 0);
	return 0;
}

static ssize_t
    fbdamage_read(struct file* file, char __user* buffer, size_t count, loff_t* ppos)
{
	int head, tail;
	if (count < sizeof(mxcfb_damage_update)) {
		return -EINVAL;
	}
	/* no need for locks, since we only allow one reader */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* read index before reading contents at that index */
	head = smp_load_acquire(&damage_circ.head);
#else
	head = ACCESS_ONCE(damage_circ.head);
#endif
	tail = damage_circ.tail;
	while (!CIRC_CNT(head, tail, DMG_BUF_SIZE)) {
		// If the ring buffer is currently empty, wait for fb_ioctl to wake us up,
		// (at which point we'll be guaranteed to have something to read).

		if (file->f_flags & O_NONBLOCK) {
			// Except if we were open'ed in non-blocking mode, of course...
			return -EAGAIN;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		if (wait_event_interruptible(
			listen_queue, (CIRC_CNT(smp_load_acquire(&damage_circ.head), damage_circ.tail, DMG_BUF_SIZE)))) {
#else
		if (wait_event_interruptible(listen_queue,
					     (CIRC_CNT(ACCESS_ONCE(damage_circ.head), damage_circ.tail, DMG_BUF_SIZE)))) {
#endif
			return -ERESTARTSYS;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		head = smp_load_acquire(&damage_circ.head);
#else
		head = ACCESS_ONCE(damage_circ.head);
#endif
		tail = damage_circ.tail;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	/* read index before reading contents at that index */
	smp_rmb();
#endif

	/* extract one item from the buffer */
	damage_circ.buffer[tail].overflow_notify = atomic_xchg(&overflows, 0);
	if (copy_to_user(buffer, &damage_circ.buffer[tail], sizeof(mxcfb_damage_update))) {
		return -EFAULT;
	}
	/* Finish reading descriptor before incrementing tail. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	smp_store_release(&damage_circ.tail, (tail + 1) & (DMG_BUF_SIZE - 1));
#else
	smp_mb(); /* finish reading descriptor before incrementing tail */
	ACCESS_ONCE(damage_circ.tail) = (tail + 1) & (DMG_BUF_SIZE - 1);
#endif
	return sizeof(mxcfb_damage_update);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
static __poll_t
#else
static unsigned int
#endif
    fbdamage_poll(struct file* file, poll_table* wait)
{
	int head, tail;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
	__poll_t mask = 0;
#else
	unsigned int mask = 0;
#endif

	/* no need for locks, since we only allow one reader */
	poll_wait(file, &listen_queue, wait);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* read index before reading contents at that index */
	head = smp_load_acquire(&damage_circ.head);
#else
	head = ACCESS_ONCE(damage_circ.head);
#endif
	tail = damage_circ.tail;
	if (CIRC_CNT(head, tail, DMG_BUF_SIZE) >= 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
		mask = EPOLLIN | EPOLLRDNORM;
#else
		mask = POLLIN | POLLRDNORM;
#endif
	}

	return mask;
}

static dev_t                        dev;
static struct class*                fbdamage_class;
static struct device*               fbdamage_device;
static struct cdev                  cdev;
static const struct file_operations fbdamage_fops = { .owner   = THIS_MODULE,
						      .open    = fbdamage_open,
						      .read    = fbdamage_read,
						      .release = fbdamage_release,
						      .poll    = fbdamage_poll };

int
    init_module(void)
{
	int ret;

	if (!registered_fb[fbnode]) {
		return -ENODEV;
	}

	if ((ret = alloc_chrdev_region(&dev, 0, 1, "mxc_epdc_fb_damage"))) {
		return ret;
	}
	cdev_init(&cdev, &fbdamage_fops);
	if ((ret = cdev_add(&cdev, dev, 1) < 0)) {
		unregister_chrdev_region(dev, 1);
		return ret;
	}

	orig_fb_ioctl                          = registered_fb[fbnode]->fbops->fb_ioctl;
	registered_fb[fbnode]->fbops->fb_ioctl = fb_ioctl;

	fbdamage_class  = class_create(THIS_MODULE, "fbdamage");
	fbdamage_device = device_create(fbdamage_class, NULL, dev, NULL, "fbdamage");
	return 0;
}

void
    cleanup_module(void)
{
	cdev_del(&cdev);
	device_destroy(fbdamage_class, dev);
	class_destroy(fbdamage_class);
	unregister_chrdev_region(dev, 1);

	registered_fb[fbnode]->fbops->fb_ioctl = orig_fb_ioctl;
}

MODULE_LICENSE("GPL");
