#ifndef LIB_H
#define LIB_H

#include <stdlib.h>
#include "libusb.h"

enum usbi_dev_type {
	USBI_DEV_BLOCK = 1,
	USBI_DEV_CHAR = 2,
};

int libusb_get_blockdev_path(libusb_device *dev, int iface_idx, char **path);
int libusb_get_chardev_path(libusb_device *dev, int iface_idx, char **path);

#endif /* !LIB_H */
