BUILD_FLAGS		= -O0 -g -std=c++11 -Wall -Wno-deprecated
BUILD_PATH		= ./bin
SRC				= ./src/core/chunkwm.cpp ./src/core/callback.cpp \
				  ./src/core/plugin.cpp ./src/core/wqueue.cpp \
				  ./src/core/dispatch/carbon.cpp ./src/core/dispatch/event.cpp \
				  ./src/core/dispatch/workspace.mm ./src/core/dispatch/display.cpp
BINS			= $(BUILD_PATH)/chunkwm
LINK			= -ldl -lpthread -framework Carbon -framework Cocoa

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
