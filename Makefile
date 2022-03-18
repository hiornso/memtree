CC ?= clang
PKGCONFIG ?= pkg-config

OS ?= $(shell uname)

OPT ?= 2

WARNINGS += -Wall -Wextra -Wpedantic -Wno-overlength-strings
INCLUDE += -Ibackends/$(OS)
CFLAGS += -g -MMD -MP `$(PKGCONFIG) --cflags gtk4` -O$(OPT) $(WARNINGS) $(INCLUDE) -pthread
LDFLAGS += -O$(OPT)
LDLIBS += `$(PKGCONFIG) --libs gtk4` -lpthread

BUILD_DIR ?= build

SRCS = main.c caching.c table.c tablesearch.c tablesort.c label.c backends/$(OS)/getapps.c backends/$(OS)/backend.c resources.c
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(patsubst %.c,$(BUILD_DIR)/%.d,$(SRCS))

TARGET ?= memtree

.PHONY: default all clean
default: $(TARGET)
all: $(TARGET)

-include $(DEPS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(BUILD_DIR)/backends/$(OS): | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/backends/$(OS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR) $(BUILD_DIR)/backends/$(OS)
	$(CC) $(CFLAGS) -c -o $@ $<

resources.c: memtree.gresource.xml $(wildcard resources/*/*) | $(BUILD_DIR)
	glib-compile-resources --generate-source --target=$@ $<

clean:
	rm -rf $(TARGET) $(BUILD_DIR) resources.c