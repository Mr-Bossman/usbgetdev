/*static IOReturn IORegistryChildConformsTo(const io_service_t *entry, io_service_t *child, const io_name_t class_name) {
	CFMutableDictionaryRef matchingDict;

	matchingDict = IOServiceMatching (class_name);

	*child = IOServiceGetMatchingService (*entry, matchingDict);

	if (*child == IO_OBJECT_NULL)
		return kIOReturnNoDevice;

	return kIOReturnSuccess;

}*/

static IOReturn IORegistryChildConformsTo(const io_service_t *service, io_service_t *child, const io_name_t class_name) {
	io_service_t	int_service = IO_OBJECT_NULL;
	io_iterator_t	deviceIterator = MACH_PORT_NULL;
	IOReturn	kresult;

	*child = IO_OBJECT_NULL;

	kresult = IORegistryEntryCreateIterator(*service, kIOServicePlane, kIORegistryIterateRecursively, &deviceIterator);
	if (kresult != kIOReturnSuccess)
		return kresult;

	while (1) {
		int_service = IOIteratorNext (deviceIterator);
		if (int_service == IO_OBJECT_NULL)
			break;

		if (IOObjectConformsTo(int_service, class_name) == TRUE) {
			*child = int_service;
			IOObjectRelease (deviceIterator);

			return kIOReturnSuccess;
		}

		IOObjectRelease (int_service);
	}

	IOObjectRelease (deviceIterator);

	return kIOReturnNoDevice;
}

static IOReturn IORegistryFromLegacy(io_service_t *service, uint32_t location) {
	CFMutableDictionaryRef propertyMatchDict, matchingDict;
	CFTypeRef locationCF;

	matchingDict = IOServiceMatching (kIOUSBHostDeviceClassName);

	propertyMatchDict = CFDictionaryCreateMutable (kCFAllocatorDefault, 0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	locationCF = CFNumberCreate (NULL, kCFNumberSInt32Type, &location);

	CFDictionarySetValue (matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
	CFDictionarySetValue (propertyMatchDict, CFSTR(kUSBDevicePropertyLocationID), locationCF);

	CFRelease (locationCF);
	CFRelease (propertyMatchDict);

	*service = IOServiceGetMatchingService (darwin_default_master_port, matchingDict);

	return kIOReturnSuccess;
}

int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path) {

	struct darwin_cached_device	*priv = DARWIN_CACHED_DEVICE(dev);
	io_service_t			service = IO_OBJECT_NULL;
	io_service_t			child = IO_OBJECT_NULL;
	IOReturn			kresult;
	CFTypeRef			cf_bsd;
	bool				ret;
	CFIndex				length;

	if (0 == priv->active_config)
		return LIBUSB_ERROR_NOT_FOUND;

	kresult = IORegistryFromLegacy(&service, priv->location);
	if (kresult != kIOReturnSuccess)
		return LIBUSB_ERROR_NOT_FOUND;

	if (IORegistryChildConformsTo(&service, &child, "IOStorage") != kIOReturnSuccess) {
		IOObjectRelease (service);
		return LIBUSB_ERROR_NOT_FOUND;
	}

	IOObjectRelease (service);

	cf_bsd = IORegistryEntrySearchCFProperty (service, kIOServicePlane, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, kIORegistryIterateRecursively);
	if (!cf_bsd) {
		IOObjectRelease (child);
		return LIBUSB_ERROR_NOT_FOUND;
	}

	/* Add 1 for NULL terminator */
      	length = CFDataGetLength (cf_bsd) + 1;

	*path = malloc(length);

	if (!*path)
		return LIBUSB_ERROR_NO_MEM;

	ret = CFStringGetCString(cf_bsd, *path, length, kCFStringEncodingASCII);

	CFRelease (cf_bsd);
	IOObjectRelease (child);

	if (ret)
		return LIBUSB_SUCCESS;

	free(*path);
	*path = NULL;

	return LIBUSB_ERROR_NOT_FOUND;
}

//IORegistryEntryLocation