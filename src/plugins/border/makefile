BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BIN_PATH		= ./../../../plugins
BUILD_PATH      = ./bin
SRC				= ./plugin.mm
BINS			= $(BUILD_PATH)/border
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices
DIR := ${CURDIR}
NOW := $(shell date "+%s")

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall
install: clean $(BINS)

.PHONY: all clean install

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -rf $(BUILD_PATH)/border*
	rm -f $(BIN_PATH)/border.so

$(BUILD_PATH)/border: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@_$(NOW).so $(LINK)
	ln -s $(DIR)/$@_$(NOW).so $(BIN_PATH)/border.so
