# Makefile - Linux GTK3 版本加解密工具
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -pthread $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -pthread -lm

SRC_DIR = src
BUILD_DIR = ../../build/linux

TARGET = $(BUILD_DIR)/encrypt-gtk

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/crypto.c $(SRC_DIR)/crypto_img.c
OBJECTS = $(SOURCES:.c=.o)

PREFIX ?= /usr/local
DESTDIR ?=

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 增量编译：源文件或头文件变化时只重编对应 .o
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/crypto.h $(SRC_DIR)/crypto_img.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)$(PREFIX)/bin/encrypt-gtk
