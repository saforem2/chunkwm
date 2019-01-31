BUILD_FLAGS     = -O0 -g -DCHUNKWM_DEBUG -std=c++11 -Wall -Wno-deprecated
#-DGIT_VERSION=\"$(GIT_VERSION)\"
BUILD_PATH      = ./bin
SRC             = ./src/core/chunkwm.mm
BINS            = $(BUILD_PATH)/chunkwm
LINK            = -rdynamic -ldl -lpthread -framework Carbon -framework Cocoa -framework ScriptingBridge  -DGIT_VERSION=\"$(GIT_VERSION)\"
GIT_VERSION    := "$(shell git describe --abbrev=4 --dirty --always)"


all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecated
install: clean $(BINS)

.PHONY: all clean install

$(BINS): | $(BUILD_PATH)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -rf $(BUILD_PATH)

$(BUILD_PATH)/chunkwm: $(SRC)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
