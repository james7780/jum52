# Makefile for Raspbian version of Jum52
#
# compile SDL project from cli like so:
#gcc -o test test.c `sdl-config --cflags --libs`

CC = gcc
RM = rm

# get the proper CFLAGS and LDFLAGS for SDL:
#SDL_CFLAGS := $(shell sdl-config --cflags)
#SDL_LDFLAGS := $(shell sdl-config --libs)

#CFLAGS := -std=c99 -Wall -O2 -DRASPBIAN $(shell sdl-config --cflags)
CFLAGS := -std=c99 -Wall -g -DRASPBIAN $(shell sdl-config --cflags)
LDFLAGS := $(shell sdl-config --libs)

TARGET = jum52
OBJS = main.o 6502dis.o ../5200gfx.o ../52emu.o ../6502.o ../font5200.o ../pokey.o ../crc32.o ../gamesave.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

clean:
	$(RM) -f $(TARGET)
	$(RM) -f $(OBJS)

