#ifndef _ARM_SINGLESTEP_H_
#define _ARM_SINGLESTEP_H_

// Forward declarations
class target;


class ArmSingleStep
{
	public:
		unsigned long nextAddress(target *context);

	private:
		unsigned long nextAddressThumb(target *);
		unsigned long nextAddressARM(target *);
		bool willExecute(unsigned long insn, unsigned long cpsr);

};


#endif // _ARM_SINGLESTEP_H_

