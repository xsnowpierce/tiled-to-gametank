CC      := gcc
CFLAGS  := -g -Wall -Wextra
BUILD   := build
TARGET  := $(BUILD)/TiledToGameTank
CJSON   := third_party/cJSON

.PHONY: all clean prereqs

all: $(TARGET)

$(TARGET): main.c $(CJSON)/cJSON.c $(CJSON)/cJSON.h | $(BUILD)
	$(CC) $(CFLAGS) -I$(CJSON) -o $@ main.c $(CJSON)/cJSON.c

$(BUILD):
	mkdir -p $@

prereqs:
	mkdir -p third_party
	git clone https://github.com/DaveGamble/cJSON.git $(CJSON)

clean:
	rm -rf $(BUILD)
