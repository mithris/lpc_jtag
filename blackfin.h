/*
	LPCJTAG, a GDB remote JTAG connector,

	Copyright (C) 2008 Bjorn Bosell (mithris@misantrop.org)

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	3. The name of the author may not be used to endorse or promote products
	   derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _BLACKFIN_H_
#define _BLACKFIN_H_


//###########################################################################
// TAP instructions.
#define BF_TAP_INSTR_IDCODE		0x02
#define BF_TAP_INSTR_DBGCTL		0x04
#define BF_TAP_INSTR_DBGSTAT	0x0c
#define BF_TAP_INSTR_EMUIR		0x08
#define BF_TAP_INSTR_EMUDAT		0x14
#define BF_TAP_INSTR_EMUPC		0x1e




//###########################################################################
// Register defines
#define BF_MAX_REGISTER   47
/*
8 R0-R7
6 P0-P5 (SP, FP) ?
2 A0.X, A0.W, A1.X, A1.W
4 I0-I3
4 M0-M3
4 B0-B3
4 L0-L3

1 ASTAT
1 SEQSTAT
1 SYSCFG
1 RETI
1 RETX
1 RETN
1 RETE
1 RETS
1 LC0
1 LT1
1 LB0
1 LB1
1 CYCLES
1 CYCLES2
1 EMUDAT
*/

//###########################################################################
// Debug control register

#define BF_DBGCTL_EMPWR		0x0001
#define BF_DBGCTL_EMFEN		0x0002
#define BF_DBGCTL_EMEEN		0x0004
#define BF_DBGCTL_EMPEN		0x0008
#define BF_DBGCTL_EMUIRSZ	0x0030
#define BF_DBGCTL_EMUIRLPSZ	0x0040
#define BF_DBGCTL_EMUDATZ	0x0180
#define BF_DBGCTL_ESSTEP	0x0200
#define BF_DBGCTL_SYSRST	0x0400
#define BF_DBGCTL_WAKEUP	0x0800
#define BF_DBGCTL_SRAM_INIT	0x1000


#define BF_DBGSTAT_EMUDOF                  0x0001
#define BF_DBGSTAT_EMUDIF                  0x0002
#define BF_DBGSTAT_EMUDOOVF                0x0004
#define BF_DBGSTAT_EMUDIOVF                0x0008
#define BF_DBGSTAT_EMUREADY                0x0010
#define BF_DBGSTAT_EMUACK                  0x0020
#define BF_DBGSTAT_EMUCAUSE                0x03C0
#define BF_DBGSTAT_BIST_DONE               0x0400
#define BF_DBGSTAT_LPDEC0                  0x0800
#define BF_DBGSTAT_IN_RESET                0x1000
#define BF_DBGSTAT_IDLE                    0x2000
#define BF_DBGSTAT_CORE_FAULT              0x4000
#define BF_DBGSTAT_LPDEC1                  0x8000

#define BF_EMULATION_CAUSE(x) ((x >> 6) & 0xf)

#define BF_EMUCAUSE_EMUEXCPT               0x0
#define BF_EMUCAUSE_EMUIN                  0x1
#define BF_EMUCAUSE_WATCHPT                0x2
#define BF_EMUCAUSE_PERFMON0               0x4
#define BF_EMUCAUSE_PERFMON1               0x5
#define BF_EMUCAUSE_STEP                   0x8

//###########################################################################
// Instructions

#define BF_INSN_NOP		0x00000000
#define BF_INSN_RTE		0x00140000
#define BF_INSN_SSYNC	0x00240000

// Shamelessly stolen, please don't tell FSF
#define GROUP(x)    ((x >> 3) & 7)

#define T_REG_R       0x00
#define T_REG_P       (1 << 3)
#define T_REG_I       (2 << 3)
#define T_REG_M      ((2 << 3) + 4)
#define T_REG_B       (3 << 3)
#define T_REG_L      ((3 << 3) + 4)
#define T_REG_A       (4 << 3)
#define T_REG_LP      (6 << 3)
#define T_REG_X       (7 << 3)
#define T_NOGROUP     (8 << 3)   // all registers above this value don't
                                 // belong to a core register group

enum bfin_registers {
        REG_R0    = T_REG_R, REG_R1, REG_R2, REG_R3, REG_R4, REG_R5, REG_R6, REG_R7, 
        REG_P0    = T_REG_P, REG_P1, REG_P2, REG_P3, REG_P4, REG_P5, REG_SP, REG_FP,
        REG_I0    = T_REG_I, REG_I1, REG_I2, REG_I3,
        REG_M0    = T_REG_M, REG_M1, REG_M2, REG_M3, 
        REG_B0    = T_REG_B, REG_B1, REG_B2, REG_B3,
        REG_L0    = T_REG_L, REG_L1, REG_L2, REG_L3, 
        REG_A0x   = T_REG_A, REG_A0w, REG_A1x, REG_A1w,
        REG_ASTAT = T_REG_A + 6, REG_RETS,

        REG_LC0   = T_REG_LP, REG_LT0, REG_LB0,  REG_LC1, REG_LT1, REG_LB1,
                          REG_CYCLES, REG_CYCLES2,

        REG_USP   = T_REG_X, REG_SEQSTAT, REG_SYSCFG,
                          REG_RETI, REG_RETX, REG_RETN, REG_RETE, REG_EMUDAT,
};


//###########################################################################
// BF533 Memory map.



#define BF_SDRAM_START			0x00000000
#define BF_SDRAM_END			0x08000000
#define BF_ASYNC_B0_START		0x20000000
#define BF_ASYNC_B1_START		0x20100000
#define BF_ASYNC_B2_START		0x20200000
#define BF_ASYNC_B3_START		0x20300000

#define BF_L1_INSTR_SRAM_START	0xffa00000
#define BF_L1_INSTR_SRAM_END	0xffa14000

#define BF_L1_SCRATCH_START		0xffb00000
#define BF_L1_SCRATCH_END		0xffb01000 // Only 4k :(
#define BF_SYSTEM_MMR			0xffc00000
#define BF_CORE_MMR				0xffe00000

#define BF_SWRST				0xffc00100
#define BF_SYSCR				0xffc00104

//###########################################################################
// WPU registers/values.

#define BF_N_DATA_WATCHPOINT		2
#define BF_N_INSTRUCTION_WATCHPOINT	6
#define BF_N_WATCHPOINT				(BF_N_DATA_WATCHPOINT + BF_N_INSTRUCTION_WATCHPOINT)


#define BF_WPU_WPPWR		(1)
#define BF_WPU_WPIACTL		(0xffe07000)
#define BF_WPU_WPDACTL		(0xffe07100)
#define BF_WPU_WPDA0		(0xffe07140)
#define BF_WPU_WPDA1		(0xffe07144)
#define BF_WPU_WPSTAT		(0xffe07200)

#define BF_WPU_WPDACNT0		(0xffc07180)
#define BF_WPU_WPDACNT1		(0xffc07184)


#define BF_WPU_WPIA0		(0xffe07040)
#define BF_WPU_WPIA1		(0xffe07044)
#define BF_WPU_WPIA2		(0xffe07048)
#define BF_WPU_WPIA3		(0xffe0704C)
#define BF_WPU_WPIA4		(0xffe07050)
#define BF_WPU_WPIA5		(0xffe07054)

enum DWARF_REGS
{
  DWARF_REG_R0 = 0,
  DWARF_REG_R1,
  DWARF_REG_R2,
  DWARF_REG_R3,
  DWARF_REG_R4,
  DWARF_REG_R5,
  DWARF_REG_R6,
  DWARF_REG_R7,
  DWARF_REG_P0,
  DWARF_REG_P1,
  DWARF_REG_P2,
  DWARF_REG_P3,
  DWARF_REG_P4,
  DWARF_REG_P5,
  DWARF_REG_SP,
  DWARF_REG_FP,
  DWARF_REG_I0,
  DWARF_REG_I1,
  DWARF_REG_I2,
  DWARF_REG_I3,
  DWARF_REG_B0,
  DWARF_REG_B1,
  DWARF_REG_B2,
  DWARF_REG_B3,
  DWARF_REG_L0,
  DWARF_REG_L1,
  DWARF_REG_L2,
  DWARF_REG_L3,
  DWARF_REG_M0,
  DWARF_REG_M1,
  DWARF_REG_M2,
  DWARF_REG_M3,
  DWARF_REG_A0x,
  DWARF_REG_A1x,
  DWARF_REG_CC,
  DWARF_REG_RETS,
  DWARF_REG_RETI,
  DWARF_REG_RETX,
  DWARF_REG_RETN,
  DWARF_REG_RETE,
  DWARF_REG_ASTAT,
  DWARF_REG_SEQSTAT,
  DWARF_REG_USP,

	/* Additional registers*/
	DWARF_REG_A0w,
	DWARF_REG_A1w,
	DWARF_REG_LC0,
	DWARF_REG_LT0, 
	DWARF_REG_LB0,
	DWARF_REG_LC1,
	DWARF_REG_LT1, 
	DWARF_REG_LB1,
	DWARF_REG_CYCLES,
	DWARF_REG_CYCLES2,
	DWARF_REG_EMUDAT,
};

// MDMA SFRs (Atleast for BF533)
#define MDMA_D0_NEXT_DESC			(0xffc00e00)
#define MDMA_S0_NEXT_DESC			(0xffc00e40)
#define MDMA_D1_NEXT_DESC			(0xffc00e80)
#define MDMA_S1_NEXT_DESC			(0xffc00ec0)

#define MDMA_D0_STARTADDR			(0xffc00e04)
#define MDMA_S0_STARTADDR			(0xffc00e44)
#define MDMA_D1_STARTADDR			(0xffc00e84)
#define MDMA_S1_STARTADDR			(0xffc00ec4)

#define MDMA_D0_CONFIG				(0xffc00e08)
#define MDMA_S0_CONFIG				(0xffc00e48)
#define MDMA_D1_CONFIG				(0xffc00e88)
#define MDMA_S1_CONFIG				(0xffc00ec8)

#define MDMA_D0_X_COUNT				(0xffc00e10)
#define MDMA_S0_X_COUNT				(0xffc00e50)
#define MDMA_D1_X_COUNT				(0xffc00e90)
#define MDMA_S1_X_COUNT				(0xffc00ed0)

#define MDMA_D0_X_MODIFY			(0xffc00e14)
#define MDMA_S0_X_MODIFY			(0xffc00e54)
#define MDMA_D1_X_MODIFY			(0xffc00e94)
#define MDMA_S1_X_MODIFY			(0xffc00ed4)

#define MDMA_D0_Y_COUNT				(0xffc00e18)
#define MDMA_S0_y_COUNT				(0xffc00e58)
#define MDMA_D1_Y_COUNT				(0xffc00e98)
#define MDMA_S1_Y_COUNT				(0xffc00ed8)

#define MDMA_D0_Y_MODIFY			(0xffc00e1C)
#define MDMA_S0_Y_MODIFY			(0xffc00e5C)
#define MDMA_D1_Y_MODIFY			(0xffc00e9C)
#define MDMA_S1_Y_MODIFY			(0xffc00edC)

#define MDMA_D0_IRQ_STATUS			(0xffc00e28)
#define MDMA_S0_IRQ_STATUS			(0xffc00e68)
#define MDMA_D1_IRQ_STATUS			(0xffc00ea8)
#define MDMA_S1_IRQ_STATUS			(0xffc00ee8)



#endif // _BLACKFIN_H_
