/****************************************************************************
 h6280.h
 Function protoypes for simulated execution routines
 ****************************************************************************/

#ifndef H6280_H_
#define H6280_H_

#include "cleantyp.h"

/********************************************/
/* function parameters:                     */
/* --------------------                     */
/* - address (16-bit unsigned),             */
/* - pointer to buffer @ program counter    */
/********************************************/

extern void exe_instruct(void);
extern void exe_go(void);

extern UInt32 adc_abs(void);
extern UInt32 adc_absx(void);
extern UInt32 adc_absy(void);
extern UInt32 adc_imm(void);
extern UInt32 adc_zp(void);
extern UInt32 adc_zpind(void);
extern UInt32 adc_zpindx(void);
extern UInt32 adc_zpindy(void);
extern UInt32 adc_zpx(void);
extern UInt32 and_abs(void);
extern UInt32 and_absx(void);
extern UInt32 and_absy(void);
extern UInt32 and_imm(void);
extern UInt32 and_zp(void);
extern UInt32 and_zpind(void);
extern UInt32 and_zpindx(void);
extern UInt32 and_zpindy(void);
extern UInt32 and_zpx(void);
extern UInt32 asl_a(void);
extern UInt32 asl_abs(void);
extern UInt32 asl_absx(void);
extern UInt32 asl_zp(void);
extern UInt32 asl_zpx(void);
extern UInt32 bbr0(void);
extern UInt32 bbr1(void);
extern UInt32 bbr2(void);
extern UInt32 bbr3(void);
extern UInt32 bbr4(void);
extern UInt32 bbr5(void);
extern UInt32 bbr6(void);
extern UInt32 bbr7(void);
extern UInt32 bbs0(void);
extern UInt32 bbs1(void);
extern UInt32 bbs2(void);
extern UInt32 bbs3(void);
extern UInt32 bbs4(void);
extern UInt32 bbs5(void);
extern UInt32 bbs6(void);
extern UInt32 bbs7(void);
extern UInt32 bcc(void);
extern UInt32 bcs(void);
extern UInt32 beq(void);
extern UInt32 bit_abs(void);
extern UInt32 bit_absx(void);
extern UInt32 bit_imm(void);
extern UInt32 bit_zp(void);
extern UInt32 bit_zpx(void);
extern UInt32 bmi(void);
extern UInt32 bne(void);
extern UInt32 bpl(void);
extern UInt32 bra(void);
extern UInt32 brek(void);
extern UInt32 bsr(void);
extern UInt32 bvc(void);
extern UInt32 bvs(void);
extern UInt32 cla(void);
extern UInt32 clc(void);
extern UInt32 cld(void);
extern UInt32 cli(void);
extern UInt32 clv(void);
extern UInt32 clx(void);
extern UInt32 cly(void);
extern UInt32 cmp_abs(void);
extern UInt32 cmp_absx(void);
extern UInt32 cmp_absy(void);
extern UInt32 cmp_imm(void);
extern UInt32 cmp_zp(void);
extern UInt32 cmp_zpind(void);
extern UInt32 cmp_zpindx(void);
extern UInt32 cmp_zpindy(void);
extern UInt32 cmp_zpx(void);
extern UInt32 cpx_abs(void);
extern UInt32 cpx_imm(void);
extern UInt32 cpx_zp(void);
extern UInt32 cpy_abs(void);
extern UInt32 cpy_imm(void);
extern UInt32 cpy_zp(void);
extern UInt32 csh(void);
extern UInt32 csl(void);
extern UInt32 dec_a(void);
extern UInt32 dec_abs(void);
extern UInt32 dec_absx(void);
extern UInt32 dec_zp(void);
extern UInt32 dec_zpx(void);
extern UInt32 dex(void);
extern UInt32 dey(void);
extern UInt32 eor_abs(void);
extern UInt32 eor_absx(void);
extern UInt32 eor_absy(void);
extern UInt32 eor_imm(void);
extern UInt32 eor_zp(void);
extern UInt32 eor_zpind(void);
extern UInt32 eor_zpindx(void);
extern UInt32 eor_zpindy(void);
extern UInt32 eor_zpx(void);
extern UInt32 halt(void);
extern UInt32 inc_a(void);
extern UInt32 inc_abs(void);
extern UInt32 inc_absx(void);
extern UInt32 inc_zp(void);
extern UInt32 inc_zpx(void);
extern UInt32 inx(void);
extern UInt32 iny(void);
extern UInt32 jmp(void);
extern UInt32 jmp_absind(void);
extern UInt32 jmp_absindx(void);
extern UInt32 jsr(void);
extern UInt32 lda_abs(void);
extern UInt32 lda_absx(void);
extern UInt32 lda_absy(void);
extern UInt32 lda_imm(void);
extern UInt32 lda_zp(void);
extern UInt32 lda_zpind(void);
extern UInt32 lda_zpindx(void);
extern UInt32 lda_zpindy(void);
extern UInt32 lda_zpx(void);
extern UInt32 ldx_abs(void);
extern UInt32 ldx_absy(void);
extern UInt32 ldx_imm(void);
extern UInt32 ldx_zp(void);
extern UInt32 ldx_zpy(void);
extern UInt32 ldy_abs(void);
extern UInt32 ldy_absx(void);
extern UInt32 ldy_imm(void);
extern UInt32 ldy_zp(void);
extern UInt32 ldy_zpx(void);
extern UInt32 lsr_a(void);
extern UInt32 lsr_abs(void);
extern UInt32 lsr_absx(void);
extern UInt32 lsr_zp(void);
extern UInt32 lsr_zpx(void);
extern UInt32 nop(void);
extern UInt32 ora_abs(void);
extern UInt32 ora_absx(void);
extern UInt32 ora_absy(void);
extern UInt32 ora_imm(void);
extern UInt32 ora_zp(void);
extern UInt32 ora_zpind(void);
extern UInt32 ora_zpindx(void);
extern UInt32 ora_zpindy(void);
extern UInt32 ora_zpx(void);
extern UInt32 pha(void);
extern UInt32 php(void);
extern UInt32 phx(void);
extern UInt32 phy(void);
extern UInt32 pla(void);
extern UInt32 plp(void);
extern UInt32 plx(void);
extern UInt32 ply(void);
extern UInt32 rmb0(void);
extern UInt32 rmb1(void);
extern UInt32 rmb2(void);
extern UInt32 rmb3(void);
extern UInt32 rmb4(void);
extern UInt32 rmb5(void);
extern UInt32 rmb6(void);
extern UInt32 rmb7(void);
extern UInt32 rol_a(void);
extern UInt32 rol_abs(void);
extern UInt32 rol_absx(void);
extern UInt32 rol_zp(void);
extern UInt32 rol_zpx(void);
extern UInt32 ror_a(void);
extern UInt32 ror_abs(void);
extern UInt32 ror_absx(void);
extern UInt32 ror_zp(void);
extern UInt32 ror_zpx(void);
extern UInt32 rti(void);
extern UInt32 rts(void);
extern UInt32 sax(void);
extern UInt32 say(void);
extern UInt32 sbc_abs(void);
extern UInt32 sbc_absx(void);
extern UInt32 sbc_absy(void);
extern UInt32 sbc_imm(void);
extern UInt32 sbc_zp(void);
extern UInt32 sbc_zpind(void);
extern UInt32 sbc_zpindx(void);
extern UInt32 sbc_zpindy(void);
extern UInt32 sbc_zpx(void);
extern UInt32 sec(void);
extern UInt32 sed(void);
extern UInt32 sei(void);
extern UInt32 set(void);
extern UInt32 smb0(void);
extern UInt32 smb1(void);
extern UInt32 smb2(void);
extern UInt32 smb3(void);
extern UInt32 smb4(void);
extern UInt32 smb5(void);
extern UInt32 smb6(void);
extern UInt32 smb7(void);
extern UInt32 st0(void);
extern UInt32 st1(void);
extern UInt32 st2(void);
extern UInt32 sta_abs(void);
extern UInt32 sta_absx(void);
extern UInt32 sta_absy(void);
extern UInt32 sta_zp(void);
extern UInt32 sta_zpind(void);
extern UInt32 sta_zpindx(void);
extern UInt32 sta_zpindy(void);
extern UInt32 sta_zpx(void);
extern UInt32 stx_abs(void);
extern UInt32 stx_zp(void);
extern UInt32 stx_zpy(void);
extern UInt32 sty_abs(void);
extern UInt32 sty_zp(void);
extern UInt32 sty_zpx(void);
extern UInt32 stz_abs(void);
extern UInt32 stz_absx(void);
extern UInt32 stz_zp(void);
extern UInt32 stz_zpx(void);
extern UInt32 sxy(void);
extern UInt32 tai(void);
extern UInt32 tam(void);
extern UInt32 tax(void);
extern UInt32 tay(void);
extern UInt32 tdd(void);
extern UInt32 tia(void);
extern UInt32 tii(void);
extern UInt32 tin(void);
extern UInt32 tma(void);
extern UInt32 trb_abs(void);
extern UInt32 trb_zp(void);
extern UInt32 tsb_abs(void);
extern UInt32 tsb_zp(void);
extern UInt32 tstins_abs(void);
extern UInt32 tstins_absx(void);
extern UInt32 tstins_zp(void);
extern UInt32 tstins_zpx(void);
extern UInt32 tsx(void);
extern UInt32 txa(void);
extern UInt32 txs(void);
extern UInt32 tya(void);

#define INT_NONE        0            /* No interrupt required      */
#define INT_IRQ         1            /* Standard IRQ interrupt     */
#define INT_NMI         2            /* Non-maskable interrupt     */
#define INT_QUIT        3            /* Exit the emulation         */
#define	INT_TIMER       4
#define	INT_IRQ2        8

#define	VEC_RESET	0xFFFE
#define	VEC_NMI		0xFFFC
#define	VEC_TIMER	0xFFFA
#define	VEC_IRQ		0xFFF8
#define	VEC_IRQ2	0xFFF6
#define	VEC_BRK		0xFFF6

extern UChar flnz_list[256];

extern int Cycles_per_Line;

UChar imm_operand(UInt16 addr);
UChar get_8bit_zp(UChar zp_addr);
UInt16 get_16bit_zp(UChar zp_addr);
void put_8bit_zp(UChar zp_addr, UChar byte);

#undef INLINED_ACCESSORS
#if !defined(INLINED_ACCESSORS)
#define get_8bit_addr(addr) Rd6502((addr))
#define put_8bit_addr(addr,byte) Wr6502((addr),(byte))
#define get_16bit_addr(addr) (Rd6502(addr) + (Rd6502(addr + 1) << 8))
#else
inline UChar get_8bit_addr(UInt16 addr);
inline void put_8bit_addr(UInt16 addr, UChar byte);
inline UInt16 get_16bit_addr(UInt16 addr);
#endif

#endif
