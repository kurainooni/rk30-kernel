cmd_arch/arm/common/fiq_glue.o := ../prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-gcc -Wp,-MD,arch/arm/common/.fiq_glue.o.d  -nostdinc -isystem ../prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/../lib/gcc/arm-eabi/4.4.3/include -I/media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-rk30/include -Iarch/arm/plat-rk/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float        -c -o arch/arm/common/fiq_glue.o arch/arm/common/fiq_glue.S

source_arch/arm/common/fiq_glue.o := arch/arm/common/fiq_glue.S

deps_arch/arm/common/fiq_glue.o := \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/linkage.h \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/smp.h) \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/arm/thumb.h) \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/hwcap.h \
  /media/florian/android_compile_/Airis_3066_411_TRA/kernel/arch/arm/include/asm/domain.h \
    $(wildcard include/config/io/36.h) \
    $(wildcard include/config/cpu/use/domains.h) \

arch/arm/common/fiq_glue.o: $(deps_arch/arm/common/fiq_glue.o)

$(deps_arch/arm/common/fiq_glue.o):
