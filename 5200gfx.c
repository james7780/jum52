//
//        Jum's A5200 Emulator
//
//  Copyright James Higgs 1999-2016

// TODO: fix some timing problems
// TODO: optimise PF drawing (and PM drawing)

#include "global.h"
#include "osdepend.h"

#include <stdio.h>
//#include <stdlib.h>

#include "5200.h"
#include "6502.h"
#include "colours.h"
#include "pokey.h"

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

#ifdef WIN32
#define INLINE _inline
#else
#define INLINE inline
#endif

#define P_DMA	0x40;
#define M_DMA	0x20;
#define CYCLESPERLINE	120 //114

#define KEYS_LINE		245
#define VBI_LINE		248

// global variables
ANTICTYPE antic = { 0, 0 };			// ANTIC NMI stuff
uint8 irqen = 0, irqst = 0;	    // IRQ registers
uint8 M2PF[4];				// collision regs
uint8 P2PF[4];
uint8 M2PL[4];
uint8 P2PL[4];
int vcount = 0;
int tickstoHSYNC = 0;
int tickstilldraw = 0;
uint16 dladdr = 0;
uint16 scanaddr = 0;
int scanline = 0;				// only used locally
int finished = 0;				// only used locally
static int pfLineCount = 0;		// only used locally
int dli = 0;						// only used locally
int next_dli;
int current_mode = 0;		// only used locally   // running current mode
int mode_y = 0;					// only used locally   // y-offset in that mode
int next_mode_line = 0;
int bytes_per_line = 0;	// only used locally
int framesdrawn = 0;
int stolencycles = 0;   // no of cycles stolen in this horiz line

// JH - for PS2
#ifdef _EE
uint8 vidbuffer[VID_WIDTH * VID_HEIGHT] __attribute__((aligned(16)));
#else
uint8 vidbuffer[VID_WIDTH * VID_HEIGHT];
#endif
uint8 *vid = NULL;

#define GET_LINE_BASE(v) (vid + (v * VID_WIDTH))

// Remap Allegro functions to new drawing code. To be removed.
#define getpixel(dst, x, y) my_get_pixel(x, y)
#define putpixel(dst, x, y, c) my_put_pixel(x, y, c)
#define hline(dst, h, v, w, c) draw_horizontal_line(h, v, w, c)

// lookup for lines/ antic mode
static const int modelines[16] = {
//0  1  2  3   4  5   6  7   8  9  A  B  C  D  E  F (mode)
	1, 1, 8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1
};

// lookup for modes/pfw data req.
static const int modebytes[3][4] = {
	{ 0, 8, 10, 12 },
	{ 0, 16, 20, 24 },
	{ 0, 32, 40, 48 }
};

// lookup for playfield width starting clock
static const int pfw_start[4] = { 0, 32, 0, -32 };

// lookup for hscroll data requirements
static const int hscroll_data_adjust[4] = { 0, 2, 3, 3 };

// PM horizontal size table
static const int pm_size_table[4] = { 1, 2, 1, 4 };

// PM / PF priority table
// 1 means PM infront of PF
// 0 means PM behind PF
// 9 means conflicting PRIOR setting
static const uint8 priority[5][16] = {
	{ 1, 1, 1, 1, 0, 9, 9, 9, 0, 9, 9, 9, 0, 9, 9, 9 },
	{ 1, 1, 1, 1, 0, 9, 9, 9, 0, 9, 9, 9, 0, 9, 9, 9 },
	{ 1, 1, 0, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 9, 9, 9 },
	{ 1, 1, 0, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 9, 9, 9 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static uint8 pmp[4];
// collision buffer stuff
static uint8 collbuff[VID_WIDTH * VID_HEIGHT];
static char *pcb = NULL;                  // pointer to collision buffer

// this is global for use in pf_render and PM_putpixel
static uint8 pfcol[5];
static uint8 pcol[4];

static uint8 *ppix8 = NULL;			// pntr to gfx buffer for fast gfx

static int vscroll = 0;				// vscroll status
static int hscroll = 0;				// hscroll status
static int scrolltrig = 0;		// start/end vscroll
static int vscrollStartLine;	// vscroll start line 

// New GFX code from Richard
static int gfxInited = 0;

int init_gfx(void)
{
	if (gfxInited)
	{
		sprintf(errormsg, "Double initialisation of GFX code!");
		return -1;
	}

	// JH - malloc of vid replaced here
	vid = vidbuffer;

	// We're allocated now
	gfxInited = 1;
	reset_gfx();

	HostPrepareForPaletteSet();

	// Initialise the palette
	for (int i = 0; i < 256; i++)
	{
		HostSetPaletteEntry((uint8)i,
												(uint8)((colourtable[i] >> 16) & 0xFF),
												(uint8)((colourtable[i] >> 8) & 0xFF),
												(uint8)(colourtable[i] & 0xFF));
	}

	// Force palette to take effect
	HostRefreshPalette();

	// All done
	return 0;
}

void reset_gfx(void)
{
	if (gfxInited)
	{
		// Clear buffer
		memset(vid, 0, VID_WIDTH * VID_HEIGHT);

		// Initialise video mode
		// options.videomode = NTSC;
	}
	else
	{
		init_gfx();
	}
}

void draw_horizontal_line(int h, int v, int w, int c)
{
	uint8 *dst = vid + (h + (v * VID_WIDTH));
	int i;

	for (i = 0; i <= w; i++)
		*dst++ = c;
}

void my_put_pixel(int x, int y, int c)
{
	uint8 *dst = (vid + x + (y * VID_WIDTH));
	*dst = c;
}

char my_get_pixel(int x, int y)
{
	uint8 *dst = (vid + x + (y * VID_WIDTH));
	return *dst;
}


// initialise ANTIC parameters
void initANTIC(void)
{
	clear_collision_regs();
	//tickstoVBI = 29829;					// not used
	tickstoHSYNC = CYCLESPERLINE;				// should be 114
	tickstilldraw = 9999999;
	vcount = -2;
	scanline = 0;
	next_dli = 300;
	vscroll = 0;
	hscroll = 0;
	scrolltrig = 0;
	vscrollStartLine = 0;
	next_mode_line = 0;
	bytes_per_line = 0;
	framesdrawn = 0;

	// init IRQ's
	irqst = 0xFF;                // init to no IRQ state
	irqen = 0x0;                // no IRQ's enabled
	antic.nmien = 0;
	antic.nmist = 0;

}

// helper function to return 16-bit uint16 addr
// *** OPTMISE - change to MACRO !!! ***
static uint16 getaddr(uint16 memaddr) {
	return memory5200[memaddr] + (memory5200[memaddr + 1] << 8);
}

// THIS FUNCTION CALLED EVERY HBL
// update ANTIC and check if time for VBI
void updateANTIC(void)
{
	vcount++;

	// DEBUG
	//fprintf(stderr, "vcount=%d\n", vcount);

	// Reset VBI status bit on line 39
	if (39 == vcount)
		{
		// reset VBI NMI interrupt
		antic.nmist &= 0xBF;
		memory5200[NMIST] = antic.nmist;
		}

	//// check for key IRQ just before VBI
	//// (otherwise it gets lost in VBI)
	//// (do_keys() commented out in code below)
	//if(vcount == KEYS_LINE || vcount == 120) {
	//		do_keys();				// get kbcode and set interrupt
	//}	

	// NB: 5200 will do a keyboard IRQ every 32 scanlines if a key is held down
	//     (and kbd IRQ is enabled)
	if (32 == vcount || 64 == vcount || 96 == vcount || 128 == vcount || 160 == vcount || 192 == vcount)
		{
		do_keys();				// get kbcode and trigger interrupt if neccessary
		}

	// do actual drawing area of screen
	if (vcount <= VBI_LINE) {
		// do DLI NMI if we are on the last line of a DL instr
		// with a DLI enabled
		// NB: this is sensitive!!!
		if (vcount == next_dli) {
			antic.nmist |= 0x80;
			memory5200[NMIST] |= 0x80;
			if (antic.nmien & 0x80) {
				nmi6502();
			}
		}

		// SCHEDULE DRAW SCANLINE for 40 CTIA cycles time
		// 20 CPU cycles
		tickstilldraw = 20;

		// do VBI if we are at line 248
		if (vcount == VBI_LINE) {
			// update sound
			//Pokey_process(snd, SND_BUF_SIZE);
			HostProcessSoundBuffer();

			// set off VBI by setting VBI status bit
			// and calling NMI (if enabled)
			antic.nmist = 0x40;			// HACK
			memory5200[NMIST] = 0x40;		// HACK

			// do VBI NMI if VBI enabled in NMIEN
			// NOTE: to get diagnostic cart working,
			// enable VBI NMI after x cycles
			if (antic.nmien & 0x40) {
				nmi6502();
			}

			// Do the video copy. Speed throttle goes here
			// Or in the HostProcessSoundBuffer routine.
			HostBlitVideo();
			// clear collision buffer
			memset(collbuff, 0, VID_WIDTH * VID_HEIGHT);


			finished = FALSE;
			framesdrawn++;
			pfLineCount = 0;

			// Controller/Event processing
			HostDoEvents();

			// RFB: What's this for?
			// JH: Joystick B button used for A5200 controller
			// "side buttons", which cause a BRK interrupt.
			//if(joy[0].button[1].b)
			//{
			//	irqst &= 0x7F;	// BRK key
			//	// check irqen and do interrupt if bit 7 set
			//	if(irqen & 0x80) irq6502();
			//}

		} // end if VBI
	} // end if vcount < 249
	else {
		// just check for end of frame
		// fiddling with this might get some games working???
		if (260 == vcount) {
			vcount = -2;
			next_mode_line = 8;
			scanline = 0;
			next_dli = 300;			// disable next dli until next frame
			// reload scan counter address
			dladdr = memory5200[DLISTL] + 256 * memory5200[DLISTH];
		}
	} // end else scanline < 249


}


// Render 1 line of playfield
int pf_line_render(void)
{
	uint8 d, c, ci;
	uint16 ch_base, ch_addr;
	int i, j, k, x;
	uint8 gfxcol[4];
	uint8 pfw;

	stolencycles = 0;

	// reset dli
	dli = FALSE;

	// JH - this bitmasked now
	// for 20 chars/line modes, top 6 bits of CHBASE used
	// for 40 chars/line modes, top 7 bits of CHBASE used
	ch_base = (memory5200[CHBASE] & 0xFE) << 8;

	// draw black scanline if playfield DMA off
	// (otherwise we get crap on the screen).
	if (!(memory5200[DMACTL] & 0x20)) {
		if ((vcount > -1) && (vcount < 249))	hline(gfxdest, 0, vcount, 319, 0);
		return 0;
	}

	// draw background only if DL finished
	if ((finished == TRUE) && (vcount < 249)) {
		if (vcount > -1) hline(gfxdest, 0, vcount, 319, memory5200[COLBK]);
		return 0;
	}

	// if vcount < 0, then return
	// (otherwise we write out of gfx buffer and crash!)
	if ((vcount < 0) || (vcount >= VID_HEIGHT)) return 0;

	// load playfield colours
	pfcol[0] = memory5200[COLBK];
	pfcol[1] = memory5200[COLPF0];
	pfcol[2] = memory5200[COLPF1];
	pfcol[3] = memory5200[COLPF2];
	pfcol[4] = memory5200[COLPF3];

	gfxcol[0] = memory5200[COLPF0];
	gfxcol[1] = memory5200[COLPF1];
	gfxcol[2] = memory5200[COLPF2];
	gfxcol[3] = memory5200[COLPF3];

	// find playfield width
	pfw = memory5200[DMACTL] & 0x3;
	x = pfw_start[pfw];

	// NEW: this has been split into 2 sections:
	//			1. If line 0 of mode, do cmd
	//			2. draw a line of this mode

	// read and decode DL command
	// if we are on the first line of this mode block
	if (vcount == next_mode_line) {					// optimise
		scanaddr += bytes_per_line;
		mode_y = 0;
		uint8 cmd = memory5200[dladdr++];
		current_mode = cmd & 0x0F;
		next_mode_line += modelines[current_mode];

		// check for DLI bit set
		// this causes an interrupt on the LAST line
		// displayed by the current DL command
		if (cmd & 0x80) {
			dli = TRUE;			// dli for next mode line
			// set next line for DLI interrupt
			next_dli = vcount + modelines[current_mode] - 1;
			// check for case cmd = 0 (blank lines)
			if (current_mode == 0) {
				next_dli = vcount + ((cmd & 0x70) >> 4);
			}
			// if single-line mode, DLI will be postponed one line (???)
			if (next_dli == vcount) next_dli = vcount + 1;
			// DEBUG !!!! - remove later
			//fprintf(stderr, "dli set at line %d with cmd %X\n", scanline, cmd);
		}
		// check for Horiz Scroll bit set
		if (cmd & 0x10 && current_mode > 1)
			hscroll = 1;
		else
			hscroll = 0;

		// check for Vertical Scroll On/Off
		vscrollStartLine = 0;
		scrolltrig = 0;
		if (cmd & 0x20 && current_mode > 1) {
			if (vscroll == 0) {
				scrolltrig = 1;			// trigger vscroll start
				vscroll = 1;
				vscrollStartLine = memory5200[VSCROL] & 0x0F;
				// adjust counter to next mode line
				next_mode_line -= vscrollStartLine;
			}
		}
		else {
			if (vscroll == 1) {
				scrolltrig = 2;			// trigger vscroll end
				vscroll = 0;
				// adjust counter to next mode line
				next_mode_line -= (modelines[current_mode] - (memory5200[VSCROL] & 0x0F));
				next_mode_line++;
				// adjust scanline for next DLI
				next_dli = next_mode_line - 1;
				// if single-line mode, DLI will be postponed one line (???)
				if (next_dli == vcount)
					next_dli = vcount + 1;
			}
		}

		// setup according to mode
		switch (current_mode) {		// get mode/cmd nibble
			case 0:	// blank lines
				next_mode_line = vcount + ((cmd & 0x70) >> 4) + 1;
				break;
			case 1:	// jump or jump & wait for VBI
				if (cmd & 0x40) {	// jump & wait
					dladdr = getaddr(dladdr);
					finished = TRUE;
					// set next scanline to draw to 0
					next_mode_line = 0;
				}
				else {				// jump
					// blank line drawn below
					dladdr = getaddr(dladdr);
				}
				// update DLISTL and DLISTH
				memory5200[DLISTL] = dladdr & 0xFF;
				memory5200[DLISTH] = (dladdr & 0xFF00) >> 8;
				break;
			default:
				// check scan addr update bit
				if (cmd & 0x40) {
					// update DATA load address
					scanaddr = getaddr(dladdr);
					dladdr += 2;
				}
				break;
		} // end switch
	} // end if new mode block (DL cmd read)

	// setup hscroll for this line
	// (and adjust playfield width accordingly)
	if (hscroll) {
		// set x offset
		x += (memory5200[HSCROL] & 0x0f) << 1;
		// if normal playfield with hscroll, offset -32
		if (pfw == 0x02)	x += -32;
		// if narrow playfield with hscroll, offset -32
		if (pfw == 0x01) x += -32;
		// adjust line data requirements
		pfw = hscroll_data_adjust[pfw];
	}

	// set collision buffer pointer to this line in the collision buffer
	// NB: warning! do not remove!
	// JH 16/2/2002
	pcb = collbuff + vcount * VID_WIDTH;

	// set pixel pointer to this line in gfx buffer
	ppix8 = GET_LINE_BASE(vcount);

	// *** Draw line according to mode ***
	switch (current_mode) {		// get mode/cmd nibble
		case 0:	// blank lines
		case 1:	// jump or jump & wait for VBI
			bytes_per_line = 0;
			hline(gfxdest, 0, vcount, 319, pfcol[0]);
			break;
		case 2:	// mode 2 (1-color text)
			// 40 bytes/line (normal mode)
			// Meaning of bit 7:
			// if CHACTL & 0x4 : upside down
			// if CHACTL & 0x2 : inverse
			// if CHACTL & 0x1 : blank
			bytes_per_line = modebytes[2][pfw];
			if (mode_y == 0) stolencycles += bytes_per_line;
			// draw background
			hline(gfxdest, 0, vcount, 319, pfcol[3]);
			// fg colour is same chrominance as bg colour, but different
			// luminance (?)
			c = (pfcol[3] & 0xF0) | (pfcol[2] & 0x0F);
			// draw text line
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				// get character no.
				uint8 chr = memory5200[scanaddr + i];
				ch_addr = ch_base + ((chr & 0x7f) << 3);
				d = memory5200[ch_addr + vscrollStartLine + mode_y];
				// check if inverted (bit 7 is set)
				if (chr & 0x80) {

					if (memory5200[CHACTL] & 0x2) {       // inverse ON
						d = 0xFF - d;
					}
					else if (memory5200[CHACTL] & 0x1) {
						d = 0;
					}

					if (d & 0x80) { ppix8[k] = c;   pcb[k] = 2; }
					if (d & 0x40) { ppix8[k + 1] = c; pcb[k + 1] = 2; }
					if (d & 0x20) { ppix8[k + 2] = c; pcb[k + 2] = 2; }
					if (d & 0x10) { ppix8[k + 3] = c; pcb[k + 3] = 2; }
					if (d & 0x08) { ppix8[k + 4] = c; pcb[k + 4] = 2; }
					if (d & 0x04) { ppix8[k + 5] = c; pcb[k + 5] = 2; }
					if (d & 0x02) { ppix8[k + 6] = c; pcb[k + 6] = 2; }
					if (d & 0x01) { ppix8[k + 7] = c; pcb[k + 7] = 2; }
				}
				else {
					d = memory5200[ch_addr + vscrollStartLine + mode_y];
					if (d & 0x80) { ppix8[k] = c;   pcb[k] = 2; }
					if (d & 0x40) { ppix8[k + 1] = c; pcb[k + 1] = 2; }
					if (d & 0x20) { ppix8[k + 2] = c; pcb[k + 2] = 2; }
					if (d & 0x10) { ppix8[k + 3] = c; pcb[k + 3] = 2; }
					if (d & 0x08) { ppix8[k + 4] = c; pcb[k + 4] = 2; }
					if (d & 0x04) { ppix8[k + 5] = c; pcb[k + 5] = 2; }
					if (d & 0x02) { ppix8[k + 6] = c; pcb[k + 6] = 2; }
					if (d & 0x01) { ppix8[k + 7] = c; pcb[k + 7] = 2; }
				}
				x += 8;
			} // next i
			break;

		case 3:   // mode 3 (1-color text)
			// 8x10 custom char set
			// 40 bytes/line (normal mode)
			bytes_per_line = modebytes[2][pfw];
			if (mode_y == 0) stolencycles += bytes_per_line;
			// draw background
			hline(gfxdest, 0, vcount, 319, pfcol[0]);
			c = pfcol[2];
			// draw text line
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				// get character no.
				d = memory5200[scanaddr + i];
				ch_addr = ch_base + ((d & 0x7f) << 3);
				// check if inverted
				if (d & 0x80) {
					// inverted
					switch (mode_y) {
						case 0:
							d = 0xFF;
							break;
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case 7:
							d = 0xFF - memory5200[ch_addr + vscrollStartLine + mode_y];
							break;
						case 8:
							d = 0xFF - memory5200[ch_addr + vscrollStartLine];
							break;
						case 9:
							d = 0xFF;
							break;
					}
					if (d & 0x80) { ppix8[k] = c;   pcb[k] = 2; }
					if (d & 0x40) { ppix8[k + 1] = c; pcb[k + 1] = 2; }
					if (d & 0x20) { ppix8[k + 2] = c; pcb[k + 2] = 2; }
					if (d & 0x10) { ppix8[k + 3] = c; pcb[k + 3] = 2; }
					if (d & 0x08) { ppix8[k + 4] = c; pcb[k + 4] = 2; }
					if (d & 0x04) { ppix8[k + 5] = c; pcb[k + 5] = 2; }
					if (d & 0x02) { ppix8[k + 6] = c; pcb[k + 6] = 2; }
					if (d & 0x01) { ppix8[k + 7] = c; pcb[k + 7] = 2; }
				}
				else {
					// NOT inverted
					switch (mode_y) {
						case 0:
							d = 0;
							break;
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case 7:
							d = memory5200[ch_addr + vscrollStartLine + mode_y];
							break;
						case 8:
							d = memory5200[ch_addr + vscrollStartLine];
							break;
						case 9:
							d = 0;
							break;
					}
					if (d & 0x80) { ppix8[k] = c;   pcb[k] = 2; }
					if (d & 0x40) { ppix8[k + 1] = c; pcb[k + 1] = 2; }
					if (d & 0x20) { ppix8[k + 2] = c; pcb[k + 2] = 2; }
					if (d & 0x10) { ppix8[k + 3] = c; pcb[k + 3] = 2; }
					if (d & 0x08) { ppix8[k + 4] = c; pcb[k + 4] = 2; }
					if (d & 0x04) { ppix8[k + 5] = c; pcb[k + 5] = 2; }
					if (d & 0x02) { ppix8[k + 6] = c; pcb[k + 6] = 2; }
					if (d & 0x01) { ppix8[k + 7] = c; pcb[k + 7] = 2; }
				}
				x += 8;
			} // next i
			break;

		case 4:	// mode 4 (4-color 1/2-text)
			// 40 bytes per line, 8 scanlines
			// draw text line
			// NB: char bitmask is 7 bits (128 chars)
			// JH 13/2/2002 - bit 7 changes colour used for bit pattern '11'
			//     from PF2 to PF3
			// var ci = "colour index"
		case 5:	// mode 5 (1-color text)
			// 40 bytes per line, 16 scanlines (same as mode 4, but double-height)
			bytes_per_line = modebytes[2][pfw];
			if (mode_y == 0) stolencycles += bytes_per_line;
			// calculate character offset depending on mode
			if (current_mode == 4) j = mode_y + vscrollStartLine;
			else j = mode_y / 2 + vscrollStartLine / 2;
			// draw text line
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				// get character
				d = memory5200[scanaddr + i];
				uint8 alt_clr_bit = d & 0x80;
				d &= 0x7F;
				ch_addr = ch_base + (d << 3);
				d = memory5200[ch_addr + j];
				ci = (d >> 6) & 0x03;
				c = pfcol[ci];
				if (ci == 3) { if (alt_clr_bit) c = pfcol[4]; }
				pcb[k] = ci; pcb[k + 1] = ci;
				ppix8[k++] = c; ppix8[k++] = c;
				ci = (d >> 4) & 0x03;
				c = pfcol[ci];
				if (ci == 3) { if (alt_clr_bit) c = pfcol[4]; }
				pcb[k] = ci; pcb[k + 1] = ci;
				ppix8[k++] = c; ppix8[k++] = c;
				ci = (d >> 2) & 0x03;
				c = pfcol[ci];
				if (ci == 3) { if (alt_clr_bit) c = pfcol[4]; }
				pcb[k] = ci; pcb[k + 1] = ci;
				ppix8[k++] = c; ppix8[k++] = c;
				ci = d & 0x03;
				c = pfcol[ci];
				if (ci == 3) { if (alt_clr_bit) c = pfcol[4]; }
				pcb[k] = ci; pcb[k + 1] = ci;
				ppix8[k++] = c; ppix8[k++] = c;
				x += 8;
			} // next i
			break;
		case 6:	// mode 6 (4-color text)
			// 20 bytes per line, 8 scanlines
		case 7:	// mode 7 (4-color text)
			// 20 bytes per line, 16 scanlines (same as mode 6, but double-height)
			bytes_per_line = modebytes[1][pfw];
			if (mode_y == 0) stolencycles += bytes_per_line;
			// draw background
			hline(gfxdest, 0, vcount, 319, pfcol[0]);
			// calculate character offset depending on mode
			if (current_mode == 6) j = mode_y + vscrollStartLine;
			else j = mode_y / 2 + vscrollStartLine / 2;
			// draw text line
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				d = memory5200[scanaddr + i];
				ci = ((d >> 6) & 0x3) + 1;
				c = pfcol[ci];          // get colour
				ch_addr = ch_base + ((d & 0x3f) << 3);
				d = memory5200[ch_addr + j];
				if (d & 0x80) { ppix8[k] = c; ppix8[k + 1] = c; pcb[k] = ci; pcb[k + 1] = ci; }
				if (d & 0x40) { ppix8[k + 2] = c; ppix8[k + 3] = c; pcb[k + 2] = ci; pcb[k + 3] = ci; }
				if (d & 0x20) { ppix8[k + 4] = c; ppix8[k + 5] = c; pcb[k + 4] = ci; pcb[k + 5] = ci; }
				if (d & 0x10) { ppix8[k + 6] = c; ppix8[k + 7] = c; pcb[k + 6] = ci; pcb[k + 7] = ci; }
				if (d & 0x08) { ppix8[k + 8] = c; ppix8[k + 9] = c; pcb[k + 8] = ci; pcb[k + 9] = ci; }
				if (d & 0x04) { ppix8[k + 10] = c; ppix8[k + 11] = c; pcb[k + 10] = ci; pcb[k + 11] = ci; }
				if (d & 0x02) { ppix8[k + 12] = c; ppix8[k + 13] = c; pcb[k + 12] = ci; pcb[k + 13] = ci; }
				if (d & 0x01) { ppix8[k + 14] = c; ppix8[k + 15] = c; pcb[k + 14] = ci; pcb[k + 15] = ci; }
				x += 16;
			} // next i
			break;
		case 8:	// mode 8 (4-color blocks)
			// 10 bytes per line, 8 scanlines
			bytes_per_line = modebytes[0][pfw];
			// draw line of this mode
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				d = memory5200[scanaddr + i];
				ci = (d >> 6) & 0x03;
				c = pfcol[ci];
				for (j = 8; j; j--) { pcb[k] = ci; ppix8[k++] = c; }
				ci = (d >> 4) & 0x03;
				c = pfcol[ci];
				for (j = 8; j; j--) { pcb[k] = ci; ppix8[k++] = c; }
				ci = (d >> 2) & 0x03;
				c = pfcol[ci];
				for (j = 8; j; j--) { pcb[k] = ci; ppix8[k++] = c; }
				ci = d & 0x03;
				c = pfcol[ci];
				for (j = 8; j; j--) { pcb[k] = ci; ppix8[k++] = c; }
				x += 32;
			} // next i
			break;
		case 9:	// mode 9 (1-color 4x4 blocks)
			bytes_per_line = modebytes[0][pfw];
			// draw background
			hline(gfxdest, 0, vcount, 319, pfcol[0]);
			// draw text line
			c = pfcol[2];	// is this right?
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				d = memory5200[scanaddr + i];
				for (j = 8; j; j--) {
					if (d & 0x80) {
						pcb[k] = 2; ppix8[k++] = c;
						pcb[k] = 2; ppix8[k++] = c;
						pcb[k] = 2; ppix8[k++] = c;
						pcb[k] = 2; ppix8[k++] = c;
					}
					d = d << 1;     // shift data
				}
				x += 32;
			}
			break;
		case 0xa:	// mode a (4-color 4x4 blocks)
			// 20 bytes per line, 8 scanlines
			bytes_per_line = modebytes[1][pfw];
			// draw text line
			for (i = 0; i < bytes_per_line; i++) {
				k = x;
				d = memory5200[scanaddr + i];
				for (j = 4; j; j--) {
					ci = (d >> 6) & 0x3;
					c = pfcol[ci];
					pcb[k] = ci; ppix8[k++] = c;
					pcb[k] = ci; ppix8[k++] = c;
					pcb[k] = ci; ppix8[k++] = c;
					pcb[k] = ci; ppix8[k++] = c;
					d = d << 2;     // shift data
				}
				x += 16;
			} // next i
			break;
		case 0xb: // mode b (2-line gfx, 20 bytes/line, 2 colours)
			// same as case 0xc below, but 2 lines instead of 1
		case 0xc: // mode c (single-line gfx, 20 bytes/line, 2 colours)
			bytes_per_line = modebytes[1][pfw];
			// draw background
			hline(gfxdest, 0, vcount, 319, pfcol[0]);
			// draw this scanline
			c = pfcol[1];
			for (i = 0; i < bytes_per_line; i++) {
				d = memory5200[scanaddr + i];
				if (d & 0x80) { ppix8[x] = c; ppix8[x + 1] = c; pcb[x] = 1; pcb[x + 1] = 1; }
				if (d & 0x40) { ppix8[x + 2] = c; ppix8[x + 3] = c; pcb[x + 2] = 1; pcb[x + 3] = 1; }
				if (d & 0x20) { ppix8[x + 4] = c; ppix8[x + 5] = c; pcb[x + 4] = 1; pcb[x + 5] = 1; }
				if (d & 0x10) { ppix8[x + 6] = c; ppix8[x + 7] = c; pcb[x + 6] = 1; pcb[x + 7] = 1; }
				if (d & 0x08) { ppix8[x + 8] = c; ppix8[x + 9] = c; pcb[x + 8] = 1; pcb[x + 9] = 1; }
				if (d & 0x04) { ppix8[x + 10] = c; ppix8[x + 11] = c; pcb[x + 10] = 1; pcb[x + 11] = 1; }
				if (d & 0x02) { ppix8[x + 12] = c; ppix8[x + 13] = c; pcb[x + 12] = 1; pcb[x + 13] = 1; }
				if (d & 0x01) { ppix8[x + 14] = c; ppix8[x + 15] = c; pcb[x + 14] = 1; pcb[x + 15] = 1; }
				x += 16;
			}
			break;
		case 0xd: // mode d (2-line gfx)
			// same as mode e below, but 2 lines instead of 1
		case 0xe: // mode e (single-line gfx)
			bytes_per_line = modebytes[2][pfw];
			// draw this scanline
			for (i = 0; i < bytes_per_line; i++) {
				d = memory5200[scanaddr + i];
				ci = (d >> 6) & 0x03;
				c = pfcol[ci];
				pcb[x] = ci; pcb[x + 1] = ci;
				ppix8[x] = c; ppix8[x + 1] = c;
				ci = (d >> 4) & 0x03;
				c = pfcol[ci];
				pcb[x + 2] = ci; pcb[x + 3] = ci;
				ppix8[x + 2] = c; ppix8[x + 3] = c;
				ci = (d >> 2) & 0x03;
				c = pfcol[ci];
				pcb[x + 4] = ci; pcb[x + 5] = ci;
				ppix8[x + 4] = c; ppix8[x + 5] = c;
				ci = d & 0x03;
				c = pfcol[ci];
				pcb[x + 6] = ci; pcb[x + 7] = ci;
				ppix8[x + 6] = c; ppix8[x + 7] = c;
				x += 8;
			}
			break;
		case 0xf: // mode f (single-line gfx, 40 bytes/line, 2 colours)
			// TODO: collision write
			bytes_per_line = modebytes[2][pfw];
			// check GTIA mode
			switch (memory5200[PRIOR] & 0xC0) {
				case 0x00:	// normal (CTIA) mode
					// hue from COLPF2
					// luminance from COLPF2 if bit=0
					// luminance from COLPF1 if bit=1
					// draw background
					// set to bit=0 color
					//hline(gfxdest, 0, vcount, 319, pfcol[2]);
					hline(gfxdest, 0, vcount, 319, pfcol[0]);
					// draw this scanline
					// hue from COLPF2, luminance from COLPF1
					//c = (pfcol[2] & 0xF0) | (pfcol[1] & 0x0F);
					c = pfcol[2];
					for (i = 0; i < bytes_per_line; i++) {
						d = memory5200[scanaddr + i];
						if (d & 0x80) { ppix8[x] = c; pcb[x] = 2; }
						if (d & 0x40) { ppix8[x + 1] = c; pcb[x + 1] = 2; }
						if (d & 0x20) { ppix8[x + 2] = c; pcb[x + 2] = 2; }
						if (d & 0x10) { ppix8[x + 3] = c; pcb[x + 3] = 2; }
						if (d & 0x08) { ppix8[x + 4] = c; pcb[x + 4] = 2; }
						if (d & 0x04) { ppix8[x + 5] = c; pcb[x + 5] = 2; }
						if (d & 0x02) { ppix8[x + 6] = c; pcb[x + 6] = 2; }
						if (d & 0x01) { ppix8[x + 7] = c; pcb[x + 7] = 2; }
						x += 8;
					}
					break;
				case 0x40:	// single hue, 16 luminance mode
					// Fractalus mode
					// hue set by COLBK hue nibble
					// lum set by ((COLBK lum AND 0xE) OR pix value) (4-bit)
					// draw this scanline
					c = pfcol[0] & 0xFE;		// get hue from COLBK
					for (i = 0; i < bytes_per_line; i++) {
						d = memory5200[scanaddr + i];
						hline(gfxdest, x, vcount, x + 3, (c | (d >> 4)));
						hline(gfxdest, x + 4, vcount, x + 7, (c | (d & 0x0F)));
						x += 8;
					}
					break;
				case 0x80: // 9-colour mode
					for (i = 0; i < bytes_per_line; i++) {
						d = memory5200[scanaddr + i];
						switch (d & 0xF0) {
							case 0x00: c = memory5200[COLPM0]; break;
							case 0x10: c = memory5200[COLPM1]; break;
							case 0x20: c = memory5200[COLPM2]; break;
							case 0x30: c = memory5200[COLPM3]; break;
							case 0x40: c = pfcol[1]; break;
							case 0x50: c = pfcol[2]; break;
							case 0x60: c = pfcol[3]; break;
							case 0x70: c = pfcol[4]; break;
							case 0x80: c = pfcol[0]; break;
							case 0x90: c = pfcol[0]; break;
							case 0xA0: c = pfcol[0]; break;
							case 0xB0: c = pfcol[0]; break;
							case 0xC0: c = pfcol[1]; break;
							case 0xD0: c = pfcol[2]; break;
							case 0xE0: c = pfcol[3]; break;
							case 0xF0: c = pfcol[4]; break;
						} // end switch
						hline(gfxdest, x, vcount, x + 3, c);
						x += 4;
						switch (d & 0x0F) {
							case 0x00: c = memory5200[COLPM0]; break;
							case 0x01: c = memory5200[COLPM1]; break;
							case 0x02: c = memory5200[COLPM2]; break;
							case 0x03: c = memory5200[COLPM3]; break;
							case 0x04: c = pfcol[1]; break;
							case 0x05: c = pfcol[2]; break;
							case 0x06: c = pfcol[3]; break;
							case 0x07: c = pfcol[4]; break;
							case 0x08: c = pfcol[0]; break;
							case 0x09: c = pfcol[0]; break;
							case 0x0A: c = pfcol[0]; break;
							case 0x0B: c = pfcol[0]; break;
							case 0x0C: c = pfcol[1]; break;
							case 0x0D: c = pfcol[2]; break;
							case 0x0E: c = pfcol[3]; break;
							case 0x0F: c = pfcol[4]; break;
						} // end switch
						hline(gfxdest, x, vcount, x + 3, c);
						x += 4;
					}
					break;
				case 0xC0: // 16 hues, 1 luminance
					break;
			} // end switch GTIA mode
			break;
	}	// end switch cmd
	pfLineCount++;

	// update line of mode and check if we need next mode block
	mode_y++;

	// add stolen cycles
	stolencycles += bytes_per_line;

	return 0;
}


// JH 17/2/2002
// note: format of collision buffer "pixel":
// bits 0 to 3 hold the PF value (0 to 3)
// bits 4 to 7 hold the PL bits (bit 4 = PL0, bit5 = PL1 etc)
// *** NB: OPTIMISE plot process (eg: eliminate y arg, and possibly x)
// Player gfx putpixel with priority and collision detect
INLINE void pl_plot(int x, int y, uint8 data, int pl, int width, int c) {
	int i, j, x1;
	uint8 t1, t2, plcollval;

	// get pointer to current line in gfx buffer
	ppix8 = GET_LINE_BASE(y);

	plcollval = 0x10 << pl;

	for (j = 8; j; j--) {
		if (data & 0x80) {
			i = width;
			while (i--) {
				// check for player to PF collision
				// and player to player collision
				// (what about "5th player" ????)
				// (also, eradicate player-to-self collisions)
				x1 = x;                 // save x
				t1 = pcb[x];
				if (t1) {                // collision with PF or PL
					// check collision with PF (lower 4 bits)
					t2 = t1 & 0x0F;
					if (t2) {
						if (t2 == 1) P2PF[pl] |= 0x01;
						else if (t2 == 2) P2PF[pl] |= 0x02;
						else if (t2 == 3) P2PF[pl] |= 0x04;
						else if (t2 == 4) P2PF[pl] |= 0x08;
					}
					// check collision with other players (upper 4 bits)
					t2 = (t1 & 0xF0) >> 4;
					if (t2) {
						P2PL[pl] = t2;
					}

					// plot if priority is right
					switch (pmp[pl]) {
						case 0:        // PM behind
							break;
						case 1:        // PM in front (plot)
							ppix8[x] = c;
							ppix8[x + 1] = c;
							break;
						case 9:        // PRIOR conflict (plot black)
							ppix8[x] = 0x00;
							ppix8[x + 1] = 0x00;
							break;
					} // end switch
					x += 2;

				} // end if
				else {
					// no collision - just write pixels
					ppix8[x++] = c;
					ppix8[x++] = c;
				}

				// write PL to collision buffer
				pcb[x1] |= plcollval;

			} // wend
		}
		else x += width * 2;
		// shift data up 1 bit
		data = data << 1;
	}

}

// Missile gfx putpixel with PF collision check
INLINE void miss_plot(int x, int y, int miss, int width, int c) {
	uint8 t1, t2;
	while (width--) {
		// check if this pixel is over PF
		t1 = pcb[x];
		if (t1) {
			// check collision with PF (lower 4 bits)
			t2 = t1 & 0x0F;
			if (t2) {
				if (t2 == 1) M2PF[miss] |= 0x01;
				else if (t2 == 2) M2PF[miss] |= 0x02;
				else if (t2 == 3) M2PF[miss] |= 0x04;
				else if (t2 == 4) M2PF[miss] |= 0x08;

				// plot if priority is right
				switch (pmp[miss]) {
					case 0:        // PM behind (don't plot)
						break;
					case 1:        // PM in front (plot)
						//ppix8[x] = c;
						//ppix8[x+1] = c;
						putpixel(gfxdest, x, y, c);
						putpixel(gfxdest, x + 1, y, c);
						break;
					case 9:        // PRIOR conflict (plot black)
						//ppix8[x] = 0xFF;
						//ppix8[x+1] = 0xFF;
						putpixel(gfxdest, x, y, 0x00);       // TODO: change to black
						putpixel(gfxdest, x + 1, y, 0x00);     // TODO: change to black
						break;
				} // end switch

			}
			// check collision with players
			// (and don't plot at all)
			t2 = (t1 & 0xF0) >> 4;
			if (t2) {
				M2PL[miss] = t2;
			}


		}
		else {      // missile not over PF or PL - just plot it
			putpixel(gfxdest, x, y, c);
			putpixel(gfxdest, x + 1, y, c);
		}
		x += 2;
	}
}

// draw PM graphics for a certain scanline
void pm_line_render(int line)
{
	int i, j, n, pmbase, M_addr, P_addr[4];
	int pw[4], mw[4];						// player & missile widths
	uint8 data, mcol[4];
	uint8 mdat1[4], mdat2[4];
	uint8 cur_prior;
	int mhpos[4], phpos[4];
	int vscrollStartLinem;						// single line mode

	// skip if first 8 lines
	if (vcount < 8) goto pm_line_render_end;

	// set collision buffer pointer to this line
	// (used in pl_plot() and miss_plot()
	pcb = collbuff + vcount * VID_WIDTH;

	// JH - pmbase calculation changed here
	// (moved into block below, and bitmasked)
	// This fixes PM gfx in Frogger and Miner2049

	// NB: check for double-line mode first!
	if (memory5200[DMACTL] & 0x10) {
		// single-line mode
		pmbase = (memory5200[PMBASE] & 0xF8) << 8;
		M_addr = pmbase + 768;
		P_addr[0] = pmbase + 1024;
		P_addr[1] = pmbase + 1280;
		P_addr[2] = pmbase + 1536;
		P_addr[3] = pmbase + 1792;
		vscrollStartLinem = 1;
	}
	else {
		// double line mode !    	
		pmbase = (memory5200[PMBASE] & 0xFC) << 8;
		M_addr = pmbase + 384;
		P_addr[0] = pmbase + 512;
		P_addr[1] = pmbase + 640;
		P_addr[2] = pmbase + 768;
		P_addr[3] = pmbase + 896;
		vscrollStartLinem = 0;
	}

	// VDELAY code would go here if neccessary

	// sort out missile colours
	// ("5th player")
	if (memory5200[PRIOR] & 0x10) {		// right?
		mcol[0] = memory5200[COLPF3];
		mcol[1] = mcol[0];
		mcol[2] = mcol[0];
		mcol[3] = mcol[0];
	}
	else {
		mcol[0] = memory5200[COLPM0];
		mcol[1] = memory5200[COLPM1];
		mcol[2] = memory5200[COLPM2];
		mcol[3] = memory5200[COLPM3];
	}

	// sort out player colours
	pcol[0] = memory5200[COLPM0];
	pcol[1] = memory5200[COLPM1];
	pcol[2] = memory5200[COLPM2];
	pcol[3] = memory5200[COLPM3];

	// set x offset for narrow/normal/wide screen modes
	//i = pm_mode_offset[memory5200[DMACTL] & 0x03];
	i = 48;

	for (j = 0; j < 4; j++) {
		// get missile horizontal posns
		mhpos[j] = (memory5200[HPOSM0 + j] - i) * 2;
		// get player horizontal posns
		phpos[j] = (memory5200[HPOSP0 + j] - i) * 2;
	}

	// get missile widths
	data = memory5200[SIZEM];
	mw[0] = pm_size_table[(data & 0x03)];
	mw[1] = pm_size_table[(data & 0x0c) >> 2];
	mw[2] = pm_size_table[(data & 0x30) >> 4];
	mw[3] = pm_size_table[(data & 0xc0) >> 6];

	// get player widths
	pw[0] = pm_size_table[memory5200[SIZEP0] & 0x03];
	pw[1] = pm_size_table[memory5200[SIZEP1] & 0x03];
	pw[2] = pm_size_table[memory5200[SIZEP2] & 0x03];
	pw[3] = pm_size_table[memory5200[SIZEP3] & 0x03];

	// get PM priority (speeds up checks in pm_plot() )
	cur_prior = memory5200[PRIOR] & 0x0F;
	pmp[0] = priority[0][cur_prior];
	pmp[1] = priority[1][cur_prior];
	pmp[2] = priority[2][cur_prior];
	pmp[3] = priority[3][cur_prior];

	// 1. Render players, checking for collision with PF and other PL
	// 2. Render missiles, checking for collision with PF and other PL
	//    * render missiles "behind" players

	//fprintf(stderr, "pm_line_render line %d\n", line);

	// Render players (NB: optimise!)
	// check if players data access enabled in DMACTL
	if (memory5200[DMACTL] & 0x08) {
		if (memory5200[GRACTL] & 0x02) {          // player DMA on
			stolencycles += 4;
			for (n = 3; n > -1; n--) {
				if ((phpos[n] > -16) && (phpos[n] < 476)) {      // prevent wraparound
					if (vscrollStartLinem) data = memory5200[P_addr[n] + line];
					else data = memory5200[P_addr[n] + line / 2];
					// update player data               // optimise out?
					memory5200[GRAFP0 + n] = data;
					if (data) {                          // optimisation
						// draw player
						pl_plot(phpos[n], line, data, n, pw[n], pcol[n]);
					}
				} // end if phpos
			} // next player
		}
	}
	else {      // no player DMA
		// note: does GRACTL bit 1 still have to be on?
		// apparently not (see KABOOM!)
		for (n = 3; n > -1; n--) {
			if ((phpos[n] > -16) && (phpos[n] < 476)) {      // prevent wraparound
				data = memory5200[GRAFP0 + n];           // no player DMA
				if (data) {                          // optimisation
					// draw player
					pl_plot(phpos[n], line, data, n, pw[n], pcol[n]);
				} // end if data
			} // end if phpos
		} // next player
	}

	// Render missiles (NB: optimise!)
	// check if missiles data access enabled in DMACTL
	if (memory5200[DMACTL] & 0x04) {
		if (memory5200[GRACTL] & 0x01) {          // missile DMA on
			stolencycles += 1;
			if (vscrollStartLinem) data = memory5200[M_addr + line];
			else data = memory5200[M_addr + line / 2];
			// update missile data              // optimise out?
			memory5200[GRAFM] = data;
			// optimise - only draw if there's something to draw
			if (data) {
				// get missile data into useable form
				mdat1[0] = data & 0x02;
				mdat2[0] = data & 0x01;
				mdat1[1] = data & 0x08;
				mdat2[1] = data & 0x04;
				mdat1[2] = data & 0x20;
				mdat2[2] = data & 0x10;
				mdat1[3] = data & 0x80;
				mdat2[3] = data & 0x40;
				// draw missiles (only if onscreen)
				for (n = 0; n < 4; n++) {
					if (mhpos[n] > 0) {            // JH 1/2/2002
						if (mdat1[n]) miss_plot(mhpos[n], line, n, mw[n], mcol[n]);
						if (mdat2[n]) miss_plot(mhpos[n] + mw[n] * 2, line, n, mw[n], mcol[n]);
					}
				} // end if data
			} // next missile
		}
		else {                                      // no missile DMA
			data = memory5200[GRAFM];
			if (data) {
				mdat1[0] = data & 0x02;
				mdat2[0] = data & 0x01;
				mdat1[1] = data & 0x08;
				mdat2[1] = data & 0x04;
				mdat1[2] = data & 0x20;
				mdat2[2] = data & 0x10;
				mdat1[3] = data & 0x80;
				mdat2[3] = data & 0x40;
				// NB: optimise with rects or vline's
				for (n = 0; n < 4; n++) {
					if (mhpos[n] > 0) {            // JH 1/2/2002
						if (mdat1[n]) miss_plot(mhpos[n], line, n, mw[n], mcol[n]);
						if (mdat2[n]) miss_plot(mhpos[n] + mw[n] * 2, line, n, mw[n], mcol[n]);
					}
				} // next missile
			} // end if data
		} // end if missile DMA
	} // end if missile data acces enabled


pm_line_render_end:

	// fix player-2-player collision regs
	// prevents player colliding with itself
	P2PL[0] &= 0x0E;
	P2PL[1] &= 0x0D;
	P2PL[2] &= 0x0B;
	P2PL[3] &= 0x07;
	// 2. fill in collisions missed due to reverse draw order
	if (P2PL[0] & 0x8) P2PL[3] |= 0x1;
	if (P2PL[0] & 0x4) P2PL[2] |= 0x1;
	if (P2PL[0] & 0x2) P2PL[1] |= 0x1;
	if (P2PL[1] & 0x8) P2PL[3] |= 0x2;
	if (P2PL[1] & 0x4) P2PL[2] |= 0x2;
	if (P2PL[2] & 0x8) P2PL[3] |= 0x4;

}

// clear collision registers
//	(MnPF, PnPF, MnPL, PnPL)
void clear_collision_regs(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		M2PF[i] = 0;
		P2PF[i] = 0;
		M2PL[i] = 0;
		P2PL[i] = 0;
	}
}

/*
// display character set (used by debugger)
void display_charset(void)
{
int i, j, k, x, y;
uint16 chbase;
uint8 d, c;

chbase = memory5200[CHBASE] << 8;
//clear(buffer);
//textprintf(buffer, font, 0, 0, 15, "Character set at 0x%4X", chbase);
c = 15;
for(i=0; i < 16; i++) {
for(j=0; j < 16; j++) {
for(k=0; k<8; k++) {
d = memory5200[chbase++];
x = j * 8; y = i * 8 + k;
if(d & 0x80) putpixel(buffer, x, y, c);
if(d & 0x40) putpixel(buffer, x+1, y, c);
if(d & 0x20) putpixel(buffer, x+2, y, c);
if(d & 0x10) putpixel(buffer, x+3, y, c);
if(d & 0x08) putpixel(buffer, x+4, y, c);
if(d & 0x04) putpixel(buffer, x+5, y, c);
if(d & 0x02) putpixel(buffer, x+6, y, c);
if(d & 0x01) putpixel(buffer, x+7, y, c);
}
}
}
//blit(buffer, screen, 0, 0, 320, 240, 320, 240);
//save_pcx("5200chrs.pcx", buffer, desktop_palette);
}
*/