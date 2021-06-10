// For KDevelop's sake...
#ifndef __KERNEL__
#	define __KERNEL__
#endif

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "mxc_epdc_fb_damage.h"

static int fbnode = 0;
module_param(fbnode, int, 0);

static atomic_t overflows = ATOMIC_INIT(0);

// Matches EPDC_V2_MAX_NUM_UPDATES
#define NUM_UPD_BUF 64U
/* For simplicity in read(), we store mxcfb_damage_update rather than mxcfb_update_data.
 *
 * The overflow_notify field is populated by read() and hence usually useless.
 */
typedef struct
{
	mxcfb_damage_update* buffer;
	size_t               head;
	size_t               tail;
	size_t               size;
} mxcfb_damage_circ_buf;

/*
static mxcfb_damage_update damage_buffer[NUM_UPD_BUF];
static mxcfb_damage_circ_buf damage_circ_buf = {
	.buffer = damage_buffer,
	.head   = 0,
	.tail   = 0,
	.size   = NUM_UPD_BUF
};
*/
static mxcfb_damage_circ_buf damage_circ_buf;

/*
struct mxcfb_damage_update upd_data[NUM_UPD_BUF];
unsigned long              upd_buf_head = 0;
unsigned long              upd_buf_tail = NUM_UPD_BUF - 1;
*/

static DECLARE_WAIT_QUEUE_HEAD(listen_queue);

static int (*orig_fb_ioctl)(struct fb_info* info, unsigned int cmd, unsigned long arg);

static int
    fb_ioctl(struct fb_info* info, unsigned int cmd, unsigned long arg)
{
	int ret = orig_fb_ioctl(info, cmd, arg);
	if (cmd == MXCFB_SEND_UPDATE_V1_NTX) {
		/* The fb_ioctl() is called with the fb_info mutex held, so there is no need for additional locking here */
		size_t head = damage_circ_buf.head;
		/* Said locking provide the needed ordering. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
		size_t tail = READ_ONCE(damage_circ_buf.tail);
#else
		size_t tail = ACCESS_ONCE(damage_circ_buf.tail);
#endif
		if (CIRC_SPACE(head, tail, NUM_UPD_BUF) >= 1) {
			/* insert one item into the buffer */
			(void) !copy_from_user(&damage_circ_buf.buffer[head].data,
					       (void __user*) arg,
					       sizeof(damage_circ_buf.buffer[head].data));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			smp_store_release(&damage_circ_buf.head, (head + 1) & (NUM_UPD_BUF - 1));
#else
			smp_wmb(); /* commit the item before incrementing the head */
			ACCESS_ONCE(damage_circ_buf.head) = (head + 1) & (NUM_UPD_BUF - 1);
#endif
		} else {
			atomic_inc(&overflows);
		}
		/* wake_up() will make sure that the head is committed before waking anyone up */
		wake_up(&listen_queue);
	}
	return ret;
}

static atomic_t readers = ATOMIC_INIT(0);

static int
    fbdamage_open(struct inode* inode, struct file* filp)
{
	if (atomic_xchg(&readers, 1)) {
		return -ENODEV;
	}
	return 0;
}
static int
    fbdamage_release(struct inode* inode, struct file* filp)
{
	atomic_xchg(&readers, 0);
	return 0;
}

static ssize_t
    fbdamage_read(struct file* filp, char __user* buff, size_t count, loff_t* offp)
{
	size_t head, tail;
	if (count < sizeof(mxcfb_damage_update)) {
		return -EINVAL;
	}
	/* no need for locks, since we only allow one reader */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* read index before reading contents at that index */
	head = smp_load_acquire(&damage_circ_buf.head);
#else
	head = ACCESS_ONCE(damage_circ_buf.head);
#endif
	tail = damage_circ_buf.tail;
	while (!CIRC_CNT(head, tail, NUM_UPD_BUF)) {
		// If the ring buffer is currently empty, wait for fb_ioctl to wake us up,
		// (at which point we'll be guaranteed to have something to read).
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		if (wait_event_interruptible(
			listen_queue,
			(CIRC_CNT(smp_load_acquire(&damage_circ_buf.head), damage_circ_buf.tail, NUM_UPD_BUF)))) {
#else
		if (wait_event_interruptible(
			listen_queue, (CIRC_CNT(ACCESS_ONCE(damage_circ_buf.head), damage_circ_buf.tail, NUM_UPD_BUF)))) {
#endif
			return -ERESTARTSYS;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		head = smp_load_acquire(&damage_circ_buf.head);
#else
		head = ACCESS_ONCE(damage_circ_buf.head);
#endif
		tail = damage_circ_buf.tail;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	/* read index before reading contents at that index */
	smp_rmb();
#endif

	/* extract one item from the buffer */
	damage_circ_buf.buffer[tail].overflow_notify = atomic_xchg(&overflows, 0);
	if (copy_to_user(buff, &damage_circ_buf.buffer[tail], sizeof(mxcfb_damage_update))) {
		return -EFAULT;
	}
	/* Finish reading descriptor before incrementing tail. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	smp_store_release(&damage_circ_buf.tail, (tail + 1) & (NUM_UPD_BUF - 1));
#else
	smp_mb(); /* finish reading descriptor before incrementing tail */
	ACCESS_ONCE(damage_circ_buf.tail) = (tail + 1) & (NUM_UPD_BUF - 1);
#endif
	return sizeof(mxcfb_damage_update);
}

static dev_t                        dev;
static struct class*                fbdamage_class;
static struct device*               fbdamage_device;
static struct cdev                  cdev;
// TODO: Add a poll handler.
static const struct file_operations fbdamage_fops = { .owner   = THIS_MODULE,
						      .open    = fbdamage_open,
						      .read    = fbdamage_read,
						      .release = fbdamage_release };

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

	damage_circ_buf.buffer = kcalloc(NUM_UPD_BUF, sizeof(*damage_circ_buf.buffer), GFP_KERNEL);
	if (!damage_circ_buf.buffer) {
		return -ENOMEM;
	}
	damage_circ_buf.size = NUM_UPD_BUF;

	orig_fb_ioctl                          = registered_fb[fbnode]->fbops->fb_ioctl;
	registered_fb[fbnode]->fbops->fb_ioctl = fb_ioctl;

	fbdamage_class  = class_create(THIS_MODULE, "fbdamage");
	fbdamage_device = device_create(fbdamage_class, NULL, dev, NULL, "fbdamage");
	return 0;
}

void
    cleanup_module(void)
{
	kfree(damage_circ_buf.buffer);
	cdev_del(&cdev);
	device_destroy(fbdamage_class, dev);
	class_destroy(fbdamage_class);
	unregister_chrdev_region(dev, 1);

	registered_fb[fbnode]->fbops->fb_ioctl = orig_fb_ioctl;
}

MODULE_LICENSE("GPL");
