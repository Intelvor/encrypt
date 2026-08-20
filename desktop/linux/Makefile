# Makefile - Linux GTK3 版本加解密工具
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lpthread -lm

SRC_DIR = src
BUILD_DIR = ../../build/linux

TARGET = $(BUILD_DIR)/encrypt-gtk

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/crypto.c $(SRC_DIR)/crypto_img.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)

install: $(TARGET)
	install -D $(TARGET) /usr/local/bin/encrypt-gtk

# 依赖
$(SRC_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/crypto.h $(SRC_DIR)/crypto_img.h
$(SRC_DIR)/crypto.o: $(SRC_DIR)/crypto.c $(SRC_DIR)/crypto.h
$(SRC_DIR)/crypto_img.o: $(SRC_DIR)/crypto_img.c $(SRC_DIR)/crypto_img.h
