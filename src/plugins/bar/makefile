BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./../../../plugins
SRC				= ./plugin.cpp
BINS			= $(BUILD_PATH)/bar.so
INCLUDE         = -I/opt/local/include -I/opt/local/include/freetype2
LINK			= -L/opt/local/lib -shared -fPIC -framework Carbon -framework ApplicationServices -framework OpenGL -lfreetype

all: clean $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/bar.so

$(BUILD_PATH)/bar.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(INCLUDE) $(LINK)
