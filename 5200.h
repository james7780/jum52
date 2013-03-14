// define 5200 hardware regs

// zero page locations
#define		sIRQEN			0x00           // shadow for IRQEN
#define     RTC_HI			0x01           // real time clock hi byte
#define     RTC_LO			0x02           // real time clock lo byte
#define     CRIT_CODE_FLAG	0x03           // critical code flag
#define     ATTRACT_TIMER	0x04           // attract mode timer/flag
#define     sDLISTL			0x05           //Display list lo shadow
#define     sDLISTH			0x06           //Display list hi shadow
#define     sDMACTL			0x07           // DMA ctrl shadow
#define     sCOLPM0			0x08          //Player/missile 0 color shadow
#define     sCOLPM1			0x09          //Player/missile 1 color shadow
#define     sCOLPM2			0x0A          //Player/missile 2 color shadow
#define     sCOLPM3			0x0B          //Player/missile 3 color shadow
#define     sCOLOR0			0x0C           //Color 0 shadow
#define     sCOLOR1			0x0D           //Color 1 shadow
#define     sCOLOR2			0x0E           //Color 2 shadow
#define     sCOLOR3			0x0F            //Color 3 shadow
#define     sCOLBK			0x10
#define     sPOT0			0x11           // pot 0 shadow
#define     sPOT1			0x12           // pot 1 shadow
#define     sPOT2			0x13
#define     sPOT3			0x14
#define     sPOT4			0x15
#define     sPOT5			0x16
#define     sPOT6			0x17
#define     sPOT7			0x18
// page two vectors
#define     ImmIRQVec		0x200         // immediate IRQ vector
#define     ImmVBIVec		0x202         // immediate VBI vector
#define     DfrVBIVec		0x204         // deferred VBI vector
#define     DLIVec			0x206         // DLI vector
#define     KeyIRQVec		0x208         // Keyboard IRQ vector
#define     KeyContVec		0x20A         // keypad continuation vector
#define     BREAKVec		0x20C         // BREAK key IRQ vector
#define     BRKIRQVec		0x20E         // BRK instr IRQ vector
#define     SerInRdyVec		0x210         // serial
#define     SerOutNeedVec	0x212         // serial
#define     SerOutFinVec	0x214         // serial
#define     POKEYT1Vec		0x216         // POKEY timer 1 IRQ
#define     POKEYT2Vec		0x218         // POKEY timer 2 IRQ
#define     POKEYT3Vec		0x21A         // POKEY timer 3 IRQ
// cart stuff
#define     PAL_CART		0xBFE7        // cart is PAL compat if == 2
#define     CARTSTARTVec	0xBFFE
// gtia
#define     HPOSP0			0xC000           //Horizontal position player 0
#define     HPOSP1			0xC001           // player 1 hpos
#define     HPOSP2			0xC002           // player 2 hpos
#define     HPOSP3			0xC003           // player 3 hpos
#define     HPOSM0			0xC004           // missile 0 hpos
#define     HPOSM1			0xC005           // missile 1 hpos
#define     HPOSM2			0xC006           // missile 2 hpos
#define     HPOSM3			0xC007           // missile 3 hpos
#define     SIZEP0			0xC008           // player 0 horiz size
#define     SIZEP1			0xC009           // player 1 horiz size
#define     SIZEP2			0xC00A           // player 2 horiz size
#define     SIZEP3			0xC00B           // player 3 horiz size
#define     SIZEM			0xC00C           // ???
#define     GRAFP0			0xC00D           //
#define     GRAFP1			0xC00E           //
#define     GRAFP2			0xC00F           //
#define     GRAFP3			0xC010           //
#define     GRAFM			0xC011           //
#define     COLPM0			0xC012           // pm colours (output only)
#define     COLPM1			0xC013           //
#define     TRIG0			0xC010           //
#define     TRIG1			0xC011           //
#define     TRIG2			0xC012           // triggers (input only)
#define     TRIG3			0xC013           //
#define     COLPM2			0xC014           //
#define     COLPM3			0xC015           //
#define     COLPF0			0xC016           // playfield colours
#define     COLPF1			0xC017           //
#define     COLPF2			0xC018           //
#define     COLPF3			0xC019           //
#define     COLBK			0xC01A           // aka COLPM5
#define     PRIOR			0xC01B           //
#define     VDELAY			0xC01C           //
#define     GRACTL			0xC01D           //Graphics control
#define		CONSOL			0xC01F
// antic
#define     DMACTL			0xD400           // DMA control
#define     CHACTL			0xD401           //Character control
#define     DLISTL			0xD402           //Display list lo
#define     DLISTH			0xD403           //Display list hi
#define     HSCROL			0xD404           // hscroll
#define     VSCROL			0xD405           // vscroll
#define     PMBASE			0xD407           //PM base address
#define     CHBASE			0xD409           //Character set base
#define     WSYNC			0xD40A           // wsync
#define     VCOUNT			0xD40B           // Vcount
#define     PENH			0xD40C		  // Pen horiz
#define     PENV			0xD40D           // Pen vert
#define     NMIEN			0xD40E           //NMI Enable (write)
#define     NMIRES			0xD40F		  //NMI Reset / Status (write)
#define     NMIST			0xD40F		  //NMI Reset / Status (read)
// pokey regs (write/read)
#define	AUDF1			0xE800           // POKEY reg
#define	AUDC1			0xE801           // POKEY reg
#define	AUDF2			0xE802           // POKEY reg
#define	AUDC2			0xE803           // POKEY reg
#define	AUDF3			0xE804           // POKEY reg
#define	AUDC3			0xE805           // POKEY reg
#define	AUDF4			0xE806           // POKEY reg
#define	AUDC4			0xE807           // POKEY reg
#define	AUDCTL			0xE808          // audio control reg (write)
#define	ALLPOT			0xE808          // audio control reg (read)
#define	STIMER			0xE809			// write
#define	KBCODE			0xE809			// read
#define	SKREST			0xE80A			// write
#define	RANDOM			0xE80A			// read
#define	POTGO			0xE80B			//
#define	SEROUT			0xE80D			// write
#define	SERIN			0xE80D			// read
#define	IRQEN			0xE80E			// write
#define	IRQST			0xE80E			// read
#define	SKCTL			0xE80F			// write
#define	SKSTAT			0xE80F			// read

// reset vector
#define	RESETVec		0xFFFC

// NMI vector
#define NMIVec			0xFFFA

// NTSC / PAL
#define NTSC	0x0F
#define PAL		0x01

typedef struct {
	unsigned char	nmien;
    unsigned char	nmist;
} ANTICTYPE;

extern ANTICTYPE antic;

// In 5200emu.c
void do_keys(void);
uint8 get6502memory(uint16 addr);
void put6502memory(uint16 addr, uint8 byte);
void initGTIA(void);
uint8 GTIAread(uint16 addr);
void GTIAwrite(uint16 addr, uint8 byte);
uint8 ANTICread(uint16 addr);
void ANTICwrite(uint16 addr, uint8 byte);
uint8 IOEXPread(uint16 addr);
void IOEXPwrite(uint16 addr, uint8 byte);
uint8 POKEYread(uint16 addr);
void POKEYwrite(uint16 addr, uint8 byte);
int loadCART(char *cartname);

// In 5200gfx.c
int init_gfx(void);
void reset_gfx(void);
void draw_horizontal_line(int h, int v, int w, int c);
void my_put_pixel(int x, int y, int c);
char my_get_pixel(int x, int y);
void initANTIC(void);
void updateANTIC(void);
int pf_line_render(void);
void pm_line_render(int line);
void clear_collision_regs(void);
void display_charset(void);

extern int vcount, tickstoHSYNC, tickstilldraw;

extern uint8 M2PF[4];				// collision regs
extern uint8 P2PF[4];
extern uint8 M2PL[4];
extern uint8 P2PL[4];

extern uint8 irqen, irqst;