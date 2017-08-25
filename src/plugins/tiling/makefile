BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
BIN_PATH		= ./../../../plugins
BUILD_PATH		= ./bin
SRC				= ./plugin.mm
BINS			= $(BUILD_PATH)/tiling
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices
DIR := ${CURDIR}
NOW := $(shell date "+%s")

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -rf $(BUILD_PATH)/tiling*
	rm -f $(BIN_PATH)/tiling.so

$(BUILD_PATH)/tiling: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@_$(NOW).so $(LINK)
	ln -s $(DIR)/$@_$(NOW).so $(BIN_PATH)/tiling.so
