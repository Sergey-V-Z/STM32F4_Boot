################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/FreeModbus/functions/mbfunccoils.c \
../Drivers/FreeModbus/functions/mbfunccoils_m.c \
../Drivers/FreeModbus/functions/mbfuncdiag.c \
../Drivers/FreeModbus/functions/mbfuncdisc.c \
../Drivers/FreeModbus/functions/mbfuncdisc_m.c \
../Drivers/FreeModbus/functions/mbfuncholding.c \
../Drivers/FreeModbus/functions/mbfuncholding_m.c \
../Drivers/FreeModbus/functions/mbfuncinput.c \
../Drivers/FreeModbus/functions/mbfuncinput_m.c \
../Drivers/FreeModbus/functions/mbfuncother.c \
../Drivers/FreeModbus/functions/mbutils.c 

C_DEPS += \
./Drivers/FreeModbus/functions/mbfunccoils.d \
./Drivers/FreeModbus/functions/mbfunccoils_m.d \
./Drivers/FreeModbus/functions/mbfuncdiag.d \
./Drivers/FreeModbus/functions/mbfuncdisc.d \
./Drivers/FreeModbus/functions/mbfuncdisc_m.d \
./Drivers/FreeModbus/functions/mbfuncholding.d \
./Drivers/FreeModbus/functions/mbfuncholding_m.d \
./Drivers/FreeModbus/functions/mbfuncinput.d \
./Drivers/FreeModbus/functions/mbfuncinput_m.d \
./Drivers/FreeModbus/functions/mbfuncother.d \
./Drivers/FreeModbus/functions/mbutils.d 

OBJS += \
./Drivers/FreeModbus/functions/mbfunccoils.o \
./Drivers/FreeModbus/functions/mbfunccoils_m.o \
./Drivers/FreeModbus/functions/mbfuncdiag.o \
./Drivers/FreeModbus/functions/mbfuncdisc.o \
./Drivers/FreeModbus/functions/mbfuncdisc_m.o \
./Drivers/FreeModbus/functions/mbfuncholding.o \
./Drivers/FreeModbus/functions/mbfuncholding_m.o \
./Drivers/FreeModbus/functions/mbfuncinput.o \
./Drivers/FreeModbus/functions/mbfuncinput_m.o \
./Drivers/FreeModbus/functions/mbfuncother.o \
./Drivers/FreeModbus/functions/mbutils.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/FreeModbus/functions/%.o Drivers/FreeModbus/functions/%.su Drivers/FreeModbus/functions/%.cyclo: ../Drivers/FreeModbus/functions/%.c Drivers/FreeModbus/functions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/ascii" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/functions" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/include" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/rtu" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/tcp" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/port" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/app" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-FreeModbus-2f-functions

clean-Drivers-2f-FreeModbus-2f-functions:
	-$(RM) ./Drivers/FreeModbus/functions/mbfunccoils.cyclo ./Drivers/FreeModbus/functions/mbfunccoils.d ./Drivers/FreeModbus/functions/mbfunccoils.o ./Drivers/FreeModbus/functions/mbfunccoils.su ./Drivers/FreeModbus/functions/mbfunccoils_m.cyclo ./Drivers/FreeModbus/functions/mbfunccoils_m.d ./Drivers/FreeModbus/functions/mbfunccoils_m.o ./Drivers/FreeModbus/functions/mbfunccoils_m.su ./Drivers/FreeModbus/functions/mbfuncdiag.cyclo ./Drivers/FreeModbus/functions/mbfuncdiag.d ./Drivers/FreeModbus/functions/mbfuncdiag.o ./Drivers/FreeModbus/functions/mbfuncdiag.su ./Drivers/FreeModbus/functions/mbfuncdisc.cyclo ./Drivers/FreeModbus/functions/mbfuncdisc.d ./Drivers/FreeModbus/functions/mbfuncdisc.o ./Drivers/FreeModbus/functions/mbfuncdisc.su ./Drivers/FreeModbus/functions/mbfuncdisc_m.cyclo ./Drivers/FreeModbus/functions/mbfuncdisc_m.d ./Drivers/FreeModbus/functions/mbfuncdisc_m.o ./Drivers/FreeModbus/functions/mbfuncdisc_m.su ./Drivers/FreeModbus/functions/mbfuncholding.cyclo ./Drivers/FreeModbus/functions/mbfuncholding.d ./Drivers/FreeModbus/functions/mbfuncholding.o ./Drivers/FreeModbus/functions/mbfuncholding.su ./Drivers/FreeModbus/functions/mbfuncholding_m.cyclo ./Drivers/FreeModbus/functions/mbfuncholding_m.d ./Drivers/FreeModbus/functions/mbfuncholding_m.o ./Drivers/FreeModbus/functions/mbfuncholding_m.su ./Drivers/FreeModbus/functions/mbfuncinput.cyclo ./Drivers/FreeModbus/functions/mbfuncinput.d ./Drivers/FreeModbus/functions/mbfuncinput.o ./Drivers/FreeModbus/functions/mbfuncinput.su ./Drivers/FreeModbus/functions/mbfuncinput_m.cyclo ./Drivers/FreeModbus/functions/mbfuncinput_m.d ./Drivers/FreeModbus/functions/mbfuncinput_m.o ./Drivers/FreeModbus/functions/mbfuncinput_m.su ./Drivers/FreeModbus/functions/mbfuncother.cyclo ./Drivers/FreeModbus/functions/mbfuncother.d ./Drivers/FreeModbus/functions/mbfuncother.o ./Drivers/FreeModbus/functions/mbfuncother.su ./Drivers/FreeModbus/functions/mbutils.cyclo ./Drivers/FreeModbus/functions/mbutils.d ./Drivers/FreeModbus/functions/mbutils.o ./Drivers/FreeModbus/functions/mbutils.su

.PHONY: clean-Drivers-2f-FreeModbus-2f-functions

