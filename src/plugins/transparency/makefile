BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp \
				  ./../../common/accessibility/application.cpp \
				  ./../../common/accessibility/window.cpp \
				  ./../../common/accessibility/element.cpp \
				  ./../../common/accessibility/observer.cpp \
				  ./../../common/misc/carbon.cpp \
				  ./../../common/misc/workspace.mm \
				  ./../../common/ipc/daemon.cpp
BINS			= $(BUILD_PATH)/transparency.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

clean:
	rm -f $(BUILD_PATH)/transparency.so

$(BUILD_PATH)/transparency.so: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
