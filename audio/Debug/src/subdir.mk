################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/ac97.c \
../src/ac97_demo.c \
../src/math_util.c \
../src/platform.c 

LD_SRCS += \
../src/lscript.ld 

OBJS += \
./src/ac97.o \
./src/ac97_demo.o \
./src/math_util.o \
./src/platform.o 

C_DEPS += \
./src/ac97.d \
./src/ac97_demo.d \
./src/math_util.d \
./src/platform.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo Building file: $<
	@echo Invoking: MicroBlaze gcc compiler
	mb-gcc -Wall -O0 -g3 -c -fmessage-length=0 -Wl,--no-relax -I../../audio_bsp/microblaze_0/include -mlittle-endian -mxl-barrel-shift -mxl-pattern-compare -mcpu=v8.40.a -mno-xl-soft-mul -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo Finished building: $<
	@echo ' '


