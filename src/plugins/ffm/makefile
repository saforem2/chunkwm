BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp \
				  ./../../common/dispatch/cgeventtap.cpp
BINS			= $(BUILD_PATH)/ffm.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

clean:
	rm -f $(BUILD_PATH)/ffm.so

$(BUILD_PATH)/ffm.so: $(SRC)
	g++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
