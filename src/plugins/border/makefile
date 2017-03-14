BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp ./overlay.mm \
				  ./../../common/accessibility/window.cpp \
				  ./../../common/accessibility/element.cpp \
				  ./../../common/accessibility/display.mm
BINS			= $(BUILD_PATH)/border.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean: | $(BUILD_PATH)
	rm -f $(BUILD_PATH)/border.so

$(BUILD_PATH)/border.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
