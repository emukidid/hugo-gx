//  h6280.c - Execute CPU instructions
//

#include <stdio.h>
#include <stdlib.h>

#include "interupt.h"
#include "dis_runtime.h"
#include "pce.h"
#include "hard_pce.h"
#include "gfx.h"
#include "pce.h"
#include "utils.h"

#if defined(KERNEL_DEBUG)
static
int one_bit_set(UChar arg)
{
  return (arg != 0) && ((-arg & arg) == arg);
}
#endif

int Cycles_per_Line;
// flag-value table (for speed)

UChar flnz_list[256] = {
  FL_Z,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       // 00-0F
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          // 40-4F
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          // 70-7F
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // 80-87
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // 90-97
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // A0-A7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // B0-B7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // C0-C7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // D0-D7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // E0-E7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,  // F0-F7
  FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N,FL_N
};

// Elementary operations:
// - defined here for clarity, brevity,
//   and reduced typographical errors

// This code ignores hardware-segment accesses; it should only be used
// to access immediate data (hardware segment does not run code):
//
//  as a function:

inline UChar imm_operand(UInt16 addr) { 
  register unsigned short int memreg = addr>>13;
  return( (UChar) (PageR[memreg][addr]));
}

#if defined(INLINED_ACCESSORS)

// This is the more generalized access routine:
inline UChar get_8bit_addr(UInt16 addr) {
  register unsigned short int memreg = addr>>13;

  if (PageR[memreg] == IOAREA)
    return(IO_read(addr));
  else
    return((UChar) (PageR[memreg][addr]));
}

inline void put_8bit_addr(UInt16 addr, UChar byte) {
  register unsigned int memreg = addr>>13;

  if (PageW[memreg] == IOAREA) {
    IO_write(addr, byte);
  } else {
    PageW[memreg][addr] = byte;
  }
}

inline UInt16 get_16bit_addr(UInt16 addr) {
  register unsigned int memreg = addr>>13;
  UInt16 ret_16bit = (UChar) PageR[memreg][addr];
  memreg = (++addr)>>13;
  ret_16bit += (UInt16) ((UChar) PageR[memreg][addr] << 8);

  return(ret_16bit);
}

#endif

//Addressing modes:

#define abs_operand(x)     get_8bit_addr(get_16bit_addr(x))
#define absx_operand(x)    get_8bit_addr(get_16bit_addr(x)+reg_x)
#define absy_operand(x)    get_8bit_addr(get_16bit_addr(x)+reg_y)
#define zp_operand(x)      get_8bit_zp(imm_operand(x))
#define zpx_operand(x)     get_8bit_zp(imm_operand(x)+reg_x)
#define zpy_operand(x)     get_8bit_zp(imm_operand(x)+reg_y)
#define zpind_operand(x)   get_8bit_addr(get_16bit_zp(imm_operand(x)))
#define zpindx_operand(x)  get_8bit_addr(get_16bit_zp(imm_operand(x)+reg_x))
#define zpindy_operand(x)  get_8bit_addr(get_16bit_zp(imm_operand(x))+reg_y)

// Elementary flag check (flags 'N' and 'Z'):

#define chk_flnz_8bit(x) reg_p = ((reg_p & (~(FL_N|FL_T|FL_Z))) | flnz_list[x]);

inline UChar get_8bit_zp(UChar zp_addr) {
  return((UChar) *(zp_base + zp_addr) );
}

inline UInt16 get_16bit_zp(UChar zp_addr) {
  UInt16 n = *(zp_base + zp_addr);
  n += (*(zp_base + (UChar)(zp_addr+1)) << 8);
  return(n);
}

inline void put_8bit_zp(UChar zp_addr, UChar byte) {
  *(zp_base + zp_addr) = byte;
}

inline void push_8bit(UChar byte) {
  *(sp_base + reg_s--) = byte;
}

inline UChar pull_8bit(void) {
  return((UChar) *(sp_base + ++reg_s) );
}

inline void push_16bit(UInt16 addr) {
  *(sp_base + reg_s--) = (UChar)(addr>>8);
  *(sp_base + reg_s--) = (UChar)(addr&0xFF);
  return;
}

inline UInt16 pull_16bit(void) {
  UInt16 n = (UChar) *(sp_base + ++reg_s);
  n += (UInt16)(((UChar) *(sp_base + ++reg_s)) << 8);
  return(n);
}

//
// Implementation of actual opcodes:
//

/*@ -type */

static
UChar adc(UChar acc, UChar val) {
  Int16  sig  = (Char)acc;
  UInt16 usig = (UChar)acc;
  UInt16 temp;

  if (!(reg_p & FL_D)) {		/* binary mode */
    if (reg_p & FL_C) {
      usig++;
      sig++;
    }
    sig  += (Char)val;
    usig += (UChar)val;
    acc   = (UChar)(usig & 0xFF);

    reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z|FL_C))
            | (((sig > 127) || (sig < -128)) ? FL_V:0)
            | ((usig > 255) ? FL_C:0)
            | flnz_list[acc];

  } else {				/* decimal mode */

// treatment of out-of-range accumulator
// and operand values (non-BCD) is not
// adequately defined.	Nor is overflow
// flag treatment.

// Zeo : rewrote using bcdbin and binbcd arrays to boost code speed and fix
// residual bugs

    temp  = bcdbin[usig] + bcdbin[val];

    if (reg_p & FL_C) { temp++; }

    acc    = binbcd[temp];

    reg_p  = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	     | ((temp > 99) ? FL_C:0)
	     | flnz_list[acc];

    cycles++;	/* decimal mode takes an extra cycle */

  }
  return(acc);
}

static
void sbc(UChar val) {
  Int16  sig  = (Char)reg_a;
  UInt16 usig = (UChar)reg_a;
  Int16  temp;

  if (!(reg_p & FL_D)) {		/* binary mode */
    if (!(reg_p & FL_C)) {
      usig--;
      sig--;
    }
    sig   -= (Char)val;
    usig  -= (UChar)val;
    reg_a  = (UChar)(usig & 0xFF);
    reg_p  = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z|FL_C))
	     | (((sig > 127) || (sig < -128)) ? FL_V:0)
	     | ((usig > 255) ? 0:FL_C)
	     | flnz_list[reg_a];      /* FL_N, FL_Z */

  } else {				/* decimal mode */

// treatment of out-of-range accumulator
// and operand values (non-bcd) is not
// adequately defined.	Nor is overflow
// flag treatment.

    temp  = (Int16)(bcdbin[usig] - bcdbin[val]);

    if (!(reg_p & FL_C)) { temp--; }

    reg_p  = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	     | ((temp < 0) ? 0:FL_C);

    while (temp < 0) {
      temp += 100;
    }

    chk_flnz_8bit(reg_a = binbcd[temp]);

    cycles++;	/* decimal mode takes an extra cycle */

  }
}

UInt32 adc_abs(void) {
// if flag 'T' is set, use zero-page address specified by register 'X'
// as the accumulator...

  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), abs_operand(reg_pc+1)));
    cycles+=8;
  } else {
    reg_a = adc(reg_a, abs_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 adc_absx(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), absx_operand(reg_pc+1)));
    cycles+=8;
  } else {
    reg_a = adc(reg_a, absx_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 adc_absy(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), absy_operand(reg_pc+1)));
    cycles+=8;
  } else {
    reg_a = adc(reg_a, absy_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 adc_imm(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), imm_operand(reg_pc+1)));
    cycles+=5;
  } else {
    reg_a = adc(reg_a, imm_operand(reg_pc+1));
    cycles+=2;
  }
  reg_pc+=2;
  return 0;
}

UInt32 adc_zp(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), zp_operand(reg_pc+1)));
    cycles+=7;
  } else {
    reg_a = adc(reg_a, zp_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 adc_zpx(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), zpx_operand(reg_pc+1)));
    cycles+=7;
  } else {
    reg_a = adc(reg_a, zpx_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 adc_zpind(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), zpind_operand(reg_pc+1)));
    cycles+=10;
  } else {
    reg_a = adc(reg_a, zpind_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 adc_zpindx(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), zpindx_operand(reg_pc+1)));
    cycles+=10;
  } else {
    reg_a = adc(reg_a, zpindx_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 adc_zpindy(void) {
  if (reg_p & FL_T) {
    put_8bit_zp(reg_x, adc(get_8bit_zp(reg_x), zpindy_operand(reg_pc+1)));
    cycles+=10;
  } else {
    reg_a = adc(reg_a, zpindy_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_abs(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= abs_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a &= abs_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 and_absx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= absx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a &= absx_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 and_absy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= absy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a &= absy_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 and_imm(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= imm_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=5;

  } else {
    chk_flnz_8bit(reg_a &= imm_operand(reg_pc+1));
    cycles+=2;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_zp(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= zp_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a &= zp_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_zpx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= zpx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a &= zpx_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_zpind(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= zpind_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a &= zpind_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_zpindx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= zpindx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a &= zpindx_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 and_zpindy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp &= zpindy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a &= zpindy_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 asl_a(void) {
  UChar temp1 = reg_a;
  reg_a<<=1;
  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[reg_a];
  cycles+=2;
  reg_pc++;
  return 0;
}

UInt32 asl_abs(void) {
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = temp1<<1;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;

  put_8bit_addr(temp_addr,temp);
  reg_pc+=3;
  return 0;
}

UInt32 asl_absx(void) {
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = temp1<<1;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr,temp);
  reg_pc+=3;
  return 0;
}

UInt32 asl_zp(void) {
  UChar zp_addr = imm_operand(reg_pc+1);
  UChar temp1	= get_8bit_zp(zp_addr);
  UChar temp	= temp1<<1;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=6;
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  return 0;
}

UInt32 asl_zpx(void) {
  UChar  zp_addr = imm_operand(reg_pc+1)+reg_x;
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = temp1<<1;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=6;
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  return 0;
}

UInt32 bbr0(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x01) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr1(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x02) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr2(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x04) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr3(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x08) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr4(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x10) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr5(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x20) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr6(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x40) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbr7(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x80) {
    reg_pc+=3;
    cycles+=6;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  }
  return 0;
}

UInt32 bbs0(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x01) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs1(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x02) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs2(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x04) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs3(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x08) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs4(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x10) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs5(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x20) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs6(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x40) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bbs7(void) {
  reg_p &= ~FL_T;
  if (zp_operand(reg_pc+1)&0x80) {
    reg_pc+=(Char)imm_operand(reg_pc+2)+3;
    cycles+=8;
  } else {
    reg_pc+=3;
    cycles+=6;
  }
  return 0;
}

UInt32 bcc(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_C) {
    reg_pc+=2;
    cycles+=2;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  }
  return 0;
}

UInt32 bcs(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_C) {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  } else {
    reg_pc+=2;
    cycles+=2;
  }
  return 0;
}

UInt32 beq(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_Z) {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  } else {
    reg_pc+=2;
    cycles+=2;
  }
  return 0;
}

UInt32 bit_abs(void) {
  UChar temp = abs_operand(reg_pc+1);
  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80)  ? FL_N:0)
	  | ((temp&0x40)  ? FL_V:0)
	  | ((reg_a&temp) ? 0:FL_Z);
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 bit_absx(void) {
  UChar temp = absx_operand(reg_pc+1);
  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80)  ? FL_N:0)
	  | ((temp&0x40)  ? FL_V:0)
	  | ((reg_a&temp) ? 0:FL_Z);
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 bit_imm(void) {
// orig code (Eyes/Lichty said immediate mode did not affect
//            'N' and 'V' flags):
//reg_p = (reg_p & ~(FL_T|FL_Z))
//	  | ((reg_a & imm_operand(reg_pc+1)) ? 0:FL_Z);

  UChar temp = imm_operand(reg_pc+1);
  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80)  ? FL_N:0)
	  | ((temp&0x40)  ? FL_V:0)
	  | ((reg_a&temp) ? 0:FL_Z);
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 bit_zp(void) {
  UChar temp = zp_operand(reg_pc+1);
  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80)  ? FL_N:0)
	  | ((temp&0x40)  ? FL_V:0)
	  | ((reg_a&temp) ? 0:FL_Z);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 bit_zpx(void) {
  UChar temp = zpx_operand(reg_pc+1);
  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80)  ? FL_N:0)
	  | ((temp&0x40)  ? FL_V:0)
	  | ((reg_a&temp) ? 0:FL_Z);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 bmi(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_N) {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  } else {
    reg_pc+=2;
    cycles+=2;
  }
  return 0;
}

UInt32 bne(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_Z) {
    reg_pc+=2;
    cycles+=2;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  }
  return 0;
}

UInt32 bpl(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_N) {
    reg_pc+=2;
    cycles+=2;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  }
  return 0;
}

UInt32 bra(void) {
  reg_p &= ~FL_T;
  reg_pc+=(Char)imm_operand(reg_pc+1)+2;
  cycles+=4;
  return 0;
}

UInt32 brek(void) {
#if defined(KERNEL_DEBUG)
  fprintf(stderr, "BRK opcode has been hit [PC = 0x%04x] at %s(%d)\n", reg_pc, __FILE__, __LINE__);
#endif
  push_16bit(reg_pc+2);
  reg_p &= ~FL_T;
  push_8bit(reg_p|FL_B);
  reg_p =(reg_p & ~FL_D) | FL_I;
  reg_pc=get_16bit_addr(0xFFF6);
  cycles+=8;
  return 0;
}

UInt32 bsr(void) {
  reg_p &= ~FL_T;
  push_16bit(reg_pc+1);
  reg_pc+=(Char)imm_operand(reg_pc+1)+2;
  cycles+=8;
  return 0;
}

UInt32 bvc(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_V) {
    reg_pc+=2;
    cycles+=2;
  } else {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  }
  return 0;
}

UInt32 bvs(void) {
  reg_p &= ~FL_T;
  if (reg_p & FL_V) {
    reg_pc+=(Char)imm_operand(reg_pc+1)+2;
    cycles+=4;
  } else {
    reg_pc+=2;
    cycles+=2;
  }
  return 0;
}

UInt32 cla(void) {
  reg_p &= ~FL_T;
  reg_a = 0;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 clc(void) {
  reg_p &= ~(FL_T|FL_C);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 cld(void) {
  reg_p &= ~(FL_T|FL_D);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 cli(void) {
  reg_p &= ~(FL_T|FL_I);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 clv(void) {
  reg_p &= ~(FL_V|FL_T);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 clx(void) {
  reg_p &= ~FL_T;
  reg_x = 0;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 cly(void) {
  reg_p &= ~FL_T;
  reg_y = 0;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 cmp_abs(void) {
  UChar temp = abs_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 cmp_absx(void) {
  UChar temp = absx_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 cmp_absy(void) {
  UChar temp = absy_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 cmp_imm(void) {
  UChar temp = imm_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 cmp_zp(void) {
  UChar temp = zp_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 cmp_zpx(void) {
  UChar temp = zpx_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 cmp_zpind(void) {
  UChar temp = zpind_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 cmp_zpindx(void) {
  UChar temp = zpindx_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 cmp_zpindy(void) {
  UChar temp = zpindy_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_a < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_a-temp)];
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 cpx_abs(void) {
  UChar temp = abs_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_x < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_x-temp)];
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 cpx_imm(void) {
  UChar temp = imm_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_x < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_x-temp)];
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 cpx_zp(void) {
  UChar temp = zp_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_x < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_x-temp)];
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 cpy_abs(void) {
  UChar temp = abs_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_y < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_y-temp)];
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 cpy_imm(void) {
  UChar temp = imm_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_y < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_y-temp)];
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 cpy_zp(void) {
  UChar temp = zp_operand(reg_pc+1);

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((reg_y < temp) ? 0:FL_C)
	  | flnz_list[(UChar)(reg_y-temp)];
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 csh(void) {
  Cycles_per_Line = 454; /* 21477270 / 3 / 60 / 263 */ /* 7.16 Mhz CPU clock */
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 csl(void) {
  Cycles_per_Line = 113; /* 21477270 / 12 / 60 / 263 */ /* 1.78 Mhz CPU clock */
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 dec_a(void) {
  chk_flnz_8bit(--reg_a);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 dec_abs(void) {
  UChar  temp;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  chk_flnz_8bit(temp = get_8bit_addr(temp_addr)-1);
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 dec_absx(void) {
  UChar  temp;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  chk_flnz_8bit(temp = get_8bit_addr(temp_addr)-1);
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 dec_zp(void) {
  UChar  temp;
  UChar  zp_addr = imm_operand(reg_pc+1);
  chk_flnz_8bit(temp = get_8bit_zp(zp_addr)-1);
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 dec_zpx(void) {
  UChar  temp;
  UChar  zp_addr = imm_operand(reg_pc+1)+reg_x;
  chk_flnz_8bit(temp = get_8bit_zp(zp_addr)-1);
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 dex(void) {
  chk_flnz_8bit(--reg_x);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 dey(void) {
  chk_flnz_8bit(--reg_y);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 eor_abs(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= abs_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a ^= abs_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 eor_absx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= absx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a ^= absx_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 eor_absy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= absy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a ^= absy_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 eor_imm(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= imm_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=5;

  } else {
    chk_flnz_8bit(reg_a ^= imm_operand(reg_pc+1));
    cycles+=2;
  }
  reg_pc+=2;
  return 0;
}

UInt32 eor_zp(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= zp_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a ^= zp_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 eor_zpx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= zpx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a ^= zpx_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 eor_zpind(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= zpind_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a ^= zpind_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 eor_zpindx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= zpindx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a ^= zpindx_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 eor_zpindy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp ^= zpindy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a ^= zpindy_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 halt(void) {
  return(1);
}

UInt32 inc_a(void) {
  chk_flnz_8bit(++reg_a);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 inc_abs(void) {
  UChar  temp;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  chk_flnz_8bit(temp = get_8bit_addr(temp_addr)+1);
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 inc_absx(void) {
  UChar  temp;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  chk_flnz_8bit(temp = get_8bit_addr(temp_addr)+1);
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 inc_zp(void) {
  UChar temp;
  UChar zp_addr = imm_operand(reg_pc+1);
  chk_flnz_8bit(temp = get_8bit_zp(zp_addr)+1);
  put_8bit_zp(zp_addr, temp);

  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 inc_zpx(void) {
  UChar temp;
  UChar zp_addr = imm_operand(reg_pc+1)+reg_x;
  chk_flnz_8bit(temp = get_8bit_zp(zp_addr)+1);
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 inx(void) {
  chk_flnz_8bit(++reg_x);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 iny(void) {
  chk_flnz_8bit(++reg_y);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 jmp(void) {
  reg_p &= ~FL_T;
  reg_pc = get_16bit_addr(reg_pc+1);
  cycles+=4;
  return 0;
}

UInt32 jmp_absind(void) {
  reg_p &= ~FL_T;
  reg_pc = get_16bit_addr(get_16bit_addr(reg_pc+1));
  cycles+=7;
  return 0;
}

UInt32 jmp_absindx(void) {
  reg_p &= ~FL_T;
  reg_pc = get_16bit_addr(get_16bit_addr(reg_pc+1)+reg_x);
  cycles+=7;
  return 0;
}

UInt32 jsr(void) {
  reg_p &= ~FL_T;
  push_16bit(reg_pc+2);
  reg_pc = get_16bit_addr(reg_pc+1);
  cycles+=7;
  return 0;
}

UInt32 lda_abs(void) {
  chk_flnz_8bit(reg_a = abs_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 lda_absx(void) {
  chk_flnz_8bit(reg_a = absx_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 lda_absy(void) {
  chk_flnz_8bit(reg_a = absy_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 lda_imm(void) {
  chk_flnz_8bit(reg_a = imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 lda_zp(void) {
  chk_flnz_8bit(reg_a = zp_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 lda_zpx(void) {
  chk_flnz_8bit(reg_a = zpx_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 lda_zpind(void) {
  chk_flnz_8bit(reg_a = zpind_operand(reg_pc+1));

  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 lda_zpindx(void) {
  chk_flnz_8bit(reg_a = zpindx_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 lda_zpindy(void) {
  chk_flnz_8bit(reg_a = zpindy_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 ldx_abs(void) {
  chk_flnz_8bit(reg_x = abs_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 ldx_absy(void) {
  chk_flnz_8bit(reg_x = absy_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 ldx_imm(void) {
  chk_flnz_8bit(reg_x = imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 ldx_zp(void) {
  chk_flnz_8bit(reg_x = zp_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 ldx_zpy(void) {
  chk_flnz_8bit(reg_x = zpy_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 ldy_abs(void) {
  chk_flnz_8bit(reg_y = abs_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 ldy_absx(void) {
  chk_flnz_8bit(reg_y = absx_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 ldy_imm(void) {
  chk_flnz_8bit(reg_y = imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 ldy_zp(void) {
  chk_flnz_8bit(reg_y = zp_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 ldy_zpx(void) {
  chk_flnz_8bit(reg_y = zpx_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 lsr_a(void) {
  UChar temp = reg_a;
  reg_a/=2;
  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp&1) ? FL_C:0)
	  | flnz_list[reg_a];
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 lsr_abs(void) {
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = temp1/2;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1&1) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 lsr_absx(void) {
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = temp1/2;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1&1) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 lsr_zp(void) {
  UChar  zp_addr = imm_operand(reg_pc+1);
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = temp1/2;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1&1) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 lsr_zpx(void) {
  UChar  zp_addr = imm_operand(reg_pc+1)+reg_x;
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = temp1/2;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1&1) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 nop(void)  {
  reg_p &= ~FL_T;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 ora_abs(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= abs_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a |= abs_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 ora_absx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= absx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a |= absx_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 ora_absy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= absy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=8;

  } else {
    chk_flnz_8bit(reg_a |= absy_operand(reg_pc+1));
    cycles+=5;
  }
  reg_pc+=3;
  return 0;
}

UInt32 ora_imm(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= imm_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=5;

  } else {
    chk_flnz_8bit(reg_a |= imm_operand(reg_pc+1));
    cycles+=2;
  }
  reg_pc+=2;
  return 0;
}

UInt32 ora_zp(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= zp_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a |= zp_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 ora_zpx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= zpx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=7;

  } else {
    chk_flnz_8bit(reg_a |= zpx_operand(reg_pc+1));
    cycles+=4;
  }
  reg_pc+=2;
  return 0;
}

UInt32 ora_zpind(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= zpind_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a |= zpind_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 ora_zpindx(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= zpindx_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a |= zpindx_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 ora_zpindy(void) {
  if (reg_p & FL_T) {
    UChar temp = get_8bit_zp(reg_x);
    chk_flnz_8bit(temp |= zpindy_operand(reg_pc+1));
    put_8bit_zp(reg_x, temp);
    cycles+=10;

  } else {
    chk_flnz_8bit(reg_a |= zpindy_operand(reg_pc+1));
    cycles+=7;
  }
  reg_pc+=2;
  return 0;
}

UInt32 pha(void) {
  reg_p &= ~FL_T;
  push_8bit(reg_a);
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 php(void) {
  reg_p &= ~FL_T;
  push_8bit(reg_p);
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 phx(void) {
  reg_p &= ~FL_T;
  push_8bit(reg_x);
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 phy(void) {
  reg_p &= ~FL_T;
  push_8bit(reg_y);
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 pla(void) {
  chk_flnz_8bit(reg_a = pull_8bit());
  reg_pc++;
  cycles+=4;
  return 0;
}

UInt32 plp(void) {
  reg_p = pull_8bit();
  reg_pc++;
  cycles+=4;
  return 0;
}

UInt32 plx(void) {
  chk_flnz_8bit(reg_x = pull_8bit());
  reg_pc++;
  cycles+=4;
  return 0;
}

UInt32 ply(void) {
  chk_flnz_8bit(reg_y = pull_8bit());
  reg_pc++;
  cycles+=4;
  return 0;
}

UInt32 rmb0(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x01));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb1(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x02));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb2(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x04));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb3(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x08));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb4(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x10));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb5(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x20));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb6(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x40));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rmb7(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) & (~0x80));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 rol_a(void) {
  UChar flg_tmp = (reg_p & FL_C) ? 1:0;
  UChar temp = reg_a;

  reg_a = (reg_a<<1)+flg_tmp;
  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp & 0x80) ? FL_C:0)
	  | flnz_list[reg_a];
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 rol_abs(void) {
  UChar  flg_tmp   = (reg_p & FL_C) ? 1:0;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = (temp1<<1)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 rol_absx(void) {
  UChar  flg_tmp   = (reg_p & FL_C) ? 1:0;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = (temp1<<1)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 rol_zp(void) {
  UChar  flg_tmp = (reg_p & FL_C) ? 1:0;
  UChar  zp_addr = imm_operand(reg_pc+1);
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = (temp1<<1)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 rol_zpx(void) {
  UChar  flg_tmp = (reg_p & FL_C) ? 1:0;
  UChar  zp_addr = imm_operand(reg_pc+1)+reg_x;
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = (temp1<<1)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x80) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 ror_a(void) {
  UChar flg_tmp = (reg_p & FL_C) ? 0x80:0;
  UChar temp	= reg_a;

  reg_a = (reg_a/2)+flg_tmp;
  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp & 0x01) ? FL_C:0)
	  | flnz_list[reg_a];
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 ror_abs(void) {
  UChar  flg_tmp   = (reg_p & FL_C) ? 0x80:0;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1);
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = (temp1/2)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x01) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 ror_absx(void) {
  UChar  flg_tmp   = (reg_p & FL_C) ? 0x80:0;
  UInt16 temp_addr = get_16bit_addr(reg_pc+1)+reg_x;
  UChar  temp1	   = get_8bit_addr(temp_addr);
  UChar  temp	   = (temp1/2)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x01) ? FL_C:0)
	  | flnz_list[temp];
  cycles+=7;
  put_8bit_addr(temp_addr, temp);
  reg_pc+=3;
  return 0;
}

UInt32 ror_zp(void) {
  UChar  flg_tmp = (reg_p & FL_C) ? 0x80:0;
  UChar  zp_addr = imm_operand(reg_pc+1);
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = (temp1/2)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x01) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 ror_zpx(void) {
  UChar  flg_tmp = (reg_p & FL_C) ? 0x80:0;
  UChar  zp_addr = imm_operand(reg_pc+1)+reg_x;
  UChar  temp1	 = get_8bit_zp(zp_addr);
  UChar  temp	 = (temp1/2)+flg_tmp;

  reg_p = (reg_p & ~(FL_N|FL_T|FL_Z|FL_C))
	  | ((temp1 & 0x01) ? FL_C:0)
	  | flnz_list[temp];
  put_8bit_zp(zp_addr, temp);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 rti(void) {
  /* FL_B reset in RTI */
  reg_p = pull_8bit() & ~FL_B;
  reg_pc = pull_16bit();
  cycles+=7;
  return 0;
}

UInt32 rts(void) {
  reg_p &= ~FL_T;
  reg_pc = pull_16bit()+1;
  cycles+=7;
  return 0;
}

UInt32 sax(void) {
  UChar temp = reg_x;
  reg_p &= ~FL_T;
  reg_x = reg_a;
  reg_a = temp;
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 say(void) {
  UChar temp = reg_y;
  reg_p &= ~FL_T;
  reg_y = reg_a;
  reg_a = temp;
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 sbc_abs(void) {
  sbc(abs_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 sbc_absx(void) {
  sbc(absx_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 sbc_absy(void) {
  sbc(absy_operand(reg_pc+1));
  reg_pc+=3;
  cycles+=5;
  return 0;
}

UInt32 sbc_imm(void) {
  sbc(imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=2;
  return 0;
}

UInt32 sbc_zp(void) {
  sbc(zp_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sbc_zpx(void) {
  sbc(zpx_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sbc_zpind(void) {
  sbc(zpind_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 sbc_zpindx(void) {
  sbc(zpindx_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 sbc_zpindy(void) {
  sbc(zpindy_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 sec(void) {
  reg_p = (reg_p|FL_C) & ~FL_T;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 sed(void) {
  reg_p = (reg_p|FL_D) & ~FL_T;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 sei(void) {
#ifdef DUMP_ON_SEI
	UInt32 i;
	Log("MMR[7]\n");
	for (i = 0xE000; i < 0xE100; i++)
		{
			Log("%02X ", get_8bit_addr(i));
		}
#endif
  reg_p = (reg_p|FL_I) & ~FL_T;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 set(void) {
  reg_p |= FL_T;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 smb0(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x01);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb1(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x02);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb2(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x04);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb3(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x08);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb4(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x10);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb5(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x20);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb6(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x40);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 smb7(void) {
  UChar temp = imm_operand(reg_pc+1);
  reg_p &= ~FL_T;
  put_8bit_zp(temp, get_8bit_zp(temp) | 0x80);
  reg_pc+=2;
  cycles+=7;
  return 0;
}

UInt32 st0(void) {
  reg_p &= ~FL_T;
  IO_write(0,imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 st1(void) {
  reg_p &= ~FL_T;
  IO_write(2,imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 st2(void) {
  reg_p &= ~FL_T;
  IO_write(3,imm_operand(reg_pc+1));
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sta_abs(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1), reg_a);
  reg_pc+=3;
  return 0;
}

UInt32 sta_absx(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1)+reg_x, reg_a);
  reg_pc+=3;
  return 0;
}

UInt32 sta_absy(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1)+reg_y, reg_a);
  reg_pc+=3;
  return 0;
}

UInt32 sta_zp(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1), reg_a);

  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sta_zpx(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1)+reg_x, reg_a);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sta_zpind(void) {
  reg_p &= ~FL_T;
  cycles+=7;
  put_8bit_addr(get_16bit_zp(imm_operand(reg_pc+1)), reg_a);
  reg_pc+=2;
  return 0;
}

UInt32 sta_zpindx(void) {
  reg_p &= ~FL_T;
  cycles+=7;
  put_8bit_addr(get_16bit_zp(imm_operand(reg_pc+1)+reg_x), reg_a);
  reg_pc+=2;
  return 0;
}

UInt32 sta_zpindy(void) {
  reg_p &= ~FL_T;
  cycles+=7;
  put_8bit_addr(get_16bit_zp(imm_operand(reg_pc+1))+reg_y, reg_a);
  reg_pc+=2;
  return 0;
}

UInt32 stx_abs(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1), reg_x);
  reg_pc+=3;
  return 0;
}

UInt32 stx_zp(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1), reg_x);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 stx_zpy(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1)+reg_y, reg_x);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sty_abs(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1), reg_y);
  reg_pc+=3;
  return 0;
}

UInt32 sty_zp(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1), reg_y);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sty_zpx(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1)+reg_x, reg_y);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 stz_abs(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr(get_16bit_addr(reg_pc+1), 0);
  reg_pc+=3;
  return 0;
}

UInt32 stz_absx(void) {
  reg_p &= ~FL_T;
  cycles+=5;
  put_8bit_addr((get_16bit_addr(reg_pc+1)+reg_x), 0);
  reg_pc+=3;
  return 0;
}

UInt32 stz_zp(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1), 0);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 stz_zpx(void) {
  reg_p &= ~FL_T;
  put_8bit_zp(imm_operand(reg_pc+1)+reg_x, 0);
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 sxy(void) {
  UChar temp = reg_y;
  reg_p &= ~FL_T;
  reg_y = reg_x;
  reg_x = temp;
  reg_pc++;
  cycles+=3;
  return 0;
}

UInt32 tai(void) {
  UInt16 from, to, len, alternate;

  reg_p &= ~FL_T;
  from = get_16bit_addr(reg_pc+1);
  to   = get_16bit_addr(reg_pc+3);
  len  = get_16bit_addr(reg_pc+5);
  if (len == 0) len = 0xffff; /* from pcetech.txt */
  alternate = 0;

#if defined(CD_DEBUG) && defined(INLINED_ACCESSORS)
  fprintf(stderr, "Transfert from bank 0x%02x to bank 0x%02x\n", mmr[from >> 13], mmr[to >> 13]);
#endif

  cycles+=(6 * len) + 17;
  while (len-- != 0) {
    put_8bit_addr(to++, get_8bit_addr(from+alternate));
   alternate ^= 1;
  }
  reg_pc+=7;
  return 0;
}

/* from pcetech.txt */
static int tamwrite = -1;
static int tmaread = 0;

UInt32 tam(void) {
  UInt16 i;
  UChar bitfld = imm_operand(reg_pc+1);

#if defined(KERNEL_DEBUG)
  if (bitfld == 0)
    {
      fprintf(stderr, "TAM with argument 0\n");
      Log("TAM with argument 0\n");
    }
  else if (!one_bit_set(bitfld))
    {
      fprintf(stderr, "TAM with unusual argument 0x%02x\n", bitfld);
      Log("TAM with unusual argument 0x%02x\n", bitfld);
    }
#endif

  tamwrite = -1;
  for (i = 0; i < 8; i++) {
    if (bitfld & (1 << i)) {
      mmr[i] = reg_a;
      bank_set(i, reg_a);
      tamwrite = reg_a; /* from pcetech.txt */
    }
  }

  reg_p &= ~FL_T;
  reg_pc+=2;
  cycles+=5;
  return 0;
}

UInt32 tax(void) {
  chk_flnz_8bit(reg_x = reg_a);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 tay(void) {
  chk_flnz_8bit(reg_y = reg_a);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 tdd(void) {
  UInt16 from, to, len;

  reg_p &= ~FL_T;
  from = get_16bit_addr(reg_pc+1);
  to   = get_16bit_addr(reg_pc+3);
  len  = get_16bit_addr(reg_pc+5);
  if (len == 0) len = 0xffff; /* from pcetech.txt */

#if defined(CD_DEBUG) && defined(INLINED_ACCESSORS)
  fprintf(stderr, "Transfert from bank 0x%02x to bank 0x%02x\n", mmr[from >> 13], mmr[to >> 13]);
#endif

  cycles+=(6 * len) + 17;
  while (len-- != 0) {
    put_8bit_addr(to--, get_8bit_addr(from--));
  }
  reg_pc+=7;
  return 0;
}

UInt32 tia(void) {
  UInt16 from, to, len, alternate;

  reg_p &= ~FL_T;
  from = get_16bit_addr(reg_pc+1);
  to   = get_16bit_addr(reg_pc+3);
  len  = get_16bit_addr(reg_pc+5);
  if (len == 0) len = 0xffff; /* from pcetech.txt */
  alternate = 0;

#if defined(CD_DEBUG) && defined(INLINED_ACCESSORS)
  fprintf(stderr, "Transfert from bank 0x%02x to bank 0x%02x\n", mmr[from >> 13], mmr[to >> 13]);
#endif

  cycles+=(6 * len) + 17;
  while (len-- != 0) {
    put_8bit_addr(to+alternate, get_8bit_addr(from++));
    alternate ^= 1;
  }
  reg_pc+=7;
  return 0;
}

UInt32 tii(void) {
  UInt16 from, to, len;

  reg_p &= ~FL_T;
  from = get_16bit_addr(reg_pc+1);
  to   = get_16bit_addr(reg_pc+3);
  len  = get_16bit_addr(reg_pc+5);
  if (len == 0) len = 0xffff; /* from pcetech.txt */

#if defined(CD_DEBUG) && defined(INLINED_ACCESSORS)
  fprintf(stderr, "Transfert from bank 0x%02x to bank 0x%02x\n", mmr[from >> 13], mmr[to >> 13]);
#endif

  cycles+=(6 * len) + 17;
  while (len-- != 0) {
    put_8bit_addr(to++, get_8bit_addr(from++));
  }
  reg_pc+=7;
  return 0;
}

UInt32 tin(void) {
  UInt16 from, to, len;

  reg_p &= ~FL_T;
  from = get_16bit_addr(reg_pc+1);
  to   = get_16bit_addr(reg_pc+3);
  len  = get_16bit_addr(reg_pc+5);
  if (len == 0) len = 0xffff; /* from pcetech.txt */

#if defined(CD_DEBUG) && defined(INLINED_ACCESSORS)
  fprintf(stderr, "Transfert from bank 0x%02x to bank 0x%02x\n", mmr[from >> 13], mmr[to >> 13]);
#endif

  cycles+=(6 * len) + 17;
  while (len-- != 0) {
    put_8bit_addr(to, get_8bit_addr(from++));
  }
  reg_pc+=7;
  return 0;
}

UInt32 tma(void) {
  UInt32 i;
  UChar bitfld = imm_operand(reg_pc+1);

#if defined(KERNEL_DEBUG)
  if (bitfld == 0)
    {
      fprintf(stderr, "TMA with argument 0\n");
      Log("TMA with argument 0\n");
    }
  else if (!one_bit_set(bitfld))
    {
      fprintf(stderr, "TMA with unusual argument 0x%02x\n", bitfld);
      Log("TMA with unusual argument 0x%02x\n", bitfld);
    }
#endif

  /* from pcetech.txt */
  if (bitfld & 0xff)
  {
    for (i = 0; i < 8; i++) {
      if (bitfld & (1 << i)) {
        reg_a = mmr[i];
      }
    }

    tmaread = reg_a;
  }
  else
  {
    reg_a = (tamwrite != -1) ? tamwrite : tmaread;
  }

  reg_p &= ~FL_T;
  reg_pc+=2;
  cycles+=4;
  return 0;
}

UInt32 trb_abs(void) {
  UInt16 abs_addr = get_16bit_addr(reg_pc+1);
  UChar  temp	  = get_8bit_addr(abs_addr);
  UChar  temp1	  = (~reg_a) & temp;

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp1&0x80) ? FL_N:0)
	  | ((temp1&0x40) ? FL_V:0)
	  | ((temp & reg_a) ? 0:FL_Z);
  cycles+=7;
  put_8bit_addr(abs_addr, temp1);
  reg_pc+=3;
  return 0;
}

UInt32 trb_zp(void) {
  UChar zp_addr  = imm_operand(reg_pc+1);
  UChar temp	 = get_8bit_zp(zp_addr);
  UChar temp1	 = (~reg_a) & temp;

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp1&0x80) ? FL_N:0)
	  | ((temp1&0x40) ? FL_V:0)
	  | ((temp & reg_a) ? 0:FL_Z);
  put_8bit_zp(zp_addr, temp1);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 tsb_abs(void) {
  UInt16 abs_addr = get_16bit_addr(reg_pc+1);
  UChar  temp	  = get_8bit_addr(abs_addr);
  UChar  temp1	  = reg_a | temp;

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp1&0x80) ? FL_N:0)
	  | ((temp1&0x40) ? FL_V:0)
	  | ((temp & reg_a) ? 0:FL_Z);
  cycles+=7;
  put_8bit_addr(abs_addr, temp1);
  reg_pc+=3;
  return 0;
}

UInt32 tsb_zp(void) {
  UChar zp_addr  = imm_operand(reg_pc+1);
  UChar temp	 = get_8bit_zp(zp_addr);
  UChar temp1	 = reg_a | temp;

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp1&0x80) ? FL_N:0)
	  | ((temp1&0x40) ? FL_V:0)
	  | ((temp & reg_a) ? 0:FL_Z);
  put_8bit_zp(zp_addr, temp1);
  reg_pc+=2;
  cycles+=6;
  return 0;
}

UInt32 tstins_abs(void) {
  UChar  imm	  = imm_operand(reg_pc+1);
  UChar  temp	  = abs_operand(reg_pc+2);

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80) ? FL_N:0)
	  | ((temp&0x40) ? FL_V:0)
	  | ((temp&imm)  ? 0:FL_Z);
  cycles+=8;
  reg_pc+=4;
  return 0;
}

UInt32 tstins_absx(void) {
  UChar  imm	  = imm_operand(reg_pc+1);
  UChar  temp	  = absx_operand(reg_pc+2);

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80) ? FL_N:0)
	  | ((temp&0x40) ? FL_V:0)
	  | ((temp&imm)  ? 0:FL_Z);
  cycles+=8;
  reg_pc+=4;
  return 0;
}

UInt32 tstins_zp(void) {
  UChar imm	= imm_operand(reg_pc+1);
  UChar temp	= zp_operand(reg_pc+2);

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80) ? FL_N:0)
	  | ((temp&0x40) ? FL_V:0)
	  | ((temp&imm)  ? 0:FL_Z);
  cycles+=7;
  reg_pc+=3;
  return 0;
}

UInt32 tstins_zpx(void) {
  UChar imm	= imm_operand(reg_pc+1);
  UChar temp	= zpx_operand(reg_pc+2);

  reg_p = (reg_p & ~(FL_N|FL_V|FL_T|FL_Z))
	  | ((temp&0x80) ? FL_N:0)
	  | ((temp&0x40) ? FL_V:0)
	  | ((temp&imm)  ? 0:FL_Z);
  cycles+=7;
  reg_pc+=3;
  return 0;
}


UInt32 tsx(void) {
  chk_flnz_8bit(reg_x = reg_s);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 txa(void) {
  chk_flnz_8bit(reg_a = reg_x);
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 txs(void) {
  reg_p &= ~FL_T;
  reg_s = reg_x;
  reg_pc++;
  cycles+=2;
  return 0;
}

UInt32 tya(void) {
  chk_flnz_8bit(reg_a = reg_y);
  reg_pc++;
  cycles+=2;
  return 0;
}

// perform machine operations for IRQ2:

UInt32 int_irq2 (void) {
  if ((io.irq_mask & FL_IRQ2) != 0) {   // interrupt disabled
     return 0;
  }
//if ((irq_register & FL_IRQ2) == 0) {	 // interrupt disabled ?
//   return 0;
//}
  cycles+=7;
  push_16bit(reg_pc);
  push_8bit(reg_p);
  reg_p = (reg_p & ~FL_D) | FL_I;
  reg_pc = get_16bit_addr(0xFFF6);
  return 0;
}

// perform machine operations for IRQ1 (video interrupt):

UInt32 int_irq1 (void) {
  if ((io.irq_mask & FL_IRQ1) != 0) {   // interrupt disabled
     return 0;
  }
//if ((irq_register & FL_IRQ1) == 0) {	 // interrupt disabled ?
//   return 0;
//}
  cycles+=7;
  push_16bit(reg_pc);
  push_8bit(reg_p);
  reg_p = (reg_p & ~FL_D) | FL_I;
  reg_pc = get_16bit_addr(0xFFF8);
  return 0;
}

// perform machine operations for timer interrupt:

UInt32 int_tiq(void) {
  if ((io.irq_mask & FL_TIQ) != 0) {    // interrupt disabled
     return 0;
  }
//if ((irq_register & FL_TIQ) == 0) {	// interrupt disabled ?
//   return 0;
//}
  cycles+=7;
  push_16bit(reg_pc);
  push_8bit(reg_p);
  reg_p = (reg_p & ~FL_D) | FL_I;
  reg_pc = get_16bit_addr(0xFFFA);
  return 0;
}

/*@ =type */

// Execute a single instruction :

void exe_instruct(void) {
  (*optable_runtime[PageR[reg_pc>>13][reg_pc]].func_exe)();
}

static void
Int6502 (UChar Type)
{
  UInt16 J = 0;

  if ((Type == INT_NMI) || (!(reg_p & FL_I)))
    {
      cycles += 7;
      push_16bit(reg_pc);
      push_8bit(reg_p);
      reg_p = (reg_p & ~FL_D);
/*
      WrRAM (SP + R->S,
	     ((R->P & ~(B_FLAG | T_FLAG)) & ~(N_FLAG | V_FLAG | Z_FLAG)) |
	     (R->NF & N_FLAG) | (R->VF & V_FLAG) | (R->ZF ? 0 : Z_FLAG));
*/

      if (Type == INT_NMI)
	{
	  J = VEC_NMI;
	}
      else
	{
          reg_p |=  FL_I;

	  switch (Type)
	    {

	    case INT_IRQ:
	      J = VEC_IRQ;
	      break;

	    case INT_IRQ2:
	      J = VEC_IRQ2;
	      break;

	    case INT_TIMER:
	      J = VEC_TIMER;
	      break;

	    }

	}
#ifdef KERNEL_DEBUG
			Log("Interruption : %04X\n", J);
#endif
      reg_pc = get_16bit_addr((UInt16)J);
    } else {
#ifdef KERNEL_DEBUG
    	Log("Dropped interruption %02X\n", Type);
#endif
    }
}

#ifdef KERNEL_DEBUG
//! Log all needed info to guess what went wrong in the cpu
void dump_pce_core() {

  int i;

  fprintf(stderr, "Dumping PCE core\n");

  Log("PC = 0x%04x\n", reg_pc);
  Log("A = 0x%02x\n", reg_a);
  Log("X = 0x%02x\n", reg_x);
  Log("Y = 0x%02x\n", reg_y);
  Log("P = 0x%02x\n", reg_p);
  Log("S = 0x%02x\n", reg_s);

  for (i = 0; i < 8; i++)
    {
      Log("MMR[%d] = 0x%02x\n", i, mmr[i]);
    }

  for (i = 0x2000; i < 0xFFFF; i++)
    {

      if ((i & 0xF) == 0)
        {
          Log("%04X: ", i);
        }

      Log("%02x ", get_8bit_addr((UInt16)i));
      if ((i & 0xF) == 0xF)
        {
          Log("\n");
        }
      if ((i & 0x1FFF) == 0x1FFF)
        {
          Log("\n-------------------------------------------------------------\n");
        }
    }
}
#endif


#ifdef BENCHMARK
/* code copied from utils.c */
#include <sys/time.h>
static double osd_getTime(void)
{
#ifdef WIN32  
  return (SDL_GetTicks() * 1e-3);
#elif defined(DJGPP)
  return uclock() * (1.0 / UCLOCKS_PER_SEC);
#else
  struct timeval tp;

  gettimeofday(&tp, NULL);
  // printf("current microsec = %f\n",tp.tv_sec + 1e-6 * tp.tv_usec);
  return tp.tv_sec + 1e-6 * tp.tv_usec;
#endif
}
#endif

// Execute instructions as a machine would, including all
// important (known) interrupts, hardware functions, and
// actual video display on the hardware
//
// Until the following happens:
// (1) An unknown instruction is to be executed
// (2) An unknown hardware access is performed
// (3) <ESC> key is hit

void exe_go(void) {
  int err = 0;
  UChar I;

#if defined(KERNEL_DEBUG)
  UInt16 old_reg_pc;
#endif

#ifdef BENCHMARK
  static int countNb = 8;   /* run for 8 * 65536 scan lines */
  static int countScan = 0; /* scan line counter */
  static double lastTime;
  lastTime = osd_getTime();
#endif

  // err is set on a 'trap':
  while (!err) {

//    Log("Pc = %04x : %s\n", reg_pc, optable_runtime[Page[reg_pc>>13][reg_pc]].opname);

#ifdef DEBUG_KERNEL_DS
/*
      if (reg_pc<0xE000)
      {
      Log("PC = %04X (%02X), A = %02X, X = %02X, Y = %02X, P = %02X\n",
          reg_pc,
          Page[reg_pc>>13][reg_pc],
          reg_a,
          reg_x,
          reg_y,
          reg_p);

      {
        int i;
        for (i=0xF0; i<256; i++)
          Log("%02X",get_8bit_zp(i));
        Log("\n");
      }

      }
*/
#endif

    /*
    {
      static char in_rom = 0;
      if (reg_pc == 0x3800) in_rom = 1;

      if (in_rom)
        Log("PC = %04X\n",reg_pc, in_rom);

      if (reg_pc == 0x3837)
        {
          int i;
          for (i = 0; i < 8; i++)
            Log("tmm[%d] = %d\n",i,mmr[i]);
          for (i = 0xE000; i < 0xFFFE; i++)
            Log("%02x%s",Page[i>>13][i],i & 0xf?"":"\n");
        }

      if (reg_pc >= 0xE000)
        in_rom = 0;

      if (reg_pc == 0x4000)
        {
          int i;
          for (i = 0; i < 8; i++)
            Log("tmm[%d] = %d\n",i,mmr[i]);
          for (i = 0x68; i < 0x7F; i++)
            {
              int x;
              Log("bank %d :",i);
              for (x = 0; x < 20; x++)
                Log("%02X",ROMMap[i][x]);
              Log("\n");
            }
        }

    }
    */

#if defined(SHARED_MEMORY)
    if (external_control_cpu >= 0) {
      while (external_control_cpu == 0) {
#if defined(WIN32)
        SDL_Delay(1);
#else
        usleep(1);
#endif
      }
      if (external_control_cpu > 0)
        external_control_cpu--;
    }
#endif

#if defined(KERNEL_DEBUG)
    old_reg_pc = reg_pc;
#endif

#undef OPCODE_LOGGING
#if defined(OPCODE_LOGGING)
#if defined(SDL)
    extern Uint8* key;

	// if (!key[SDLK_F11])
#endif
	if (0xC7A8 == reg_pc) {
		UChar offset = PageR[reg_pc >> 13][reg_pc + 1];
		Log("C7A8 !! %02X\n", offset);
		Log("zp[%02X] = %02X\n", offset, get_8bit_addr(get_16bit_zp(offset)));
	}
    Log("[%04X] (%02X) (%02X,%02X,%02X) (%02X,%02X) {%02X,%04X} {%02X}\n",
        reg_pc,
        PageR[reg_pc>>13][reg_pc],
        reg_a,
        reg_x,
        reg_y,
        reg_s,
        reg_p,
        get_8bit_addr(get_16bit_zp(0)),
        get_16bit_zp(0),
        get_8bit_zp(0x48));
#endif

#ifdef USE_INSTR_SWITCH
#include "instr-switch.c"
#else
    err = (*optable_runtime[PageR[reg_pc>>13][reg_pc]].func_exe)();
#endif

#if defined(KERNEL_DEBUG)

    if (mmr[2] == 0x69)
      {
        static int old_4062 = -1;
        if (PageR[0x4062 >> 13][0x4062] != old_4062)
          {
            Log("[0x4062] changed from 0x%02x to 0x%02x\n", old_4062, PageR[0x4062 >> 13][0x4062]);
            old_4062 = PageR[0x4062 >> 13][0x4062];
          }
      }

    if (reg_pc < 0x2000)
      {
        fprintf(stderr, "PC in I/O area [0x%04x] referer = 0x%04x\n", reg_pc, old_reg_pc);
        Log("PC in I/O area [0x%04x] referer = 0x%04x\n", reg_pc, old_reg_pc);
      }
/*
	if ((reg_pc >= 0x2000) && (reg_pc < 0x4000))
	  {
		 fprintf(stderr, "PC in RAM [0x%04x] *PC = 0x%02x referer = 0x%04x\n", reg_pc, Page[reg_pc>>13][reg_pc], old_reg_pc);
		 Log("PC in RAM [0x%04x] *PC = 0x%02x referer = 0x%04x\n", reg_pc, Page[reg_pc>>13][reg_pc], old_reg_pc);
	  }
*/
    if (err)
      {
        dump_pce_core();
		 /*
		 int i;
		 fprintf(stderr,"Err was set, PC = 0x04%x\n", reg_pc);
		 for (i = -15; i < 15; i++)
		   {
			  fprintf(stderr,"%02x ", Page[(reg_pc+i)>>13][reg_pc+i]);
		   }
		 */
      }
#endif

    // err = (*optable[Page[reg_pc>>13][reg_pc]].func_exe)();

    // TIMER stuff:
//  if (tiq_flag) {
//     tiq_flag = 0;
//     if (!(reg_p & FL_I)) int_tiq();
//  }

    // HSYNC stuff - count cycles:
    if (cycles > Cycles_per_Line)
      {

#ifdef BENCHMARK
        countScan++;
        if ((countScan & 0xFFFF) == 0) {
          double currentTime = osd_getTime();
          printf("%f\n", currentTime - lastTime);
          lastTime = currentTime;
          countNb--;
          if (countNb == 0) return;
        }
#endif

/*
      Log("Horizontal sync, cycles = %d, cycleNew = %d\n",
          cycles,
          CycleNew);
*/
        CycleNew += cycles;
        cycles -= Cycles_per_Line;
        // scanline++;

        // Log("Calling periodic handler\n");
        I = Loop6502 ();	/* Call the periodic handler */
        // _ICount += _IPeriod;
        /* Reset the cycle counter */
        cycles = 0;

        // Log("Requested interrupt is %d\n",I);

        if (I == INT_QUIT)
          {
#if !defined(FINAL_RELEASE)
            fprintf(stderr,"Normal exit of the cpu loop (INT_QUIT interruption caught)\n");
#endif
            return;	/* Exit if INT_QUIT     */
          }
        if (I)
          Int6502 (I);	/* Interrupt if needed  */

        if ((unsigned int) (CycleNew - cyclecountold) >
            (unsigned int) TimerPeriod * 2)

          cyclecountold = CycleNew;

      }
    else
      {

        if (CycleNew - cyclecountold >= TimerPeriod)
          {
            cyclecountold += TimerPeriod;
            I = TimerInt ();
            if (I)
              Int6502 (I);
          }

      }

  }
#if !defined(FINAL_RELEASE)
  fprintf(stderr,"Abnormal exit from the cpu loop\n");
#endif
  Log("Abnormal exit from the cpu loop\n");
}
