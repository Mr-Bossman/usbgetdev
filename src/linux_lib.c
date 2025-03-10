#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#include "libusb.h"
#include "lib.h"

#define SYSFS_DEVICE_PATH "/sys/bus/usb/devices"

/*
 * Check if a sysfs directory matches the given subsystem.
 * Subsystem being `/sys/class/.*`
 * Returns 0 if the subsystem matches
 * Returns 1 if the subsystem does not match
 * Returns LIBUSB_ERROR code on error
 */
static int check_subsystem(const char *sys_path, const char* subsystem)
{
	char *path, *subsystem_path;
	int ret;

	ret = asprintf(&path, "%s/subsystem", sys_path);
	if (ret < 0)
		return LIBUSB_ERROR_NO_MEM;

	subsystem_path = realpath(path, NULL);
	free(path);
	if (!subsystem_path && errno != ENOENT)
		return LIBUSB_ERROR_IO;

	if (subsystem_path) {
		ret = !!strcmp(subsystem_path, subsystem);
		free(subsystem_path);
	} else {
		ret = 1;
	}

	return ret;
}

static int get_subsytem(char **buf,
	const char *dir, const char* subsystem, int depth)
{
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;
	char *path;
	int ret;

	/* Arbitrary max recursion depth */
	if (depth >= 20)
		return LIBUSB_ERROR_NOT_FOUND;

	ret = check_subsystem(dir, subsystem);
	if (ret < 0)
		return ret;

	if (!ret) {
		path = strrchr(dir, '/');
		if(path && strlen(path) > 1) {
			ret = asprintf(buf, "/dev%s", path);
			if (ret < 0) {
				*buf = NULL;
				return LIBUSB_ERROR_NO_MEM;
			}
			return LIBUSB_SUCCESS;
		}
	}
	ret = LIBUSB_ERROR_NOT_FOUND;

	if ((dp = opendir(dir)) == NULL) {
		fprintf(stderr, "opendir devices failed, errno=%d", errno);
		return LIBUSB_ERROR_IO;
	}

	while ((entry = readdir(dp)) != NULL) {
		if(strcmp(".", entry->d_name) == 0 ||
		   strcmp("..", entry->d_name) == 0)
			continue;

		ret = asprintf(&path, "%s/%s", dir, entry->d_name);
		if (ret < 0) {
			ret = LIBUSB_ERROR_NO_MEM;
			break;
		}

		ret = lstat(path, &statbuf);
		if (ret < 0) {
			free(path);
			ret = LIBUSB_ERROR_IO;
			break;
		}

		ret = LIBUSB_ERROR_NOT_FOUND;
		if (S_ISDIR(statbuf.st_mode))
			ret = get_subsytem(buf, path, subsystem, depth + 1);
		free(path);

		if (ret != LIBUSB_ERROR_NOT_FOUND)
			break;
	}

	closedir(dp);
	return ret;
}

#if 0
#define SYSFS_DIR_LEN 256
static int get_sysfs_dir(struct libusb_device *dev, char **path)
{
	*path = malloc(256);

	if(!*path)
		return LIBUSB_ERROR_NO_MEM;

	return libusb_get_platform_device_id(dev, *path, SYSFS_DIR_LEN);
}
#else
static int get_sysfs_dir(struct libusb_device *dev, char **path)
{
	int ret;
	uint8_t port_path[8];

	ret = libusb_get_port_numbers(dev, port_path, sizeof(port_path));
	if (ret < 0)
		return ret;
	else if (ret == 0)
		return LIBUSB_ERROR_NOT_FOUND;

	/* If r is >1 does that mean it is a HUB? */
	ret = asprintf(path, "%d-%d", libusb_get_bus_number(dev), port_path[0]);
	if (ret < 0)
		return LIBUSB_ERROR_NO_MEM;

	return LIBUSB_SUCCESS;
}
#endif

static int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path)
{
	int ret, active_config;
	char *sysfs_dir, *dir;

	active_config = 1;
/*
	ret = sysfs_get_active_config(dev, &active_config);
	if (ret < 0)
		return ret;

	if (active_config == -1) {
		fprintf(stderr, "device unconfigured");
		return LIBUSB_ERROR_NOT_FOUND;
	}
*/

	if (!get_sysfs_dir(dev, &sysfs_dir))
		return LIBUSB_ERROR_NOT_FOUND;

	/* root hub? */
	if (!strchr(sysfs_dir, '-')) {
		free(sysfs_dir);
		return LIBUSB_ERROR_NOT_FOUND;
	}

	ret = asprintf(&dir,  SYSFS_DEVICE_PATH "/%s:%d.%d", sysfs_dir,
		       active_config, iface_idx);
	free(sysfs_dir);
	if (ret < 0)
		return LIBUSB_ERROR_NO_MEM;

	if (dev_type == USBI_DEV_BLOCK)
		ret = get_subsytem(path, dir, "/sys/class/block", 0);
	else if(dev_type == USBI_DEV_CHAR)
		ret = get_subsytem(path, dir, "/sys/class/tty", 0);
	else
		ret = LIBUSB_ERROR_NOT_FOUND;
	free(dir);

	return ret;
}

/** \ingroup libusb_misc
 * Get the block device path of USB resource.
 * A string that contains a block device that is associated
 * with the USB resource using the active config descriptor.
 * An example path on *nix is `/dev/sdaX`
 *
 * \param dev a device
 * \param iface_idx the <tt>bInterfaceNumber</tt> of the interface you wish to probe
 * \param path pointer to an allocated string that will contain the block device path
 * if the function is successful, or NULL on error
 * \note The caller is responsible for freeing the string.
 * \returns 0 on success
 * \returns \ref LIBUSB_ERROR_NOT_FOUND the device doesn't have an associated device
 * \returns another LIBUSB_ERROR code on error
 */
int libusb_get_blockdev_path(libusb_device *dev, int iface_idx, char **path)
{
	struct libusb_config_descriptor *config;
	int r;

	*path = NULL;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		fprintf(stderr, "could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	if (iface_idx >= config->bNumInterfaces)
		return LIBUSB_ERROR_NOT_FOUND;

	return get_dev_path(dev, iface_idx, USBI_DEV_BLOCK, path);
}

/** \ingroup libusb_misc
 * Get the character device path of USB resource.
 * A string that contains a character device that is associated
 * with the USB resource using the active config descriptor.
 * An example path on *nix is `/dev/ttyX`
 *
 * \param dev a device
 * \param iface_idx the <tt>bInterfaceNumber</tt> of the interface you wish to probe
 * \param path pointer to an allocated string that will contain the character device path
 * if the function is successful, or NULL on error
 * \note The caller is responsible for freeing the string.
 * \returns 0 on success
 * \returns \ref LIBUSB_ERROR_NOT_FOUND the device doesn't have an associated device
 * \returns another LIBUSB_ERROR code on error
 */
int libusb_get_chardev_path(libusb_device *dev, int iface_idx, char **path)
{
	struct libusb_config_descriptor *config;
	int r;

	*path = NULL;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		fprintf(stderr, "could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	if (iface_idx >= config->bNumInterfaces)
		return LIBUSB_ERROR_NOT_FOUND;

	return get_dev_path(dev, iface_idx, USBI_DEV_CHAR, path);
}
