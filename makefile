BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall -Wno-deprecated
BUILD_PATH		= ./bin
SRC				= ./src/core/chunkwm.mm
BINS			= $(BUILD_PATH)/chunkwm
LINK			= -rdynamic -ldl -lpthread -framework Carbon -framework Cocoa

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecated
install: clean $(BINS)

.PHONY: all clean install

$(BINS): | $(BUILD_PATH)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -rf $(BUILD_PATH)

$(BUILD_PATH)/chunkwm: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
