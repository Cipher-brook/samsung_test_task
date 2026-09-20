// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "common.h"

/**
 * DOC: Overview
 *
 * A few notes about the stt_cdev devices to make it easier to understand.
 * The stt_cdev device supports writing and reading strings;
 * other data is undefined.
 * The stt_cdev device doesn't support non-blocking input output.
 *
 * The module creates and registers two character devices (Device_0, Device_1)
 * with the same major number but different minor numbers (0 and 1). The major
 * number for the character device is dynamically allocated.
 * The functionality for Device_0 and Device_1 is the same.
 *
 * Each device supports the following operations:
 *
 * * open:
 *     opens the appropriate device (Device_0 or Device_1);
 * * read:
 *     reads last written string from a database;
 * * write:
 *     writes the string to a database;
 * * ioctl:
 *     clears string database(delete all strings);
 *     sets the reference string;
 * * fasync:
 *     sends string match event notification to the application if newly
 *     written string is equal with the reference string (previously set by
 *     IOCTL command);
 * * release:
 *     closes device;
 *
 * The module creates a folder in debug file system that corresponds to the
 * module name(stt_cdev) and file that corresponds to the minor number of the
 * specific device (0 or 1). When the file is read, it displays all the strings
 * that were written to the database of the specific device.
 *
 * When the module was loaded the devices and files below were added to your
 * operating system:
 *
 * 1. /dev/Device_0 - supports open, read, write, ioctl, fasync,
 *                    release operations
 * 2. /dev/Device_1 - supports open, read, write, ioctl, fasync,
 *                    release operations
 * 3. /sys/kernel/debug/stt_cdev/0 - only supports read all written strings
 *                                   operation
 * 4. /sys/kernel/debug/stt_cdev/1 - only supports read all written strings
 *                                   operation
 */

/**
 * struct db - represents database to store strings
 * @buf: database buffer
 * @size: database buffer size
 * @capacity: database buffer capacity
 * @last_str_size: last written string size
 * @ref_str: reference string
 * @rw_lock: read write semaphore to access database
 */
struct db {
	char *buf;
	size_t size;
	size_t capacity;
	size_t last_str_size;
	const char *ref_str;
	struct rw_semaphore rw_lock;
};

/**
 * struct cdev_data - represents character device data
 * @cdev: represents a character device
 * @db: character device database
 * @fasync: represents asynchronous notifications
 */
struct cdev_data {
	struct cdev cdev;
	struct db db;
	struct fasync_struct *fasync;
};

/**
 * struct cdev_mod_data - represents module data
 * @dev: represents a device
 * @cl: device classes
 * @debugfs_dir: debugfs directory
 * @cdev_data: an array of character devices data
 */
struct cdev_mod_data {
	dev_t dev;
	struct class *cl;
	struct dentry *debugfs_dir;
	struct cdev_data cdev_data[MAX_MINORS];
};

static struct cdev_mod_data mdata;

#define LOG(fmt, ...)						\
	pr_info(KMOD_NAME ": " fmt "\n", ##__VA_ARGS__)

#define ERR(fmt, ...)						\
	pr_err(KMOD_NAME ": [%s:%d] " fmt "\n",			\
		__func__, __LINE__, ##__VA_ARGS__)

#define DBG(fmt, ...)						\
	pr_debug(KMOD_NAME ": " fmt "\n", ##__VA_ARGS__)

#define MINOR_BUF_SIZE				2U
#define DEF_DB_CAPACITY				512U
#define DEV0					MKDEV(MAJOR(mdata.dev), 0UL)
#define DEV1					MKDEV(MAJOR(mdata.dev), 1UL)
#define CDEV_DRIVER_VERSION			"1.0.0"

static __always_inline void cdev_db_buffer_clear(struct db *db);

/**
 * cdev_fasync - notify the device of a change in it's FASYNC flag
 *               (asynchronous notification)
 * @fd: file descriptor
 * @f:  represents an open file
 * @on: notification state
 *
 * Returns negative on error, 0 if it did no changes
 * and positive if it added/deleted the entry.
 */
static int cdev_fasync(int fd, struct file *f, int on)
{
	struct cdev_data *cdev_data = (struct cdev_data *)f->private_data;

	return fasync_helper(fd, f, on, &cdev_data->fasync);
}

/**
 * cdev_db_increase - increase database buffer
 * @db:            character device database
 * @need_capacity: needed database buffer capacity
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int __must_check cdev_db_increase(struct db *db, size_t need_capacity)
{
	void *new_buf = NULL;

	if (unlikely(db->capacity > need_capacity)) {
		ERR("current capacity more that needed capacity");
		return -EINVAL;
	}

	db->capacity = need_capacity * 2U;

	new_buf = krealloc(db->buf, db->capacity, GFP_KERNEL);
	if (IS_ERR(new_buf)) {
		ERR("couldn't reallocate database buffer");
		return PTR_ERR(new_buf);
	}

	db->buf = (char *)new_buf;

	DBG("database increased, new capacity %zu", db->capacity);

	return 0;
}

/**
 * cdev_open - open the device
 * @i: internally represent file
 * @f: represents an open file
 *
 * Returns 0 on success, opening the device always succeeds.
 */
static int cdev_open(struct inode *i, struct file *f)
{
	struct cdev_data *cdev_data = NULL;

	cdev_data = container_of(i->i_cdev, struct cdev_data, cdev);

	f->private_data = (void *)cdev_data;

	return 0;
}

/**
 * cdev_release - release the device
 * @i: internally represent file
 * @f: represents an open file
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int cdev_release(struct inode *i, struct file *f)
{
	int ret = -1;

	ret = cdev_fasync(-1, f, 0);
	if (likely(ret >= 0))
		return 0;

	ERR("couldn't clear queue");

	return -EIO;
}

/**
 * cdev_read - read data from the device
 * @f:   represents an open file
 * @buf: user space buffer for reading data
 * @len: length of reading data
 * @pos: current reading position
 *
 * Returns the number of bytes read, otherwise negative error code.
 */
static
ssize_t cdev_read(struct file *f, char __user *buf, size_t len, loff_t *pos)
{
	int ret = -1;
	char *last_str = NULL;
	struct db *db = &((struct cdev_data *)f->private_data)->db;

	down_read(&db->rw_lock);

	if (unlikely(db->last_str_size > len)) {
		ERR("small buffer to read string, len=%zu", len);
		up_read(&db->rw_lock);
		return -EINVAL;
	}

	len = db->last_str_size;
	last_str = &db->buf[db->size - db->last_str_size - 1U];

	ret = copy_to_user((void __user *)buf, (const void *)last_str, len);
	if (unlikely(ret)) {
		ERR("couldn't copy to user");
		up_read(&db->rw_lock);
		return -EFAULT;
	}

	DBG("Read string %s", last_str);

	up_read(&db->rw_lock);

	return (ssize_t)len;
}

/**
 * cdev_write - write data to the device
 * @f:   represents an open file
 * @buf: user space buffer with written data
 * @len: length of writing data
 * @pos: current writing position
 *
 * Returns the number of bytes written, otherwise negative error code.
 */
static ssize_t cdev_write(struct file *f, const char __user *buf,
			  size_t len, loff_t *pos)
{
	int ret = -1;
	char *new_str = NULL;
	size_t str_size = len + 1U, need_capacity = 0U;
	struct cdev_data *cdev_data = (struct cdev_data *)f->private_data;
	struct db *db = &cdev_data->db;

	if (unlikely(!len))
		return 0;

	down_write(&db->rw_lock);
	need_capacity = db->size + str_size;
	if (need_capacity >= db->capacity) {
		ret = cdev_db_increase(db, need_capacity);
		if (unlikely(ret)) {
			ERR("couldn't increase db");
			up_write(&db->rw_lock);
			return ret;
		}
	}

	new_str = &db->buf[db->size];
	ret = copy_from_user((void *)new_str, (const void __user *)buf, len);
	if (unlikely(ret)) {
		ERR("couldn't copy from user");
		up_write(&db->rw_lock);
		return -EFAULT;
	}

	ret = strncmp(new_str, db->ref_str, len);
	if (!ret)
		kill_fasync(&cdev_data->fasync, SIGIO, POLL_IN);

	db->size += str_size;
	db->last_str_size = len;

	DBG("Written string %s", new_str);

	up_write(&db->rw_lock);

	return (ssize_t)len;
}

/**
 * cdev_ref_str_get - get reference string from user space
 * @arg: address user space data
 *
 * Returns pointer to new reference string on success,
 * otherwise a pointer with error encoded within it's value.
 */
static char __must_check *cdev_ref_str_get(unsigned long arg)
{
	int ret = -1;
	struct entry entry;
	char *new_ref_str = NULL;

	ret = copy_from_user((void *)&entry,
			     (const void __user *)arg, sizeof(struct entry));
	if (unlikely(ret)) {
		ERR("couldn't copy entry from user");
		return ERR_PTR(-EFAULT);
	}

	if (unlikely(!entry.size)) {
		ERR("entry size isn't correct");
		return ERR_PTR(-EINVAL);
	}

	new_ref_str = kmalloc(entry.size + 1U, GFP_KERNEL);
	if (IS_ERR(new_ref_str)) {
		ERR("couldn't allocate new reference string");
		return ERR_PTR(-ENOMEM);
	}

	ret = copy_from_user((void *)new_ref_str,
			     (const void __user *)entry.str, entry.size);
	if (unlikely(ret)) {
		ERR("couldn't copy new reference string from user");
		kfree((const void *)new_ref_str);
		return ERR_PTR(-EFAULT);
	}

	new_ref_str[entry.size] = '\0';

	return new_ref_str;
}

/**
 * cdev_data_reset - clear database buffer, update reference string
 * @cdev_data: character device data
 * @arg:       address user space data
 *
 * Returns 0 on success, otherwise negative error code.
 */
static __always_inline
int cdev_data_reset(struct cdev_data *cdev_data, unsigned long arg)
{
	char *new_ref_str = NULL;
	struct db *db = &cdev_data->db;

	new_ref_str = cdev_ref_str_get(arg);
	if (IS_ERR(new_ref_str)) {
		ERR("couldn't get new reference string from user");
		return PTR_ERR(new_ref_str);
	}

	down_write(&db->rw_lock);

	kfree((const void *)db->ref_str);
	db->ref_str = new_ref_str;
	cdev_db_buffer_clear(db);
	DBG("DB cleared, reference string %s", db->ref_str);

	up_write(&db->rw_lock);

	return 0;
}

/**
 * cdev_unlock_ioctl - offer a way to issue device-specific commands
 * @f:   represents an open file
 * @cmd: command
 * @arg: address user space data
 *
 * Returns 0 on success, otherwise negative error code.
 */
static
long cdev_unlock_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct cdev_data *cdev_data = (struct cdev_data *)f->private_data;

	if (likely(cmd == IOW_RESET))
		return cdev_data_reset(cdev_data, arg);

	return -EINVAL;
}

/* Supported operations for character device */
static const struct file_operations cdev_fops = {
	.owner = THIS_MODULE,
	.open = cdev_open,
	.release = cdev_release,
	.read = cdev_read,
	.write = cdev_write,
	.fasync = cdev_fasync,
	.unlocked_ioctl = cdev_unlock_ioctl
};

/**
 * cdev_debugfs_read - read data from debugfs file
 * @f:   represents an open file
 * @buf: user space buffer for reading data
 * @len: length of reading data
 * @pos: current reading position
 *
 * Returns the number of bytes read, otherwise negative error code.
 */
static ssize_t cdev_debugfs_read(struct file *f, char __user *buf,
				 size_t len, loff_t *pos)
{
	int ret = 0;
	struct db *db = &((struct cdev_data *)f->f_inode->i_private)->db;

	down_read(&db->rw_lock);

	if (unlikely(db->size > len)) {
		ERR("small buffer to read strings, len=%zu", len);
		up_read(&db->rw_lock);
		return -EINVAL;
	}

	len = db->size;

	ret = copy_to_user((void __user *)buf, (const void *)db->buf, len);
	if (unlikely(ret)) {
		ERR("couldn't copy strings to user");
		up_read(&db->rw_lock);
		return -EFAULT;
	}

	up_read(&db->rw_lock);

	return (ssize_t)len;
}

/* Supported operations for debugfs file */
static const struct file_operations debugfs_fops = {
	.owner = THIS_MODULE,
	.read = cdev_debugfs_read,
};

/**
 * cdev_debugfs_create - create files in debugfs
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int __cold __must_check cdev_debugfs_create(void)
{
	size_t i = 0U;

	mdata.debugfs_dir = debugfs_create_dir(KMOD_NAME, NULL);
	if (IS_ERR(mdata.debugfs_dir)) {
		ERR("couldn't create debugfs dir");
		return -EIO;
	}

	for (; i < MAX_MINORS; ++i) {
		int ret = -1;
		char minor[MINOR_BUF_SIZE];
		const struct dentry *file = NULL;

		ret = snprintf(minor, MINOR_BUF_SIZE, "%ld", i);
		if (unlikely(ret != MINOR_BUF_SIZE - 1)) {
			ERR("couldn't create minor for Device_%zu", i);
			debugfs_remove(mdata.debugfs_dir);
			return -ENODEV;
		}

		file = debugfs_create_file(minor, 0644, mdata.debugfs_dir,
					   (void *)&mdata.cdev_data[i],
					   &debugfs_fops);
		if (IS_ERR(file)) {
			ERR("couldn't create file for Device_%zu", i);
			debugfs_remove(mdata.debugfs_dir);
			return -ENODEV;
		}
	}

	return 0;
}

/**
 * cdev_db_init - allocate memory for database buffer,
 *                initialize internally database parameters
 * @db: character device database
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int __cold __must_check cdev_db_init(struct db *db)
{
	db->capacity = DEF_DB_CAPACITY;

	db->buf = kmalloc(db->capacity, GFP_KERNEL);
	if (IS_ERR(db->buf)) {
		ERR("couldn't allocate buffer");
		return PTR_ERR(db->buf);
	}

	init_rwsem(&db->rw_lock);

	db->size = 0U;
	db->ref_str = NULL;
	db->last_str_size = 0U;

	return 0;
}

/**
 * cdev_db_buffer_clear - clear database buffer
 * @db: character device database
 */
static __always_inline void cdev_db_buffer_clear(struct db *db)
{
	// if need to clear sensitive data
	// memzero_explicit(db->buf, db->size);

	db->size = 0U;
	db->last_str_size = 0U;
}

/**
 * cdev_db_free - clear and free database buffer, reference string
 * @db: character device database
 */
static void __cold cdev_db_free(struct db *db)
{
	cdev_db_buffer_clear(db);
	kfree((const void *)db->buf);
	db->buf = NULL;

	db->capacity = 0U;
}

/**
 * cdev_devices_create - create character devices in /dev
 * @cdev_data: character device data
 * @dev:       represents a device
 * @dev_name:  character device name
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int __cold __must_check
cdev_devices_create(struct cdev_data *cdev_data,
		    dev_t dev, const char *dev_name)
{
	int ret = -1;
	const struct device *dev_ret = NULL;

	dev_ret = device_create(mdata.cl, NULL, dev, NULL, dev_name);
	if (IS_ERR(dev_ret)) {
		ERR("couldn't create cdev %s", dev_name);
		ret = PTR_ERR(dev_ret);
		goto err_exit;
	}

	cdev_init(&cdev_data->cdev, &cdev_fops);

	ret = cdev_add(&cdev_data->cdev, dev, 1U);
	if (unlikely(ret)) {
		ERR("couldn't add cdev %s", dev_name);
		goto err_cdev_destroy;
	}

	ret = cdev_db_init(&cdev_data->db);
	if (unlikely(ret)) {
		ERR("couldn't init cdev %s", dev_name);
		goto err_cdev_del;
	}

	return ret;

err_cdev_del:
	cdev_del(&cdev_data->cdev);
err_cdev_destroy:
	device_destroy(mdata.cl, dev);
err_exit:
	return ret;
}

/**
 * cdev_dev_destroy - only destroy one character device data
 */
static void __cold cdev_dev_destroy(struct cdev_data *cdev_data, dev_t dev)
{
	kfree((const void *)cdev_data->db.ref_str);
	cdev_data->db.ref_str = NULL;
	cdev_db_free(&cdev_data->db);
	device_destroy(mdata.cl, dev);
	cdev_del(&cdev_data->cdev);
}

/**
 * cdev_devs_destroy - destroy all character devices data
 */
static void __cold cdev_devs_destroy(void)
{
	size_t i = 0U;
	dev_t dev = 0UL;

	for (; i < MAX_MINORS; ++i) {
		dev = i == MINOR(DEV0) ? DEV0 : DEV1;

		cdev_dev_destroy(&mdata.cdev_data[i], dev);
	}
}

/**
 * cdev_mod_init - driver initialization entry point,
 *                 run at kernel boot time or module insertion
 *
 * Returns 0 on success, otherwise negative error code.
 */
static int __init cdev_mod_init(void)
{
	int ret = -1;
	size_t i = 0U;
	dev_t dev = 0UL;
	const char *dev_name = NULL;

	ret = alloc_chrdev_region(&mdata.dev, 0, MAX_MINORS, "STT cdevs");
	if (unlikely(ret != 0)) {
		ret = -ENOMEM;
		ERR("couldn't allocate chrdev region");
		goto err_exit;
	}

	mdata.cl = class_create("chardrv");
	if (IS_ERR(mdata.cl)) {
		ret = PTR_ERR(mdata.cl);
		ERR("couldn't create chardrv");
		goto err_unregister_chrdev_region;
	}

	for (; i < MAX_MINORS; ++i) {
		dev = i == MINOR(DEV0) ? DEV0 : DEV1;
		dev_name = i == MINOR(DEV0) ? DEV0_NAME : DEV1_NAME;

		ret = cdev_devices_create(&mdata.cdev_data[i], dev, dev_name);
		if (unlikely(ret)) {
			ERR("couldn't create %s", dev_name);
			if (i == MINOR(DEV1))
				cdev_dev_destroy(&mdata.cdev_data[0U], DEV0);
			goto err_class_destroy;
		}
	}

	ret = cdev_debugfs_create();
	if (unlikely(ret)) {
		ERR("couldn't create debugfs");
		goto err_devices_destroy;
	}

	LOG("registered");

	return ret;

err_devices_destroy:
	cdev_devs_destroy();
err_class_destroy:
	class_destroy(mdata.cl);
err_unregister_chrdev_region:
	unregister_chrdev_region(mdata.dev, MAX_MINORS);
err_exit:
	return ret;
}

/**
 * cdev_mod_exit - driver exit entry point,
 *                 function to be run when driver is removed
 */
static void __exit cdev_mod_exit(void)
{
	debugfs_remove(mdata.debugfs_dir);
	cdev_devs_destroy();
	class_destroy(mdata.cl);
	unregister_chrdev_region(mdata.dev, MAX_MINORS);

	LOG("unregistered");
}

module_init(cdev_mod_init);
module_exit(cdev_mod_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Software engineer");
MODULE_DESCRIPTION("Samsung Test Task");
MODULE_VERSION(CDEV_DRIVER_VERSION);
