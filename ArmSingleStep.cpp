#include "ArmSingleStep.h"
#include "target.h"
#include "ARM.h"


unsigned long 
ArmSingleStep::nextAddress(target *t)
{
	// Find out if we're talking thumb here.
	// And yes, we're assuming it's an ARM target.

	if (t->GetRegister(ARM_REG_CPSR) & ARM_CPSR_BIT_THUMB) { 
		return nextAddressThumb(t);

	}

	return nextAddressARM(t);
}

bool
ArmSingleStep::willExecute(unsigned long insn, unsigned long cpsr)
{

	switch((insn >> 28)) {
		case 0: // EQ
			// This is true if Z is set in CSPR
			if ((cpsr & (1 << 30)) != 0) return true;
			else return false;
		case 1: // NE
			// This should be true if Z is not set.
			if ((cpsr & (1 << 30)) == 0) return true;
			else return false;
		case 2:	// CS/HS
			//  this one should be true if C is set.
			if ((cpsr & (1 << 29)) != 0) return true;
			else return false;
		case 3:	// CC/LO
			//  this one should be true if C is clear.
			if ((cpsr & (1 << 29)) == 0) return true;
			else return false;
		case 4: // MI
			//  this one should be true if N is set
			if ((cpsr & (1 << 31)) != 0) return true;
			else return false;
		case 5: // PL
			//  this one should be true if N is clear.
			if ((cpsr & (1 << 31)) == 0) return true;
			else return false;
		case 6: // VS
			//  this one should be true if V is set
			if ((cpsr & (1 << 28)) != 0) return true;
			else return false;
		case 7:	// VC
			//  this one should be true if V is clear.
			if ((cpsr & (1 << 28)) == 0) return true;
			else return false;
		case 8: // HI
			// This is true if C and Z is clear
			if (((cpsr & (1 << 30)) == 0) && ((cpsr & (1 << 29)) == 0)) return true;
			else return false;
		case 9:	// LS
			// C clear OR Z set
			if (((cpsr & (1 << 29)) == 0) || ((cpsr & (1 << 30)) != 0)) return true;
			else return false;
		case 10: // GE
			// N set AND V set || N clear and V clear
			if ((cpsr & (1 << 31)) == (cpsr & (1 << 28))) return true;
			else return false;
		case 11: // LT
			// N != V
			if ((cpsr & (1 << 31)) != (cpsr & (1 << 28))) return true;
			else return false;
		case 12: // GT
			// Z == 0, N == V
			if (((cpsr & (1 << 30)) == 0) && ((cpsr & (1 << 31)) == (cpsr & (1 << 28)))) return true;
			else return false;
		case 13: // LE
			if (((cpsr & (1 << 30)) == 1) && ((cpsr & (1 << 31)) != (cpsr & (1 << 28)))) return true;
			else return false;
		case 14: // AL
			return true;
		default:
			break;
	}

	return false;
}


unsigned long
ArmSingleStep::nextAddressARM(target *t)
{

	return 0;
}



struct T_Shift_immed // Mark : 0x00
{
	unsigned short 	Rd : 3;
	unsigned short	Rm : 3;
	unsigned short 	Immed : 5;
	unsigned short	op : 2; // Special case here, op must not be 0b11
	unsigned short	mark : 3;
};

struct T_Add_Sub_Reg_immed // Mark : 0x06
{
	unsigned short	Rd : 3;
	unsigned short	Rn : 3;
	unsigned short	Rm : 3;
	unsigned short	op : 1;
	unsigned short	op1 : 1;
	unsigned short	mark : 5;

};

struct T_ascm_immed // Add/Sub/Cmp/Move immediate | Mark: 0x01
{
	unsigned short 	immed : 8;
	unsigned short 	Rd : 3;
	unsigned short 	op : 2;
	unsigned short 	mark : 3;

};

struct T_dpr // Mark : 0x10
{
	unsigned short Rd : 3;
	unsigned short Rm : 3;
	unsigned short op : 4;
	unsigned short mark : 6;
};

struct T_sdpr // Mark : 0x11
{
	unsigned short Rd : 3;
	unsigned short Rm : 3;
	unsigned short H1 : 1;
	unsigned short H2 : 1;
	unsigned short op : 2;
	unsigned short mark : 6;
};

struct T_bx // Mark : 0x47
{
	unsigned short sbz : 3;
	unsigned short Rm : 3;
	unsigned short H2 : 1;
	unsigned short L : 1;
	unsigned short mark : 8;
};

struct T_load_PC_rel // Mark: 0x09
{
	unsigned short offset : 8;
	unsigned short Rd : 3;
	unsigned short mark : 5;
};

struct T_load_store_reg_offset // Mark: 0x05
{
	unsigned short Rd : 3;
	unsigned short Rn : 3;
	unsigned short Rm : 3;
	unsigned short opcode : 3;
	unsigned short mark : 4;

};

struct T_load_store_WB_immed_offset // Mark: 0x03
{
	unsigned short Rd : 3;
	unsigned short Rn : 3;
	unsigned short immed : 5;
	unsigned short l : 1;
	unsigned short b : 1;
	unsigned short mark : 3;
};

struct T_load_store_HW_immed_offset // Mark : 0x08
{
	unsigned short Rd : 3;
	unsigned short Rn : 3;
	unsigned short immed : 5;
	unsigned short l : 1;
	unsigned short mark : 4;

};

struct T_load_store_stack // Mark : 0x09
{
	unsigned short immed : 8;
	unsigned short Rd : 3;
	unsigned short l : 1;
	unsigned short mark : 4;
};

struct T_add_sp_pc // Mark : 0x0a
{
	unsigned short immed : 8;
	unsigned short Rd : 3;
	unsigned short SP : 1;
	unsigned short mark : 4;
};

struct T_Misc // Mark : 0x0b
{
	unsigned short data : 12;
	unsigned short mark : 4;
};

struct T_lsdr // Mark : 0x0c
{
	unsigned short reglist : 8;
	unsigned short Rn : 3;
	unsigned short L : 1;
	unsigned short mark : 4;
};

struct T_cond_branch // Mark : 0x0d (cond != 0b1111 && cond != 0b1110)
{
			 short offset : 8;
	unsigned short cond : 4; // Condition is not allowed to be 0b1110 or 0b1111 or the instruction is identical to undef/swi
	unsigned short mark : 4;
};

struct T_undef // Mark : 0xde
{
	unsigned short data : 8;
	unsigned short mark : 8;
};

struct T_SWI // Mark : 0xdf
{
	unsigned short data : 8;
	unsigned short mark : 8;
};

struct T_BL // Mark : 0x1c (offset & 0x01 == 0)
{
	unsigned short offset : 11; // if bit0 is set, this is an undefined instruction.
	unsigned short mark : 5;
};

struct T_BLX_prefix // Mark : 0x1e
{
			 short offset : 11;
	unsigned short mark : 5;
};

struct T_BL_suffix // Mark 0x1f
{
	unsigned short offset : 11;
	unsigned short mark : 5;
};


typedef union {
	// Can change the PC.
	struct T_bx							BX;						// Mark : 0x47
	struct T_add_sp_pc					Add_SP_PC;				// Mark : 0x0a
	struct T_cond_branch				Cond_Branch;			// Mark : 0x0d (cond != 0b1111 && cond != 0b1110)
	struct T_SWI						SWI;					// Mark : 0xdf
	struct T_undef						Undef;					// Mark : 0xde
	struct T_BL							BL;						// Mark : 0x1c (offset & 0x01 == 0)
	struct T_BLX_prefix					BLX_Prefix;				// Mark : 0x1e
	struct T_BL_suffix					BL_Suffix;				// Mark : 0x1f


	struct T_Shift_immed 				Shift_Immed; 			// Mark : 0x00
	struct T_Add_Sub_Reg_immed 			Add_Sub_Reg_Immed; 		// Mark : 0x06
	struct T_ascm_immed					ASCM_Immed; 			// Add/Sub/Cmp/Move immediate | Mark: 0x01
	struct T_dpr						DPR;					// Mark : 0x10
	struct T_sdpr						SDPR;					// Mark : 0x11
	struct T_load_PC_rel				Load_PC_Rel;			// Mark: 0x09
	struct T_load_store_reg_offset		Load_Store_Reg_Offset;	// Mark: 0x05
	struct T_load_store_WB_immed_offset	Load_Store_WB_Immed_Ofs;// Mark: 0x03
	struct T_load_store_HW_immed_offset	Load_Store_HW_Immed_Ofs;// Mark : 0x08
	struct T_load_store_stack			Load_Store_Stack;		// Mark : 0x09
	struct T_Misc						Misc;					// Mark : 0x0b
	struct T_lsdr						LSDR;					// Mark : 0x0c

	unsigned short data;
} ThumbInsn;

unsigned long
ArmSingleStep::nextAddressThumb(target *t)
{

	ThumbInsn insn;
	unsigned long PC = t->GetRegister(t->GetSpecialRegister(target::eSpecialRegPC));
	

	t->ReadMem((char *)&insn, PC, 2);

	if (insn.Cond_Branch.mark == 0x0d && insn.Cond_Branch.cond != 0x0f && insn.Cond_Branch.cond != 0x0e) {
		// Conditional branch.
		PC += (insn.Cond_Branch.offset << 1);
		return PC;
	}


	if (insn.BX.mark == 0x47) {
		// Bx insttruction
		PC = t->GetRegister(insn.BX.Rm + (insn.BX.H2 << 3)) & ~1;
		return PC;
	}


	if (insn.SWI.mark == 0xdf) {
		// SWI
		return 0; // Exception
	}

	if (insn.Undef.mark == 0xde) {
		// Undef, can't go from here.
		return 0; // Exception
	}


	// BL/BLX Prefix instruction
	if (insn.BLX_Prefix.mark == 0x1e) {
		// Fetch next instruction.
		ThumbInsn insn2;
		t->ReadMem((char *)&insn2, PC + 2, 2);
		
		unsigned long result = (long)PC + insn.BLX_Prefix.offset << 12;
		if (insn2.BL_Suffix.mark & 0x02) {
			// BL
			result += ((insn2.data & 0x7ff) << 1);
		} else {
			// BLX
			result = (result + ((insn2.data & 0x7ff) << 1)) & 0xfffffffc;
		}
		return result;

	}

	if (insn.BL.mark == 0x1c) { // This is the BL suffix instruction
		// BL suffix
		return 0; // Treat as undefined?
	}

	if (insn.BL_Suffix.mark == 0x1f) {
		// BLX Suffix
		return 0; // Treat as undefined?
	}

	return 0;
}

