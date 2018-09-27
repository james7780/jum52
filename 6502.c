// Most of this is by Neil Bradley circa 1998?
// The hacked parts are by James.

#include "global.h"
#include "osdepend.h"

#include <stdio.h>
#include "6502.h"
#include "5200.h"

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

//extern void _cdecl DebugPrint(const char *format, ...);
extern void DebugPrint(const char *format, ...);

//#define DEBUG
// CPU cycles between HBL's
// should be 114, but we need to subtract cycles for ANTIC DMA etc
//#define TICKSTOHBL 114        // NTSC
#define TICKSTOHBL	136

/* flags = NVRBDIZC */
uint8 a_reg,x_reg,y_reg,flag_reg,s_reg;
uint16 pc_reg = 0;

/* Address mask. Atari Asteroids/Deluxe use 0x7fff -
 * but use 0xffff for full 16 bit decode
 */
static const uint16 addrmask = 0xFFFF;

/* internal registers */
static uint8 opcode = 0;
static int clockticks6502 = 0;
static int totalticks;
static int timerTicks;
int running;
extern int stolencycles;

/* help variables */
static uint16 savepc;
static uint8 value;
static int sum;
static int saveflags;
int nmi_busy, irq_busy, irq_pending;

/* arrays */
void (*adrmode[256])();
void (*instruction[256])();
static int ticks[256];
static uint16 help;

/* Adressing modes */
/* Implied */
static void implied6502()
{
}

/* #Immediate */
static void immediate6502()
{
      savepc=PC++;
}

/* ABS */
static void abs6502()
{
      savepc = memory5200[PC] + (memory5200[PC + 1] << 8);
      PC++;
      PC++;
}

/* Branch */
static void relative6502()
{
      savepc = memory5200[PC++];
      if (savepc & 0x80) savepc -= 0x100;
      if ((savepc>>8) != (PC>>8))
              clockticks6502++;
}

/* (ABS) */
static void indirect6502()
{
      help = memory5200[PC] + (memory5200[PC + 1] << 8);
      savepc = memory5200[help] + (memory5200[help + 1] << 8);
      PC++;
      PC++;
}

/* ABS,X */
static void absx6502()
{
      savepc = memory5200[PC] + (memory5200[PC + 1] << 8);
      PC++;
      PC++;
      if (ticks[opcode]==4)
              if ((savepc>>8) != ((savepc+X)>>8))
                      clockticks6502++;
      savepc += X;
}

/* ABS,Y */
static void absy6502()
{
      savepc = memory5200[PC] + (memory5200[PC + 1] << 8);
      PC++;
      PC++;

      if (ticks[opcode]==4)
              if ((savepc>>8) != ((savepc+Y)>>8))
                      clockticks6502++;
      savepc += Y;
}

/* ZP */
static void zp6502()
{
      savepc=memory5200[PC++];
}

/* ZP,X */
static void zpx6502()
{
      savepc=memory5200[PC++]+X;
      savepc &= 0x00ff;
}

/* ZP,Y */
static void zpy6502()
{
      savepc=memory5200[PC++]+Y;
      savepc &= 0x00ff;
}

/* (ZP,X) */
static void indx6502()
{
      value = memory5200[PC++]+X;
      savepc = memory5200[value] + (memory5200[value+1] << 8);
}

/* (ZP),Y */
static void indy6502()
{
      value = memory5200[PC++];
      savepc = memory5200[value] + (memory5200[value+1] << 8);
      if (ticks[opcode]==5)
              if ((savepc>>8) != ((savepc+Y)>>8))
                      clockticks6502++;
      savepc += Y;
}

/* (ABS,X) */
static void indabsx6502()
{
      help = memory5200[PC] + (memory5200[PC + 1] << 8) + X;
      savepc = memory5200[help] + (memory5200[help + 1] << 8);
}

/* (ZP) */
static void indzp6502()
{
      value = memory5200[PC++];
      savepc = memory5200[value] + (memory5200[value + 1] << 8);
}

/* Instructions */
static void adc6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      // we have to use get6502memory() 'cos some dweeb game coder
      // might have adc'd with some register, with no forethought
      // for poor emu programmers
      value = get6502memory(savepc);
      saveflags = (P & C_FLAG);

      if (P & D_FLAG)
				{
				// Decimal (BCD) mode
				// This code from Altirra (JH 2016-06-11)
				uint32 lowResult, highResult;
				uint8 flags;
				lowResult = (A & 0xF) + (value & 0xF) + saveflags;
				if (lowResult >= 10)
					lowResult += 6;
				if (lowResult >= 0x20)
					lowResult -= 0x10;
				highResult = (A & 0xf0) + (value & 0xf0) + lowResult;
				flags = P & ~(C_FLAG | N_FLAG | Z_FLAG | V_FLAG);
				flags += (((highResult ^ A) & ~(value ^ A)) >> 1) & V_FLAG;
				flags += (highResult & 0x80);							// update N flag
				if (highResult >= 0xA0)
					highResult += 0x60;
				if (highResult >= 0x100)
					flags += C_FLAG;
				if (!(uint8)(A + value + saveflags))
					flags += Z_FLAG;
				A = (uint8)highResult;
				P = flags;
				}
			else
				{
				// Normal/hex mode
				sum= ((char) A) + ((char) value) + saveflags;
				if ((sum>0x7f) || (sum<-0x80)) P |= V_FLAG; else P &= 0xbf;
				sum= A + value + saveflags;
				if (sum>0xff) P |= C_FLAG; else P &= 0xfe;
				A=sum;
				clockticks6502++;
				if (A) P &= 0xfd; else P |= Z_FLAG;
				if (A & 0x80) P |= N_FLAG; else P &= 0x7f;
				}

/* old code
      sum= ((char) A) + ((char) value) + saveflags;
      if ((sum>0x7f) || (sum<-0x80)) P |= V_FLAG; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= C_FLAG; else P &= 0xfe;
      A=sum;
      if (P & D_FLAG)
      {
              P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
              if ((A & 0xf0)>0x90)
              {
                      A += 0x60;
                      P |= C_FLAG;
              }
      }
      else
      {
              clockticks6502++;
      }

      if (A) P &= 0xfd; else P |= Z_FLAG;
      if (A & 0x80) P |= N_FLAG; else P &= 0x7f;
*/
}

static void and6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      // we have to use get6502memory() 'cos of AND'ing with regs
      value = get6502memory(savepc);
      A &= value;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void asl6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      value = get6502memory(savepc);
      P= (P & 0xfe) | ((value >>7) & 0x01);
      value = value << 1;
      put6502memory(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void asla6502()
{
      P= (P & 0xfe) | ((A >>7) & 0x01);
      A = A << 1;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void bcc6502()
{
      if ((P & 0x01)==0)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value = memory5200[PC++];
}

static void bcs6502()
{
      if (P & 0x01)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void beq6502()
{
      if (P & 0x02)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void bit6502()
{
      adrmode[opcode]();
      //value=memory5200[savepc];
      // we have to use get6502memory() 'cos of BIT'ing regs
      value = get6502memory(savepc);
      /* non-destrucive logically And between value and the accumulator
       * and set zero flag */
      if (value & A) P &= 0xfd; else P |= 0x02;

      /* set negative and overflow flags from value */
      P = (P & 0x3f) | (value & 0xc0);
}

static void bmi6502()
{
      if (P & 0x80)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void bne6502()
{
      if ((P & 0x02)==0)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void bpl6502()
{
      if ((P & 0x80)==0)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void brk6502()
{
      PC++;
      put6502memory(0x0100+S--,(uint8)(PC>>8));
      put6502memory(0x0100+S--,(uint8)(PC & 0xff));
      put6502memory(0x0100+S--,P);
      P |= 0x14;
      PC = memory5200[0xfffe & addrmask] + (memory5200[0xffff & addrmask] << 8);
}

static void bvc6502()
{
      if ((P & 0x40)==0)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void bvs6502()
{
      if (P & 0x40)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
              value=memory5200[PC++];
}

static void clc6502()
{
      P &= 0xfe;
}

static void cld6502()
{
      P &= 0xf7;
}

static void cli6502()
{
      P &= 0xfb;
}

static void clv6502()
{
      P &= 0xbf;
}

static void cmp6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      // we have to use get6502memory() 'cos of CMP'ing with regs
      value = get6502memory(savepc);
      if (A+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=A+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void cpx6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      // we have to use get6502memory() 'cos of CPX'ing regs
      value = get6502memory(savepc);
      if (X+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=X+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void cpy6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc];
      // we have to use get6502memory() 'cos of CPY'ing regs
      value = get6502memory(savepc);
      if (Y+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=Y+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void dec6502()
{
      adrmode[opcode]();
      // we could be decrementing a register here, so
      // use get/put6502memory()
      // NB: needs checking!!!
      value = get6502memory(savepc);
      value--;
      put6502memory(savepc, value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void dex6502()
{
      X--;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void dey6502()
{
      Y--;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

static void eor6502()
{
      adrmode[opcode]();
      //A ^= gameImage[savepc];
      A ^= get6502memory(savepc);
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void inc6502()
{
      adrmode[opcode]();
      //memory5200[savepc]++;
      //value = gameImage[savepc];
      value = get6502memory(savepc);
      value++;
      put6502memory(savepc, value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void inx6502()
{
      X++;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void iny6502()
{
      Y++;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

static void jmp6502()
{
      adrmode[opcode]();
      PC=savepc;
}

static void jsr6502()
{
      PC++;
      put6502memory(0x0100+S--,(uint8)(PC >> 8));
      put6502memory(0x0100+S--,(uint8)(PC & 0xff));
      PC--;
      adrmode[opcode]();
      PC=savepc;
}

static void lda6502()
{
      adrmode[opcode]();
//    A=memory5200[savepc];
      A=get6502memory(savepc);
      // set the zero flag
      if (A) P &= 0xfd; else P |= 0x02;
      // set the negative flag
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void ldx6502()
{
      adrmode[opcode]();
//      X=memory5200[savepc];
      X=get6502memory(savepc);
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void ldy6502()
{
      adrmode[opcode]();
//      Y=memory5200[savepc];
      Y=get6502memory(savepc);
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

static void lsr6502()
{
      adrmode[opcode]();
      //value=memory5200[savepc];
      value=get6502memory(savepc);

      /* set carry flag if shifting right causes a bit to be lost */
      P= (P & 0xfe) | (value & 0x01);

      value = value >>1;
      put6502memory(savepc,value);

      /* set zero flag if value is zero */
      if (value != 0) P &= 0xfd; else P |= 0x02;

      /* set negative flag if bit 8 set??? can this happen on an LSR? */
      if ((value & 0x80) == 0x80)
         P |= 0x80;
      else
         P &= 0x7f;
}

static void lsra6502()
{
      P= (P & 0xfe) | (A & 0x01);
      A = A >>1;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void nop6502()
{
#ifdef DEBUG
	fprintf(stderr, "Opcode NOP or unknown = %2X !\n", opcode);
#endif
}

static void ora6502()
{
      adrmode[opcode]();
      //A |= memory5200[savepc];
      A |= get6502memory(savepc);
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void pha6502()
{
      memory5200[0x100+S--] = A;
}

static void php6502()
{
      memory5200[0x100+S--] = P;
}

static void pla6502()
{
      A=memory5200[++S+0x100];
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void plp6502()
{
      P=memory5200[++S+0x100] | 0x20;
}

static void rol6502()
{
      saveflags=(P & 0x01);
      adrmode[opcode]();
      //value = memory5200[savepc];
      value = get6502memory(savepc);
      P= (P & 0xfe) | ((value >>7) & 0x01);
      value = value << 1;
      value |= saveflags;
      put6502memory(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void rola6502()
{
      saveflags=(P & 0x01);
      P= (P & 0xfe) | ((A >>7) & 0x01);
      A = A << 1;
      A |= saveflags;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void ror6502()
{
      saveflags=(P & 0x01);
      adrmode[opcode]();
      //value=memory5200[savepc];
      value = get6502memory(savepc);
      P= (P & 0xfe) | (value & 0x01);
      value = value >>1;
      if (saveflags) value |= 0x80;
      put6502memory(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

static void rora6502()
{
      saveflags=(P & 0x01);
      P= (P & 0xfe) | (A & 0x01);
      A = A >>1;
      if (saveflags) A |= 0x80;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

// TODO: check if returning from NMI and unset NMI busy flag
// TODO: check if returning from IRQ and unset IRQ busy flag
// check if irq pending after nmi, and do
static void rti6502()
{
	if(nmi_busy) nmi_busy = FALSE;
    if(irq_busy) irq_busy = FALSE;
      P=memory5200[++S+0x100] | 0x20;
      PC=memory5200[++S+0x100];
      PC |= (memory5200[++S+0x100] << 8);

	// do irq if pending
	if(irq_pending) {
      	irq_pending = FALSE;
        irq6502();
	}
}

static void rts6502()
{
      PC=memory5200[++S+0x100];
      PC |= (memory5200[++S+0x100] << 8);
      PC++;
}

static void sbc6502()
{
      adrmode[opcode]();
      //value = memory5200[savepc] ^ 0xff;
      value = get6502memory(savepc) ^ 0xFF;
      saveflags = (P & C_FLAG);
			// JH 2016-06-11 new code from Altirra
			if (P & D_FLAG)
				{
				// Pole Position needs N set properly here for its passing counter
				// to stop correctly!
				uint32 carry7, result, lowResult, highCarry, highResult;
				uint8 p;

				// Flags set according to binary op
				carry7 = (A & 0x7f) + (value & 0x7f) + saveflags;
				result = carry7 + (A & 0x80) + (value & 0x80);

				// BCD
				lowResult = (A & 15) + (value & 15) + saveflags;
				highCarry = 0x10;
				if (lowResult < 0x10) {
					lowResult -= 6;
					highCarry = 0;
				}

				highResult = (A & 0xf0) + (value & 0xf0) + (lowResult & 0x0f) + highCarry;

				if (highResult < 0x100)
					highResult -= 0x60;

				// Set flags
				p = P & ~(C_FLAG | N_FLAG | Z_FLAG | V_FLAG);
				p += (result & 0x80);				// set N flag
				if (result >= 0x100) p += C_FLAG;
				if (!(result & 0xff))	p += Z_FLAG;
				p += ((result >> 2) ^ (carry7 >> 1)) & V_FLAG;
				P = p;
				A = (uint8)highResult;
				}
			else
				{
				sum = ((char) A) + ((char) value) + (saveflags << 4);
				if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
				sum = A + value + saveflags;
				if (sum>0xff) P |= 0x01; else P &= 0xfe;
				A=sum;
				clockticks6502++;
				if (A) P &= 0xfd; else P |= 0x02;
				if (A & 0x80) P |= 0x80; else P &= 0x7f;
				}

/* old code
      sum= ((char) A) + ((char) value) + (saveflags << 4);
      if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= 0x01; else P &= 0xfe;
      A=sum;
      if (P & 0x08)
      {
              A -= 0x66;  
              P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
              if ((A & 0xf0)>0x90)
              {
                      A += 0x60;
                      P |= 0x01;
              }
      }
      else
      {
              clockticks6502++;
      }
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
*/
}

static void sec6502()
{
      P |= 0x01;
}

static void sed6502()
{
      P |= 0x08;
}

static void sei6502()
{
      P |= 0x04;
}

static void sta6502()
{
      adrmode[opcode]();
      put6502memory(savepc,A);
}

static void stx6502()
{
      adrmode[opcode]();
      put6502memory(savepc,X);
}

static void sty6502()
{
      adrmode[opcode]();
      put6502memory(savepc,Y);
}

static void tax6502()
{
      X=A;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void tay6502()
{
      Y=A;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

static void tsx6502()
{
      X=S;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void txa6502()
{
      A=X;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void txs6502()
{
      S=X;
}

static void tya6502()
{
      A=Y;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void bra6502()
{
      adrmode[opcode]();
      PC += savepc;
      clockticks6502++;
}

static void dea6502()
{
      A--;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void ina6502()
{
      A++;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

static void phx6502()
{
      put6502memory(0x100+S--,X);
}

static void plx6502()
{
      X=memory5200[++S+0x100];			// OPTIMISE!
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

static void phy6502()
{
      put6502memory(0x100+S--,Y);		// OPTIMISE!
}

static void ply6502()
{
      Y=memory5200[++S+0x100];			// OPTIMISE!
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

static void stz6502()
{
      adrmode[opcode]();
      put6502memory(savepc,0);
}

static void tsb6502()
{
      adrmode[opcode]();
      //memory5200[savepc] |= A;
      value = get6502memory(savepc);
      value |= A;
      put6502memory(savepc, value);
      if (value) P &= 0xfd; else P |= 0x02;

}

static void trb6502()
{
      adrmode[opcode]();
      //memory5200[savepc] = memory5200[savepc] & (A ^ 0xff);
      value = get6502memory(savepc);
      value &= (A ^ 0xFF);
      put6502memory(savepc, value);
      if (value) P &= 0xfd; else P |= 0x02;
}

// Illegal/Unofficial/Undocumented instructions

// ASO  [unofficial - ASL then ORA with Acc]
static void aso6502()
{
	uint8 data;

#ifdef DEBUG
	fprintf(stderr, "Illegal opcode ASO!\n");
#endif
    adrmode[opcode]();
    //data = memory5200[savepc];
    data = get6502memory(savepc);
    // set carry from highest bit before left shift
    if(data & 0x80) P |= C_FLAG; else P &= 0xFE;
    A = data << 1;
    if(A & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    if(A & N_FLAG) P |= N_FLAG; else P &= 0x7F;
}

/* AXA [unofficial - original decode by R.Sterba and R.Petruzela 15.1.1998 :-)]
   AXA - this is our new imaginative name for instruction with opcode hex BB.
   AXA - Store Mem AND #$FD to Acc and X, then set stackpoint to value (Acc - 4)
   It's cool! :-)
*/
static void axa6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode AXA!\n");
#endif
    adrmode[opcode]();

    //A = memory5200[savepc] & 0xFD;
    A = get6502memory(savepc) & 0xFD;
	X = A;
    if(A & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    if(A & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	S = ((uint16) A - 4) & 0xFF;
}

/* INS [unofficial - INC Mem then SBC with Acc] */
static void ins6502()
{
	uint8 data;
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode INS !\n");
#endif
	adrmode[opcode]();
    //data = ++memory5200[savepc];
    data = get6502memory(savepc);
    data++;
    put6502memory(savepc, data);

	// set flags
    if(data & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    if(data & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	// do sbc (copied from above, without getting adrmode)
	// (inherits adrmode from this opcode?)
      value = memory5200[savepc] ^ 0xff;
      saveflags=(P & 0x01);
      sum= ((char) A) + ((char) value) + (saveflags << 4);
      if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= 0x01; else P &= 0xfe;
      A=sum;
      if (P & 0x08)
      {
              A -= 0x66;  
              P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
              if ((A & 0xf0)>0x90)
              {
                      A += 0x60;
                      P |= 0x01;
              }
      }
      else
      {
              clockticks6502++;
      }
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

// LAX - Z = N = X = A = getbyte(addr)
static void lax6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode LAX!\n");
#endif
	adrmode[opcode]();
    //A = memory5200[savepc];
    A = get6502memory(savepc);

    X = A;
    if(A & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    if(A & N_FLAG) P |= N_FLAG; else P &= 0x7F;
}

//   RLA [unofficial - ROL Mem, then AND with A]
// (check this code - it may be wrong)
static void rla6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode RLA!\n");
#endif
    adrmode[opcode]();

    //value = memory5200[savepc];
    value = get6502memory(savepc);

	if(P & C_FLAG) {
		if(value & 0x80) P |= C_FLAG ; else P &= 0xFE;
        value = (value << 1) | 1;
	    if(value & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    	if(value & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	}
	else {
		if(value & 0x80) P |= C_FLAG ; else P &= 0xFE;
        value = (value << 1);
	    if(value & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
    	if(value & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	}

	put6502memory(savepc, value);

	value = A & value;
    if(value & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
   	if(value & N_FLAG) P |= N_FLAG; else P &= 0x7F;
}

//   RRA [unofficial - ROR Mem, then ADC to Acc]
// (check this code - it may be wrong)
static void rra6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode RRA!\n");
#endif
    adrmode[opcode]();
    //value = memory5200[savepc];
    value = get6502memory(savepc);

	if(P & C_FLAG) {
		P |= (value & 1);				// set C flag from value
        value = (value >> 1) | 0x80;
	    if(value & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
   		if(value & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	}
    else {
		P |= (value & 1);
        value = (value >> 1);
	    if(value & Z_FLAG) P |= Z_FLAG; else P &= 0xFD;
   		if(value & N_FLAG) P |= N_FLAG; else P &= 0x7F;
	}

	put6502memory(savepc, value);

    // do ADC
      saveflags=(P & 0x01);
      sum= ((char) A) + ((char) value) + saveflags;
      if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= 0x01; else P &= 0xfe;
      A=sum;
      if (P & 0x08)
      {
              P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
              if ((A & 0xf0)>0x90)
              {
                      A += 0x60;
                      P |= 0x01;
              }
      }
      else
      {
              clockticks6502++;
      }
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}


// SKB - "skip byte"
static void skb6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode SKB = %2X !\n", opcode);
#endif
    PC++;
}

// SKW - "skip word"
static void skw6502()
{
#ifdef DEBUG
	fprintf(stderr, "Illegal opcode SKW = %2X !\n", opcode);
#endif
    PC += 2;
}

/* Init CPU */
void init6502()
{
      //fprintf(logfile, "Initialising 6502 instructions...\n");
      ticks[0x00]=7; instruction[0x00]=brk6502; adrmode[0x00]=implied6502;
      ticks[0x01]=6; instruction[0x01]=ora6502; adrmode[0x01]=indx6502;
      ticks[0x02]=2; instruction[0x02]=nop6502; adrmode[0x02]=implied6502;
      ticks[0x03]=2; instruction[0x03]=aso6502; adrmode[0x03]=indx6502;
      ticks[0x04]=3; instruction[0x04]=tsb6502; adrmode[0x04]=zp6502;
      ticks[0x05]=3; instruction[0x05]=ora6502; adrmode[0x05]=zp6502;
      ticks[0x06]=5; instruction[0x06]=asl6502; adrmode[0x06]=zp6502;
      ticks[0x07]=2; instruction[0x07]=aso6502; adrmode[0x07]=zp6502;
      ticks[0x08]=3; instruction[0x08]=php6502; adrmode[0x08]=implied6502;
      ticks[0x09]=3; instruction[0x09]=ora6502; adrmode[0x09]=immediate6502;
      ticks[0x0a]=2; instruction[0x0a]=asla6502; adrmode[0x0a]=implied6502;
      ticks[0x0b]=2; instruction[0x0b]=aso6502; adrmode[0x0b]=immediate6502;
      ticks[0x0c]=4; instruction[0x0c]=tsb6502; adrmode[0x0c]=abs6502;
      ticks[0x0d]=4; instruction[0x0d]=ora6502; adrmode[0x0d]=abs6502;
      ticks[0x0e]=6; instruction[0x0e]=asl6502; adrmode[0x0e]=abs6502;
      ticks[0x0f]=2; instruction[0x0f]=aso6502; adrmode[0x0f]=abs6502;
      ticks[0x10]=2; instruction[0x10]=bpl6502; adrmode[0x10]=relative6502;
      ticks[0x11]=5; instruction[0x11]=ora6502; adrmode[0x11]=indy6502;
      ticks[0x12]=3; instruction[0x12]=ora6502; adrmode[0x12]=indzp6502;
      ticks[0x13]=2; instruction[0x13]=aso6502; adrmode[0x13]=indy6502;
      ticks[0x14]=3; instruction[0x14]=trb6502; adrmode[0x14]=zp6502;
      ticks[0x15]=4; instruction[0x15]=ora6502; adrmode[0x15]=zpx6502;
      ticks[0x16]=6; instruction[0x16]=asl6502; adrmode[0x16]=zpx6502;
      ticks[0x17]=2; instruction[0x17]=aso6502; adrmode[0x17]=zpx6502;
      ticks[0x18]=2; instruction[0x18]=clc6502; adrmode[0x18]=implied6502;
      ticks[0x19]=4; instruction[0x19]=ora6502; adrmode[0x19]=absy6502;
      ticks[0x1a]=2; instruction[0x1a]=ina6502; adrmode[0x1a]=implied6502;
      ticks[0x1b]=2; instruction[0x1b]=aso6502; adrmode[0x1b]=absy6502;
      ticks[0x1c]=4; instruction[0x1c]=trb6502; adrmode[0x1c]=abs6502;
      ticks[0x1d]=4; instruction[0x1d]=ora6502; adrmode[0x1d]=absx6502;
      ticks[0x1e]=7; instruction[0x1e]=asl6502; adrmode[0x1e]=absx6502;
      ticks[0x1f]=2; instruction[0x1f]=aso6502; adrmode[0x1f]=absx6502;
      ticks[0x20]=6; instruction[0x20]=jsr6502; adrmode[0x20]=abs6502;
      ticks[0x21]=6; instruction[0x21]=and6502; adrmode[0x21]=indx6502;
      ticks[0x22]=2; instruction[0x22]=nop6502; adrmode[0x22]=implied6502;
      ticks[0x23]=2; instruction[0x23]=rla6502; adrmode[0x23]=indx6502;
      ticks[0x24]=3; instruction[0x24]=bit6502; adrmode[0x24]=zp6502;
      ticks[0x25]=3; instruction[0x25]=and6502; adrmode[0x25]=zp6502;
      ticks[0x26]=5; instruction[0x26]=rol6502; adrmode[0x26]=zp6502;
      ticks[0x27]=2; instruction[0x27]=rla6502; adrmode[0x27]=zp6502;
      ticks[0x28]=4; instruction[0x28]=plp6502; adrmode[0x28]=implied6502;
      ticks[0x29]=3; instruction[0x29]=and6502; adrmode[0x29]=immediate6502;
      ticks[0x2a]=2; instruction[0x2a]=rola6502; adrmode[0x2a]=implied6502;
      ticks[0x2b]=2; instruction[0x2b]=rla6502; adrmode[0x2b]=immediate6502;
      ticks[0x2c]=4; instruction[0x2c]=bit6502; adrmode[0x2c]=abs6502;
      ticks[0x2d]=4; instruction[0x2d]=and6502; adrmode[0x2d]=abs6502;
      ticks[0x2e]=6; instruction[0x2e]=rol6502; adrmode[0x2e]=abs6502;
      ticks[0x2f]=2; instruction[0x2f]=rla6502; adrmode[0x2f]=abs6502;
      ticks[0x30]=2; instruction[0x30]=bmi6502; adrmode[0x30]=relative6502;
      ticks[0x31]=5; instruction[0x31]=and6502; adrmode[0x31]=indy6502;
      ticks[0x32]=3; instruction[0x32]=and6502; adrmode[0x32]=indzp6502;
      ticks[0x33]=2; instruction[0x33]=rla6502; adrmode[0x33]=indy6502;
      ticks[0x34]=4; instruction[0x34]=bit6502; adrmode[0x34]=zpx6502;
      ticks[0x35]=4; instruction[0x35]=and6502; adrmode[0x35]=zpx6502;
      ticks[0x36]=6; instruction[0x36]=rol6502; adrmode[0x36]=zpx6502;
      ticks[0x37]=2; instruction[0x37]=rla6502; adrmode[0x37]=zpx6502;
      ticks[0x38]=2; instruction[0x38]=sec6502; adrmode[0x38]=implied6502;
      ticks[0x39]=4; instruction[0x39]=and6502; adrmode[0x39]=absy6502;
      ticks[0x3a]=2; instruction[0x3a]=dea6502; adrmode[0x3a]=implied6502;
      ticks[0x3b]=2; instruction[0x3b]=rla6502; adrmode[0x3b]=absy6502;
      ticks[0x3c]=4; instruction[0x3c]=bit6502; adrmode[0x3c]=absx6502;
      ticks[0x3d]=4; instruction[0x3d]=and6502; adrmode[0x3d]=absx6502;
      ticks[0x3e]=7; instruction[0x3e]=rol6502; adrmode[0x3e]=absx6502;
      ticks[0x3f]=2; instruction[0x3f]=rla6502; adrmode[0x3f]=absx6502;
      ticks[0x40]=6; instruction[0x40]=rti6502; adrmode[0x40]=implied6502;
      ticks[0x41]=6; instruction[0x41]=eor6502; adrmode[0x41]=indx6502;
      ticks[0x42]=2; instruction[0x42]=nop6502; adrmode[0x42]=implied6502;
      ticks[0x43]=2; instruction[0x43]=nop6502; adrmode[0x43]=implied6502;
      ticks[0x44]=2; instruction[0x44]=skb6502; adrmode[0x44]=implied6502;
      ticks[0x45]=3; instruction[0x45]=eor6502; adrmode[0x45]=zp6502;
      ticks[0x46]=5; instruction[0x46]=lsr6502; adrmode[0x46]=zp6502;
      ticks[0x47]=2; instruction[0x47]=nop6502; adrmode[0x47]=implied6502;
      ticks[0x48]=3; instruction[0x48]=pha6502; adrmode[0x48]=implied6502;
      ticks[0x49]=3; instruction[0x49]=eor6502; adrmode[0x49]=immediate6502;
      ticks[0x4a]=2; instruction[0x4a]=lsra6502; adrmode[0x4a]=implied6502;
      ticks[0x4b]=2; instruction[0x4b]=nop6502; adrmode[0x4b]=implied6502;
      ticks[0x4c]=3; instruction[0x4c]=jmp6502; adrmode[0x4c]=abs6502;
      ticks[0x4d]=4; instruction[0x4d]=eor6502; adrmode[0x4d]=abs6502;
      ticks[0x4e]=6; instruction[0x4e]=lsr6502; adrmode[0x4e]=abs6502;
      ticks[0x4f]=2; instruction[0x4f]=nop6502; adrmode[0x4f]=implied6502;
      ticks[0x50]=2; instruction[0x50]=bvc6502; adrmode[0x50]=relative6502;
      ticks[0x51]=5; instruction[0x51]=eor6502; adrmode[0x51]=indy6502;
      ticks[0x52]=3; instruction[0x52]=eor6502; adrmode[0x52]=indzp6502;
      ticks[0x53]=2; instruction[0x53]=nop6502; adrmode[0x53]=implied6502;
      ticks[0x54]=2; instruction[0x54]=skb6502; adrmode[0x54]=implied6502;
      ticks[0x55]=4; instruction[0x55]=eor6502; adrmode[0x55]=zpx6502;
      ticks[0x56]=6; instruction[0x56]=lsr6502; adrmode[0x56]=zpx6502;
      ticks[0x57]=2; instruction[0x57]=nop6502; adrmode[0x57]=implied6502;
      ticks[0x58]=2; instruction[0x58]=cli6502; adrmode[0x58]=implied6502;
      ticks[0x59]=4; instruction[0x59]=eor6502; adrmode[0x59]=absy6502;
      ticks[0x5a]=3; instruction[0x5a]=phy6502; adrmode[0x5a]=implied6502;
      ticks[0x5b]=2; instruction[0x5b]=nop6502; adrmode[0x5b]=implied6502;
      ticks[0x5c]=2; instruction[0x5c]=skw6502; adrmode[0x5c]=implied6502;
      ticks[0x5d]=4; instruction[0x5d]=eor6502; adrmode[0x5d]=absx6502;
      ticks[0x5e]=7; instruction[0x5e]=lsr6502; adrmode[0x5e]=absx6502;
      ticks[0x5f]=2; instruction[0x5f]=nop6502; adrmode[0x5f]=implied6502;
      ticks[0x60]=6; instruction[0x60]=rts6502; adrmode[0x60]=implied6502;
      ticks[0x61]=6; instruction[0x61]=adc6502; adrmode[0x61]=indx6502;
      ticks[0x62]=2; instruction[0x62]=nop6502; adrmode[0x62]=implied6502;
      ticks[0x63]=2; instruction[0x63]=rra6502; adrmode[0x63]=indx6502;
      ticks[0x64]=3; instruction[0x64]=stz6502; adrmode[0x64]=zp6502;
      ticks[0x65]=3; instruction[0x65]=adc6502; adrmode[0x65]=zp6502;
      ticks[0x66]=5; instruction[0x66]=ror6502; adrmode[0x66]=zp6502;
      ticks[0x67]=2; instruction[0x67]=rra6502; adrmode[0x67]=zp6502;
      ticks[0x68]=4; instruction[0x68]=pla6502; adrmode[0x68]=implied6502;
      ticks[0x69]=3; instruction[0x69]=adc6502; adrmode[0x69]=immediate6502;
      ticks[0x6a]=2; instruction[0x6a]=rora6502; adrmode[0x6a]=implied6502;
      ticks[0x6b]=2; instruction[0x6b]=nop6502; adrmode[0x6b]=implied6502;
      ticks[0x6c]=5; instruction[0x6c]=jmp6502; adrmode[0x6c]=indirect6502;
      ticks[0x6d]=4; instruction[0x6d]=adc6502; adrmode[0x6d]=abs6502;
      ticks[0x6e]=6; instruction[0x6e]=ror6502; adrmode[0x6e]=abs6502;
      ticks[0x6f]=2; instruction[0x6f]=rra6502; adrmode[0x6f]=abs6502;
      ticks[0x70]=2; instruction[0x70]=bvs6502; adrmode[0x70]=relative6502;
      ticks[0x71]=5; instruction[0x71]=adc6502; adrmode[0x71]=indy6502;
      ticks[0x72]=3; instruction[0x72]=adc6502; adrmode[0x72]=indzp6502;
      ticks[0x73]=2; instruction[0x73]=rra6502; adrmode[0x73]=indy6502;
      ticks[0x74]=4; instruction[0x74]=stz6502; adrmode[0x74]=zpx6502;
      ticks[0x75]=4; instruction[0x75]=adc6502; adrmode[0x75]=zpx6502;
      ticks[0x76]=6; instruction[0x76]=ror6502; adrmode[0x76]=zpx6502;
      ticks[0x77]=2; instruction[0x77]=rra6502; adrmode[0x77]=zpx6502;
      ticks[0x78]=2; instruction[0x78]=sei6502; adrmode[0x78]=implied6502;
      ticks[0x79]=4; instruction[0x79]=adc6502; adrmode[0x79]=absy6502;
      ticks[0x7a]=4; instruction[0x7a]=ply6502; adrmode[0x7a]=implied6502;
      ticks[0x7b]=2; instruction[0x7b]=rra6502; adrmode[0x7b]=absy6502;
      ticks[0x7c]=6; instruction[0x7c]=jmp6502; adrmode[0x7c]=indabsx6502;
      ticks[0x7d]=4; instruction[0x7d]=adc6502; adrmode[0x7d]=absx6502;
      ticks[0x7e]=7; instruction[0x7e]=ror6502; adrmode[0x7e]=absx6502;
      ticks[0x7f]=2; instruction[0x7f]=rra6502; adrmode[0x7f]=absx6502;
      ticks[0x80]=2; instruction[0x80]=bra6502; adrmode[0x80]=relative6502;
      ticks[0x81]=6; instruction[0x81]=sta6502; adrmode[0x81]=indx6502;
      ticks[0x82]=2; instruction[0x82]=skb6502; adrmode[0x82]=implied6502;
      ticks[0x83]=2; instruction[0x83]=nop6502; adrmode[0x83]=implied6502;
      ticks[0x84]=2; instruction[0x84]=sty6502; adrmode[0x84]=zp6502;
      ticks[0x85]=2; instruction[0x85]=sta6502; adrmode[0x85]=zp6502;
      ticks[0x86]=2; instruction[0x86]=stx6502; adrmode[0x86]=zp6502;
      ticks[0x87]=2; instruction[0x87]=nop6502; adrmode[0x87]=implied6502;
      ticks[0x88]=2; instruction[0x88]=dey6502; adrmode[0x88]=implied6502;
      ticks[0x89]=2; instruction[0x89]=bit6502; adrmode[0x89]=immediate6502;
      ticks[0x8a]=2; instruction[0x8a]=txa6502; adrmode[0x8a]=implied6502;
      ticks[0x8b]=2; instruction[0x8b]=nop6502; adrmode[0x8b]=implied6502;
      ticks[0x8c]=4; instruction[0x8c]=sty6502; adrmode[0x8c]=abs6502;
      ticks[0x8d]=4; instruction[0x8d]=sta6502; adrmode[0x8d]=abs6502;
      ticks[0x8e]=4; instruction[0x8e]=stx6502; adrmode[0x8e]=abs6502;
      ticks[0x8f]=2; instruction[0x8f]=nop6502; adrmode[0x8f]=implied6502;
      ticks[0x90]=2; instruction[0x90]=bcc6502; adrmode[0x90]=relative6502;
      ticks[0x91]=6; instruction[0x91]=sta6502; adrmode[0x91]=indy6502;
      ticks[0x92]=3; instruction[0x92]=sta6502; adrmode[0x92]=indzp6502;
      ticks[0x93]=2; instruction[0x93]=nop6502; adrmode[0x93]=implied6502;
      ticks[0x94]=4; instruction[0x94]=sty6502; adrmode[0x94]=zpx6502;
      ticks[0x95]=4; instruction[0x95]=sta6502; adrmode[0x95]=zpx6502;
      ticks[0x96]=4; instruction[0x96]=stx6502; adrmode[0x96]=zpy6502;
      ticks[0x97]=2; instruction[0x97]=nop6502; adrmode[0x97]=implied6502;
      ticks[0x98]=2; instruction[0x98]=tya6502; adrmode[0x98]=implied6502;
      ticks[0x99]=5; instruction[0x99]=sta6502; adrmode[0x99]=absy6502;
      ticks[0x9a]=2; instruction[0x9a]=txs6502; adrmode[0x9a]=implied6502;
      ticks[0x9b]=2; instruction[0x9b]=nop6502; adrmode[0x9b]=implied6502;
      ticks[0x9c]=4; instruction[0x9c]=stz6502; adrmode[0x9c]=abs6502;
      ticks[0x9d]=5; instruction[0x9d]=sta6502; adrmode[0x9d]=absx6502;
      ticks[0x9e]=5; instruction[0x9e]=stz6502; adrmode[0x9e]=absx6502;
      ticks[0x9f]=2; instruction[0x9f]=nop6502; adrmode[0x9f]=implied6502;
      ticks[0xa0]=3; instruction[0xa0]=ldy6502; adrmode[0xa0]=immediate6502;
      ticks[0xa1]=6; instruction[0xa1]=lda6502; adrmode[0xa1]=indx6502;
      ticks[0xa2]=3; instruction[0xa2]=ldx6502; adrmode[0xa2]=immediate6502;
      ticks[0xa3]=2; instruction[0xa3]=lax6502; adrmode[0xa3]=indx6502;
      ticks[0xa4]=3; instruction[0xa4]=ldy6502; adrmode[0xa4]=zp6502;
      ticks[0xa5]=3; instruction[0xa5]=lda6502; adrmode[0xa5]=zp6502;
      ticks[0xa6]=3; instruction[0xa6]=ldx6502; adrmode[0xa6]=zp6502;
      ticks[0xa7]=2; instruction[0xa7]=lax6502; adrmode[0xa7]=zp6502;
      ticks[0xa8]=2; instruction[0xa8]=tay6502; adrmode[0xa8]=implied6502;
      ticks[0xa9]=3; instruction[0xa9]=lda6502; adrmode[0xa9]=immediate6502;
      ticks[0xaa]=2; instruction[0xaa]=tax6502; adrmode[0xaa]=implied6502;
      ticks[0xab]=2; instruction[0xab]=nop6502; adrmode[0xab]=implied6502;
      ticks[0xac]=4; instruction[0xac]=ldy6502; adrmode[0xac]=abs6502;
      ticks[0xad]=4; instruction[0xad]=lda6502; adrmode[0xad]=abs6502;
      ticks[0xae]=4; instruction[0xae]=ldx6502; adrmode[0xae]=abs6502;
      ticks[0xaf]=2; instruction[0xaf]=lax6502; adrmode[0xaf]=abs6502;
      ticks[0xb0]=2; instruction[0xb0]=bcs6502; adrmode[0xb0]=relative6502;
      ticks[0xb1]=5; instruction[0xb1]=lda6502; adrmode[0xb1]=indy6502;
      ticks[0xb2]=3; instruction[0xb2]=lda6502; adrmode[0xb2]=indzp6502;
      ticks[0xb3]=2; instruction[0xb3]=lax6502; adrmode[0xb3]=indy6502;
      ticks[0xb4]=4; instruction[0xb4]=ldy6502; adrmode[0xb4]=zpx6502;
      ticks[0xb5]=4; instruction[0xb5]=lda6502; adrmode[0xb5]=zpx6502;
      ticks[0xb6]=4; instruction[0xb6]=ldx6502; adrmode[0xb6]=zpy6502;
      ticks[0xb7]=2; instruction[0xb7]=lax6502; adrmode[0xb7]=zpy6502;
      ticks[0xb8]=2; instruction[0xb8]=clv6502; adrmode[0xb8]=implied6502;
      ticks[0xb9]=4; instruction[0xb9]=lda6502; adrmode[0xb9]=absy6502;
      ticks[0xba]=2; instruction[0xba]=tsx6502; adrmode[0xba]=implied6502;
      ticks[0xbb]=2; instruction[0xbb]=axa6502; adrmode[0xbb]=absy6502;
      ticks[0xbc]=4; instruction[0xbc]=ldy6502; adrmode[0xbc]=absx6502;
      ticks[0xbd]=4; instruction[0xbd]=lda6502; adrmode[0xbd]=absx6502;
      ticks[0xbe]=4; instruction[0xbe]=ldx6502; adrmode[0xbe]=absy6502;
      ticks[0xbf]=2; instruction[0xbf]=lax6502; adrmode[0xbf]=absy6502;
      ticks[0xc0]=3; instruction[0xc0]=cpy6502; adrmode[0xc0]=immediate6502;
      ticks[0xc1]=6; instruction[0xc1]=cmp6502; adrmode[0xc1]=indx6502;
      ticks[0xc2]=2; instruction[0xc2]=skb6502; adrmode[0xc2]=implied6502;
      ticks[0xc3]=2; instruction[0xc3]=nop6502; adrmode[0xc3]=implied6502;
      ticks[0xc4]=3; instruction[0xc4]=cpy6502; adrmode[0xc4]=zp6502;
      ticks[0xc5]=3; instruction[0xc5]=cmp6502; adrmode[0xc5]=zp6502;
      ticks[0xc6]=5; instruction[0xc6]=dec6502; adrmode[0xc6]=zp6502;
      ticks[0xc7]=2; instruction[0xc7]=nop6502; adrmode[0xc7]=implied6502;
      ticks[0xc8]=2; instruction[0xc8]=iny6502; adrmode[0xc8]=implied6502;
      ticks[0xc9]=3; instruction[0xc9]=cmp6502; adrmode[0xc9]=immediate6502;
      ticks[0xca]=2; instruction[0xca]=dex6502; adrmode[0xca]=implied6502;
      ticks[0xcb]=2; instruction[0xcb]=nop6502; adrmode[0xcb]=implied6502;
      ticks[0xcc]=4; instruction[0xcc]=cpy6502; adrmode[0xcc]=abs6502;
      ticks[0xcd]=4; instruction[0xcd]=cmp6502; adrmode[0xcd]=abs6502;
      ticks[0xce]=6; instruction[0xce]=dec6502; adrmode[0xce]=abs6502;
      ticks[0xcf]=2; instruction[0xcf]=nop6502; adrmode[0xcf]=implied6502;
      ticks[0xd0]=2; instruction[0xd0]=bne6502; adrmode[0xd0]=relative6502;
      ticks[0xd1]=5; instruction[0xd1]=cmp6502; adrmode[0xd1]=indy6502;
      ticks[0xd2]=3; instruction[0xd2]=cmp6502; adrmode[0xd2]=indzp6502;
      ticks[0xd3]=2; instruction[0xd3]=nop6502; adrmode[0xd3]=implied6502;
      ticks[0xd4]=2; instruction[0xd4]=skb6502; adrmode[0xd4]=implied6502;
      ticks[0xd5]=4; instruction[0xd5]=cmp6502; adrmode[0xd5]=zpx6502;
      ticks[0xd6]=6; instruction[0xd6]=dec6502; adrmode[0xd6]=zpx6502;
      ticks[0xd7]=2; instruction[0xd7]=nop6502; adrmode[0xd7]=implied6502;
      ticks[0xd8]=2; instruction[0xd8]=cld6502; adrmode[0xd8]=implied6502;
      ticks[0xd9]=4; instruction[0xd9]=cmp6502; adrmode[0xd9]=absy6502;
      ticks[0xda]=3; instruction[0xda]=phx6502; adrmode[0xda]=implied6502;
      ticks[0xdb]=2; instruction[0xdb]=nop6502; adrmode[0xdb]=implied6502;
      ticks[0xdc]=2; instruction[0xdc]=skw6502; adrmode[0xdc]=implied6502;
      ticks[0xdd]=4; instruction[0xdd]=cmp6502; adrmode[0xdd]=absx6502;
      ticks[0xde]=7; instruction[0xde]=dec6502; adrmode[0xde]=absx6502;
      ticks[0xdf]=2; instruction[0xdf]=nop6502; adrmode[0xdf]=implied6502;
      ticks[0xe0]=3; instruction[0xe0]=cpx6502; adrmode[0xe0]=immediate6502;
      ticks[0xe1]=6; instruction[0xe1]=sbc6502; adrmode[0xe1]=indx6502;
      ticks[0xe2]=2; instruction[0xe2]=skb6502; adrmode[0xe2]=implied6502;
      ticks[0xe3]=2; instruction[0xe3]=ins6502; adrmode[0xe3]=indx6502;
      ticks[0xe4]=3; instruction[0xe4]=cpx6502; adrmode[0xe4]=zp6502;
      ticks[0xe5]=3; instruction[0xe5]=sbc6502; adrmode[0xe5]=zp6502;
      ticks[0xe6]=5; instruction[0xe6]=inc6502; adrmode[0xe6]=zp6502;
      ticks[0xe7]=2; instruction[0xe7]=ins6502; adrmode[0xe7]=zp6502;
      ticks[0xe8]=2; instruction[0xe8]=inx6502; adrmode[0xe8]=implied6502;
      ticks[0xe9]=3; instruction[0xe9]=sbc6502; adrmode[0xe9]=immediate6502;
      ticks[0xea]=2; instruction[0xea]=nop6502; adrmode[0xea]=implied6502;
      ticks[0xeb]=2; instruction[0xeb]=nop6502; adrmode[0xeb]=implied6502;
      ticks[0xec]=4; instruction[0xec]=cpx6502; adrmode[0xec]=abs6502;
      ticks[0xed]=4; instruction[0xed]=sbc6502; adrmode[0xed]=abs6502;
      ticks[0xee]=6; instruction[0xee]=inc6502; adrmode[0xee]=abs6502;
      ticks[0xef]=2; instruction[0xef]=ins6502; adrmode[0xef]=abs6502;
      ticks[0xf0]=2; instruction[0xf0]=beq6502; adrmode[0xf0]=relative6502;
      ticks[0xf1]=5; instruction[0xf1]=sbc6502; adrmode[0xf1]=indy6502;
      ticks[0xf2]=3; instruction[0xf2]=sbc6502; adrmode[0xf2]=indzp6502;
      ticks[0xf3]=2; instruction[0xf3]=ins6502; adrmode[0xf3]=indy6502;
      ticks[0xf4]=2; instruction[0xf4]=skb6502; adrmode[0xf4]=implied6502;
      ticks[0xf5]=4; instruction[0xf5]=sbc6502; adrmode[0xf5]=zpx6502;
      ticks[0xf6]=6; instruction[0xf6]=inc6502; adrmode[0xf6]=zpx6502;
      ticks[0xf7]=2; instruction[0xf7]=ins6502; adrmode[0xf7]=zpx6502;
      ticks[0xf8]=2; instruction[0xf8]=sed6502; adrmode[0xf8]=implied6502;
      ticks[0xf9]=4; instruction[0xf9]=sbc6502; adrmode[0xf9]=absy6502;
      ticks[0xfa]=4; instruction[0xfa]=plx6502; adrmode[0xfa]=implied6502;
      ticks[0xfb]=2; instruction[0xfb]=ins6502; adrmode[0xfb]=absy6502;
      ticks[0xfc]=2; instruction[0xfc]=skw6502; adrmode[0xfc]=implied6502;
      ticks[0xfd]=4; instruction[0xfd]=sbc6502; adrmode[0xfd]=absx6502;
      ticks[0xfe]=7; instruction[0xfe]=inc6502; adrmode[0xfe]=absx6502;
      ticks[0xff]=2; instruction[0xff]=ins6502; adrmode[0xff]=absx6502;
}

/* Reset CPU */
void reset6502()
{
       //fprintf(logfile, "Resetting 6502...\n");
	nmi_busy = FALSE;
    irq_busy = FALSE;
    irq_pending = FALSE;

       A=X=Y=P=0;
       P |= 0x20;
       S=0xff;
       PC = get6502memory(0xfffc & addrmask);
       PC |= get6502memory(0xfffd & addrmask) << 8;
       //fprintf(logfile, "Reset vector: %4X    addrmask %4X\n", PC, addrmask);

       totalticks = 0;
}

/* Non maskerable interrupt */
void nmi6502()
{
      put6502memory(0x0100+S--,(uint8)(PC>>8));
      put6502memory(0x0100+S--,(uint8)(PC & 0xff));
      put6502memory(0x0100+S--,P);
      P |= 0x04;
      PC = get6502memory(0xfffa & addrmask);
      PC |= get6502memory(0xfffb & addrmask) << 8;
      nmi_busy = TRUE;
}

/* Maskerable Interrupt */
void irq6502()
{
#ifdef DEBUG
#ifdef _EE
	printf("IRQ !\n");
#else
    fprintf(stderr, "IRQ !\n");
#endif
#endif

    // only do if not busy with NMI
    if(nmi_busy) {
    	irq_pending = TRUE;
#ifdef _DEBUG
			DebugPrint("IRQ deferred (NMI busy) vcount = %d\n", vcount);
#endif
		}
    else {
#ifdef _DEBUG
		DebugPrint("IRQ triggered. IRQST = %2X, vcount = %d\n", irqst, vcount);
#endif
		put6502memory(0x0100+S--,(uint8)(PC>>8));
		put6502memory(0x0100+S--,(uint8)(PC & 0xff));
		put6502memory(0x0100+S--,P);
		P |= 0x04;
		PC=get6502memory(0xfffe & addrmask);
		PC |= get6502memory(0xffff & addrmask) << 8;
        irq_busy = TRUE;
	}
}


/* Execute Instruction */
void exec6502(int n)
{
	timerTicks = n;
#ifndef CD_BUILD
#ifdef _EE
    printf("Executing %d CPU ticks...\n", timerTicks);
#else
	sprintf(errormsg, "Executing %d CPU ticks...\n", timerTicks);
	HostLog(errormsg);
#endif
#endif
   while (timerTicks > 0) 
   {
      // fetch instruction
      opcode = memory5200[PC++];

      // execute instruction
      instruction[opcode]();

      // calculate clock cycles
      clockticks6502 = ticks[opcode];
      timerTicks -= clockticks6502;
      // check for VBI or horiz sync etc
      updateANTIC();

      totalticks += clockticks6502;

   }
}

/* Fast version of Execute Instruction */
void exec6502fast(int n)
{	
	timerTicks = n;

#ifndef CD_BUILD
#ifdef _EE
    printf("Executing %d CPU ticks...\n", timerTicks);
#else
	sprintf(errormsg, "Executing %d CPU ticks...\n", timerTicks);
	HostLog(errormsg);
#endif
#endif

//#ifdef _EE
//	printf("Executing %d CPU ticks...\n", timerTicks);
//#endif

   tickstoHSYNC = 114;

	running = 1;
	// RFB: Don't check running every opcode!
   for (;;)
   {
		// fetch instruction
		opcode = memory5200[PC++];

		// calculate clock cycles
		// bug fix: this goes before instruction[] 'cos
		// instruction[] may alter clockticks6502
		clockticks6502 = ticks[opcode];

		// execute instruction
		instruction[opcode]();

		tickstoHSYNC -= clockticks6502;
		tickstilldraw -= clockticks6502;
		// check for VBI or horiz sync etc
		// check for HSYNC / DLI
		if(tickstoHSYNC < 0)
		{
			tickstoHSYNC += (TICKSTOHBL - stolencycles);
			updateANTIC();
		}

		// check for draw scheduled
		if(tickstilldraw < 0)
		{
			// render PF

			pf_line_render();

			// render PM stuff
			pm_line_render(vcount);
    		tickstilldraw = 99999;

         	// Check if we've been stopped.
         	if (!running)
         		break;
		}

      //totalticks += clockticks6502;
   }
}


/* Debugger version of Execute Instruction */
void exec6502debug(int n)
{
	timerTicks = n;

#ifndef CD_BUILD
#ifdef _EE
    printf("Executing %d CPU ticks...\n", timerTicks);
#else
	sprintf(errormsg, "Executing %d CPU ticks...\n", timerTicks);
	HostLog(errormsg);
#endif
#endif

   while (timerTicks > 0)
   {
      // fetch instruction
      opcode = memory5200[PC++];

      // calculate clock cycles
      // bug fix: this goes before instruction[] 'cos
      // instruction[] may alter clockticks6502
      clockticks6502 = ticks[opcode];

      // execute instruction
      instruction[opcode]();

      tickstoHSYNC -= clockticks6502;
      tickstilldraw -= clockticks6502;
      // check for VBI or horiz sync etc
      // check for HSYNC / DLI
      if(tickstoHSYNC < 0) {
         tickstoHSYNC += (TICKSTOHBL - stolencycles);
			updateANTIC();
		}
		// check for draw scheduled
      if(tickstilldraw < 0) {
			// render PF
			pf_line_render();
   		// render PM stuff
      	// hack: render missiles *before* players
      	// (works better for most games)
     		pm_line_render(vcount);
         tickstilldraw = 99999;
		}

      timerTicks -= clockticks6502;

   }
}

