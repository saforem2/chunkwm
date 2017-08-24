BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.mm
BINS			= $(BUILD_PATH)/tiling
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices
NOW := $(shell date +"%c" | tr ' :' '__')

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/tiling*.so

$(BUILD_PATH)/tiling: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@.so $(LINK)
	mv $@.so $@_$(NOW).so
