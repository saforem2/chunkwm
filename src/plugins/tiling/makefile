BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall -Wno-deprecated
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp ./node.cpp ./region.cpp ./vspace.cpp \
				  ./controller.cpp ./config.cpp \
				  ./../../common/accessibility/display.mm \
				  ./../../common/accessibility/application.cpp \
				  ./../../common/accessibility/window.cpp \
				  ./../../common/accessibility/element.cpp \
				  ./../../common/accessibility/observer.cpp \
				  ./../../common/ipc/daemon.cpp \
				  ./../../common/config/cvar.cpp \
				  ./../../common/config/tokenize.cpp \
				  ./../../common/misc/carbon.cpp \
				  ./../../common/misc/workspace.mm
BINS			= $(BUILD_PATH)/tiling.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

clean:
	rm -f $(BUILD_PATH)/tiling.so

$(BUILD_PATH)/tiling.so: $(SRC)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
