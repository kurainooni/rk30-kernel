cmd_arch/arm/plat-rk/ddr_test.o := /media/florian/android_compile/rk/toolchains/arm-eabi-linaro-4.6.2/bin/arm-eabi-gcc -Wp,-MD,arch/arm/plat-rk/.ddr_test.o.d  -nostdinc -isystem /media/florian/android_compile/rk/toolchains/arm-eabi-linaro-4.6.2/bin/../lib/gcc/arm-eabi/4.6.2/include -I/media/florian/android_compile/kernel/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-rk30/include -Iarch/arm/plat-rk/include -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -Os -marm -fno-dwarf2-cfi-asm -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -Wframe-larger-than=1024 -fno-stack-protector -Wno-unused-but-set-variable -fomit-frame-pointer -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -DCC_HAVE_ASM_GOTO    -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(ddr_test)"  -D"KBUILD_MODNAME=KBUILD_STR(ddr_test)" -c -o arch/arm/plat-rk/ddr_test.o arch/arm/plat-rk/ddr_test.c

source_arch/arm/plat-rk/ddr_test.o := arch/arm/plat-rk/ddr_test.c

deps_arch/arm/plat-rk/ddr_test.o := \
    $(wildcard include/config/ddr/test.h) \
    $(wildcard include/config/ddr/freq.h) \

arch/arm/plat-rk/ddr_test.o: $(deps_arch/arm/plat-rk/ddr_test.o)

$(deps_arch/arm/plat-rk/ddr_test.o):
