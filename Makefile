BUILD_DIR = build
PROGRAM = listdevs

CFLAGS :=	$(shell pkg-config --cflags libusb-1.0) \
		-Wall \
		-Wextra

LDFLAGS :=	$(shell pkg-config --libs libusb-1.0)


HOST := $(shell gcc -dumpmachine)
C_SOURCES = src/listdevs.c src/lib.c
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
DEPS = $(OBJECTS:%.o=%.d)

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.o $(BUILD_DIR)


ifneq (, $(findstring linux, $(HOST)))
C_SOURCES += src/linux_lib.c
else ifneq (, $(findstring darwin, $(HOST)))
C_SOURCES += src/darwin_lib.c
LDFLAGS += -framework IOKit -framework CoreFoundation
else
$(error 'Could not determine the host type. Please set the $$HOST variable.')
endif

.PHONY: all clean debug

all: $(PROGRAM)

debug: CFLAGS += -g
debug: all

$(OBJECTS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)
$(BUILD_DIR)/%.o: %.c
	$(CC) -MMD -c $(CFLAGS) $< -o $@

$(PROGRAM): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	-rm -rf $(BUILD_DIR)
	-rm -f $(PROGRAM)
