#ifndef LIB_H
#define LIB_H

#include <stdlib.h>
#include "libusb.h"

int libusb_get_blockdev_path(libusb_device *dev, int iface_idx, char **path);
int libusb_get_chardev_path(libusb_device *dev, int iface_idx, char **path);

/* Private */
enum usbi_dev_type {
	USBI_DEV_BLOCK = 1,
	USBI_DEV_CHAR = 2,
};

int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path);

#endif /* !LIB_H */
