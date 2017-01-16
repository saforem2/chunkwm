BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp \
				  ./../../common/accessibility/application.cpp \
				  ./../../common/accessibility/observer.cpp \
				  ./../../common/daemon/daemon.cpp
BINS			= $(BUILD_PATH)/tiling.so
LINK			= -shared -fPIC -framework Carbon -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

clean:
	rm -f $(BUILD_PATH)/tiling.so

$(BUILD_PATH)/tiling.so: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
