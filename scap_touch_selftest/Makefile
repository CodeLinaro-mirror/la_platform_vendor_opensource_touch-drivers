
C_COMPILE := /home/hyn/wuchao/qv450/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
# arm-none-linux-gnueabi-gcc
TARG := selftest
OBJ := main.c
CFLG := -std=gnu99 -Wno-declaration-after-statement 

all:
	$(C_COMPILE) $(CFLG) $(OBJ) -o $(TARG) -static
	cp -f selftest /mnt/nfs/
