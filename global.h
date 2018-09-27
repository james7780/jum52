// global.h
// include file shared by all source files in Jum52

#ifndef GLOBAL_H
#define GLOBAL_H

//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>

// Sizes
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
//typedef signed char int8;
//typedef signed short int16;
//typedef signed long int32;
typedef char int8;
typedef short int16;
typedef long int32;


// Defines
#define JUM52_VERSION	"1.3"
#define VID_HEIGHT			270
#define VID_WIDTH				384
#define VISIBLE_HEIGHT	240
#define VISIBLE_WIDTH		320

#ifdef _EE
#define SND_RATE	48000
#else
#define SND_RATE	44100
#endif

#define CONT_MODE_ANALOG    1
#define CONT_MODE_DIGITAL   0

// Controllers
typedef struct
{
	int mode;           // 0 = digital, 1 = analog

	// All values are 1 or 0, or perhaps not...
	int left;
	int right;
	int up;
	int down;

	// JH 6/2002 - for PS2 analog sticks
	unsigned char analog_h;
	unsigned char analog_v;

	int trig;
	int side_button;

	// These may be set to 1. The core handles clearing them.
	// [BREAK]  [ # ]  [ 0 ]  [ * ]
	// [RESET]  [ 9 ]  [ 8 ]  [ 7 ]
	// [PAUSE]  [ 6 ]  [ 5 ]  [ 4 ]
	// [START]  [ 3 ]  [ 2 ]  [ 1 ]
	int key[16];
	int last_key_still_pressed;

	// Mode
	int fake_mouse_support;

	// Do not touch these (private data)
	int vpos, hpos;
	int lastRead;
} CONTROLLER;

extern CONTROLLER cont1, cont2;

// Note: VSS uses 15/120/220 for LEFT/CENTRE/RIGHT
#define POT_CENTRE 115
#define POT_LEFT   15
#define POT_RIGHT  210
extern int pot_max_left;
extern int pot_max_right;

// for direct samples
#define MAX_SAMPLE_EVENTS   200
typedef struct
{
	short vcount;
	unsigned char value;
} SampleEvent_t;

extern int numSampleEvents[4];
extern SampleEvent_t sampleEvent[4][MAX_SAMPLE_EVENTS];

extern void clearSampleEvents(void);
extern void addSampleEvent(int vcount, int channel, unsigned char value);
extern void renderMixSampleEvents(unsigned char *pokeyBuf, uint16 size);

// Global vars
typedef struct
{
	int videomode;				// 1 (NTSC) or 15 (PAL)
	int debugmode;				// 0 or 1
	int controller;				// 0 (keys), 1 (joystick), 2 (mouse)
	int controlmode;			// 0 (normal), 1 (robotron), 2 (pengo)
	int audio;					// 0 (off) or 1 (on)
	int voice;					// 0 (off) or 1 (on)
	int volume;					// 0 to 100 %
	int fullscreen;				// 0 (off/windowed) or 1 (on)
	int scale;					// 1, 2, 3, 4
	int slow;					// 0 (off) or 1 (on)
} Options_t;

extern Options_t options;

#define NUM_16K_ROM_MAPS 200

typedef struct
{				// 80 bytes
	char crc[8];
	int mapping;
	char description[68];
} Map16k_t;

extern Map16k_t *p16Maps;

//extern int num16kMappings;			// number of 16k rom mappings

#define KEY_MAP_SIZE 320		// size of key map
extern unsigned short keyMap[KEY_MAP_SIZE];

extern unsigned int colourtable[256];
extern uint8 *memory5200;		// 6502 memory
extern uint8 *vid;
extern uint8 *snd;				// pokey output buffer
//extern int8 *voiceBuffer;		// voice/sample output buffer
extern int running;				// System executing?
extern int videomode;			// NTSC or PAL?
extern uint16 snd_buf_size;		// size of sound buffer (735 for NTSC)
extern char errormsg[256];
extern const char *emu_version;

extern int framesdrawn;             // from 5200gfx.c

extern unsigned long calc_crc32(unsigned char *buf, int buflen);

int LoadState(char *filename);
int SaveState(char *filename);

// Routines which the world should see
// Everything else is internal to the core code, etc
// These functions return 0 if successful, -1 otherwise

int Jum52_Initialise(void); // Must be called at load time to initialise emulator
int Jum52_LoadROM(char *);  // Load a given ROM
int Jum52_Emulate(void);    // Execute
int Jum52_Reset(void);		// Reset current ROM

//extern void _cdecl DebugPrint(const char *format, ...);
extern void DebugPrint(const char *format, ...);

#endif
