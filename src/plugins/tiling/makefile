BUILD_FLAGS		= -O0 -g -std=c++11 -Wall -Wno-deprecated
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp ./node.cpp ./region.cpp \
				  ./../../common/accessibility/display.mm \
				  ./../../common/accessibility/application.cpp \
				  ./../../common/accessibility/window.cpp \
				  ./../../common/accessibility/element.cpp \
				  ./../../common/accessibility/observer.cpp \
				  ./../../common/dispatch/carbon.cpp \
				  ./../../common/dispatch/workspace.mm \
				  ./../../common/dispatch/cgeventtap.cpp \
				  ./../../common/ipc/daemon.cpp
BINS			= $(BUILD_PATH)/tiling.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

clean:
	rm -f $(BUILD_PATH)/tiling.so

$(BUILD_PATH)/tiling.so: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
