BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
BINS			= $(BUILD_PATH)/border.so
DEV_BIN_PATH	= ./../../../plugins
DEV_BUILD_PATH	= ./bin
DEV_BINS		= $(DEV_BUILD_PATH)/border
SRC				= ./plugin.mm
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices
DIR := ${CURDIR}
NOW := $(shell date "+%s")

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)
dev: clean $(DEV_BINS)

.PHONY: all clean install dev

$(DEV_BUILD_PATH):
	mkdir -p $(DEV_BUILD_PATH)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/border.so
	rm -rf $(DEV_BUILD_PATH)/border*
	rm -f $(DEV_BIN_PATH)/border.so

$(DEV_BUILD_PATH)/border: $(SRC) | $(DEV_BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@_$(NOW).so $(LINK)
	ln -sf $(DIR)/$@_$(NOW).so $(DEV_BIN_PATH)/border.so

$(BUILD_PATH)/border.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
