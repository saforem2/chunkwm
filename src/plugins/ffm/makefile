BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp \
				  ./../../common/accessibility/element.cpp \
				  ./../../common/config/cvar.cpp \
				  ./../../common/config/tokenize.cpp \
				  ./../../common/dispatch/cgeventtap.cpp
BINS			= $(BUILD_PATH)/ffm.so
LINK			= -shared -fPIC -framework Carbon -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/ffm.so

$(BUILD_PATH)/ffm.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
