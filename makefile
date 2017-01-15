BUILD_FLAGS		= -O0 -g -std=c++11 -Wall -Wno-deprecate
BUILD_PATH		= ./bin
SRC				= ./src/core/chunkwm.cpp ./src/core/callback.cpp \
				  ./src/core/accessibility/carbon.cpp ./src/core/accessibility/event.cpp
BINS			= $(BUILD_PATH)/chunkwm
LINK			= -ldl -framework Carbon -framework Cocoa

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecate
install: clean $(BINS)

.PHONY: all clean install

$(BINS): | $(BUILD_PATH)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -rf $(BUILD_PATH)

$(BUILD_PATH)/chunkwm: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
