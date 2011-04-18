

APPNAME=lpcjtag

##########################################
# !Change here!
####
BINPATH=/usr/local/bin/
COMPILER=${BINPATH}arm-thumb-elf-as
LINKER=${BINPATH}arm-thumb-elf-g++
CC=${BINPATH}arm-thumb-elf-g++
C=${BINPATH}arm-thumb-elf-gcc
OC=${BINPATH}arm-thumb-elf-objcopy
ASFLAGS = 
INCPATHS =  -I. 
	

LDFLAGS = -g -nostdlib -nostartfiles

CCFLAGS = $(INCPATHS)  \
        -mcpu=arm7tdmi -mtune=arm7tdmi \
        -nostdlib  -fno-builtin    \
	-fno-unwind-tables \
        -fno-exceptions -fno-common -mapcs\
        -fshort-enums -mstructure-size-boundary=32 \
        -fno-rtti -O2 -finline  -g -gdwarf-2 -fno-eliminate-unused-debug-types -c 


CFLAGS = $(INCPATHS)  \
        -mcpu=arm7tdmi -mtune=arm7tdmi \
        -nostdlib  -fno-builtin    \
	-fno-unwind-tables \
        -fno-exceptions -fno-common -mapcs\
        -fshort-enums -mstructure-size-boundary=32 \
        -finline -O2 -g -gdwarf-2 -fno-eliminate-unused-debug-types -c 



LIBS =  c gcc \


# add -l to libraries (if needed)
ifeq ($(findstring -l,$(LIBS)),)
        LIBS := $(patsubst %,-l%,$(LIBS))
endif



BUILDSTYLE= ${COMPILER}  ${ASFLAGS} $< -o$@ 
LINKSTYLE = ${LINKER}  ${LDFLAGS} ${LIBPATHS} -Tld1.ld -o $(APPNAME).elf   ${filter-out %.a %.so, $^} ${LIBS} 

OBJCOPY = ${OC} -v -O ihex $(APPNAME).elf binary.hex
CCSTYLE=${CC} ${INCPATHS} ${CCFLAGS}  $< -o$@
CSTYLE=${C} ${INCPATHS} ${CFLAGS}  $< -o$@

########################################################################
# Making LPCJTAG
########################################################################


%.o : %.s
	${BUILDSTYLE}
%.o : %.S
	${BUILDSTYLE}
%.o : %.c
	${CSTYLE}
	
%.o : %.cpp
	${CCSTYLE}



OBJ =./crt0.o \
	./main.o \
	./Serial1.o \
	./uartStream.o \
	./syscalls.o \
	./cppreq.o \
	./jtag.o \
	./jtag_sync.o \
	./JTAG_arm9tdmi.o \
	./lpcusb/target/usbcontrol.o \
	./lpcusb/target/usbhw_lpc.o \
	./lpcusb/target/usbinit.o \
	./lpcusb/target/usbstdreq.o \
	./USBSerialStream.o \
	./gdbparser.o \
	./HexConversion.o \
	./Timer.o \
	./TargetManager.o\
	./WatchPointManager.o \
	./jtag_asm.o \
	./JTAG_arm.o \
	./JTAG_arm7tdmi.o \
	./JTAG_bf533.o \
	./LPCFlash.o \
	./debug_out.o \


LPCJTAG:${OBJ}
	${LINKSTYLE}
	${OBJCOPY}

	
	
.PHONY : tidy
tidy::
	@${RM} ${OBJ}
.PHONY : clean
clean:: tidy
	@${RM} AGBMeck





# DO NOT DELETE
