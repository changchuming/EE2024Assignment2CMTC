################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: EE2024Assignment2CMTC.axf

# Tool invocations
EE2024Assignment2CMTC.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_CMSISv1p30_LPC17xx\Debug" -L"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_EaBaseBoard\Debug" -L"C:\Users\Chu Ming\Dropbox\Work\EE2024\Non-IVLE\workspace\Lib_MCU\Debug" -Xlinker --gc-sections -Xlinker -Map=EE2024Assignment2CMTC.map -mcpu=cortex-m3 -mthumb -T "demo_Debug.ld" -o "EE2024Assignment2CMTC.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) EE2024Assignment2CMTC.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size EE2024Assignment2CMTC.axf; # arm-none-eabi-objdump -h -S EE2024Assignment2CMTC.axf >EE2024Assignment2CMTC.lss
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
