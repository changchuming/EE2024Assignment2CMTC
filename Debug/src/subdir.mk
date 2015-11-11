################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cr_startup_lpc17.c \
../src/main.c \
../src/rgbfixed.c \
../src/task.c 

OBJS += \
./src/cr_startup_lpc17.o \
./src/main.o \
./src/rgbfixed.o \
./src/task.o 

C_DEPS += \
./src/cr_startup_lpc17.d \
./src/main.d \
./src/rgbfixed.d \
./src/task.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_EaBaseBoard\inc" -I"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -mcpu=cortex-m3 -mthumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


