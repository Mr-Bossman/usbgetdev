BUILD_DIR = build
LIBRARY = libusbgetdev
PROGRAM = listdevs

CFLAGS :=	-Wall \
		-Wextra \
		-fPIC

LDFLAGS :=

HOST := $(shell $(CC) -dumpmachine)
C_SOURCES := src/libusbgetdev.c
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
DEPS = $(OBJECTS:%.o=%.d)

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.o $(BUILD_DIR)

ifneq (, $(shell pkg-config --version 2>/dev/null))
	CFLAGS += $(shell pkg-config --cflags libusb-1.0)
	LDFLAGS += $(shell pkg-config --libs libusb-1.0)
else
	CFLAGS += -I/usr/include/libusb-1.0
	LDFLAGS += -lusb-1.0
endif

run_check = printf "$(1)" | $(CC) -x c -c -S -Wall -Werror $(CFLAGS) -o - - > /dev/null 2>&1; echo $$?

have_plat_devid=\#include <libusb.h>\n int main(void) { return sizeof(libusb_get_platform_device_id); }

ifeq (0, $(shell $(call run_check,$(have_plat_devid))))
CFLAGS += -DHAVE_PLAT_DEVID
endif

ifneq (, $(findstring linux, $(HOST)))
C_SOURCES += src/linux_lib.c
else ifneq (, $(findstring darwin, $(HOST)))
C_SOURCES += src/darwin_lib.c
LDFLAGS += -framework IOKit -framework CoreFoundation
else ifneq (, $(findstring msys, $(HOST)))
C_SOURCES += src/windows_lib.c
LDFLAGS += -lsetupapi
else
$(error 'Could not determine the host type. Please set the $$HOST variable.')
endif

.PHONY: all clean debug example

all: $(LIBRARY).so $(LIBRARY).a

debug: CFLAGS += -g
debug: all

example:
	@$(MAKE) $(PROGRAM) C_SOURCES="$(C_SOURCES) src/listdevs.c"

$(OBJECTS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)
$(BUILD_DIR)/%.o: %.c
	$(CC) -MMD -c $(CFLAGS) $< -o $@

$(PROGRAM): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

$(LIBRARY).so: $(OBJECTS)
	$(CC) -shared $^ $(LDFLAGS) -o $@

$(LIBRARY).a: $(OBJECTS)
	$(AR) rcs $@ $^

clean:
	-rm -rf $(BUILD_DIR)
	-rm -f $(PROGRAM)
