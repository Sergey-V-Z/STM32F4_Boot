################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/FreeModbus/modbus/functions/mbfunccoils.c \
../Drivers/FreeModbus/modbus/functions/mbfunccoils_m.c \
../Drivers/FreeModbus/modbus/functions/mbfuncdiag.c \
../Drivers/FreeModbus/modbus/functions/mbfuncdisc.c \
../Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.c \
../Drivers/FreeModbus/modbus/functions/mbfuncholding.c \
../Drivers/FreeModbus/modbus/functions/mbfuncholding_m.c \
../Drivers/FreeModbus/modbus/functions/mbfuncinput.c \
../Drivers/FreeModbus/modbus/functions/mbfuncinput_m.c \
../Drivers/FreeModbus/modbus/functions/mbfuncother.c \
../Drivers/FreeModbus/modbus/functions/mbutils.c 

C_DEPS += \
./Drivers/FreeModbus/modbus/functions/mbfunccoils.d \
./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.d \
./Drivers/FreeModbus/modbus/functions/mbfuncdiag.d \
./Drivers/FreeModbus/modbus/functions/mbfuncdisc.d \
./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.d \
./Drivers/FreeModbus/modbus/functions/mbfuncholding.d \
./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.d \
./Drivers/FreeModbus/modbus/functions/mbfuncinput.d \
./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.d \
./Drivers/FreeModbus/modbus/functions/mbfuncother.d \
./Drivers/FreeModbus/modbus/functions/mbutils.d 

OBJS += \
./Drivers/FreeModbus/modbus/functions/mbfunccoils.o \
./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.o \
./Drivers/FreeModbus/modbus/functions/mbfuncdiag.o \
./Drivers/FreeModbus/modbus/functions/mbfuncdisc.o \
./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.o \
./Drivers/FreeModbus/modbus/functions/mbfuncholding.o \
./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.o \
./Drivers/FreeModbus/modbus/functions/mbfuncinput.o \
./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.o \
./Drivers/FreeModbus/modbus/functions/mbfuncother.o \
./Drivers/FreeModbus/modbus/functions/mbutils.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/FreeModbus/modbus/functions/%.o Drivers/FreeModbus/modbus/functions/%.su Drivers/FreeModbus/modbus/functions/%.cyclo: ../Drivers/FreeModbus/modbus/functions/%.c Drivers/FreeModbus/modbus/functions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/ascii" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/functions" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/include" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/rtu" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/tcp" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/port" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/port/rtt" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-FreeModbus-2f-modbus-2f-functions

clean-Drivers-2f-FreeModbus-2f-modbus-2f-functions:
	-$(RM) ./Drivers/FreeModbus/modbus/functions/mbfunccoils.cyclo ./Drivers/FreeModbus/modbus/functions/mbfunccoils.d ./Drivers/FreeModbus/modbus/functions/mbfunccoils.o ./Drivers/FreeModbus/modbus/functions/mbfunccoils.su ./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.cyclo ./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.d ./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.o ./Drivers/FreeModbus/modbus/functions/mbfunccoils_m.su ./Drivers/FreeModbus/modbus/functions/mbfuncdiag.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncdiag.d ./Drivers/FreeModbus/modbus/functions/mbfuncdiag.o ./Drivers/FreeModbus/modbus/functions/mbfuncdiag.su ./Drivers/FreeModbus/modbus/functions/mbfuncdisc.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncdisc.d ./Drivers/FreeModbus/modbus/functions/mbfuncdisc.o ./Drivers/FreeModbus/modbus/functions/mbfuncdisc.su ./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.d ./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.o ./Drivers/FreeModbus/modbus/functions/mbfuncdisc_m.su ./Drivers/FreeModbus/modbus/functions/mbfuncholding.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncholding.d ./Drivers/FreeModbus/modbus/functions/mbfuncholding.o ./Drivers/FreeModbus/modbus/functions/mbfuncholding.su ./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.d ./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.o ./Drivers/FreeModbus/modbus/functions/mbfuncholding_m.su ./Drivers/FreeModbus/modbus/functions/mbfuncinput.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncinput.d ./Drivers/FreeModbus/modbus/functions/mbfuncinput.o ./Drivers/FreeModbus/modbus/functions/mbfuncinput.su ./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.d ./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.o ./Drivers/FreeModbus/modbus/functions/mbfuncinput_m.su ./Drivers/FreeModbus/modbus/functions/mbfuncother.cyclo ./Drivers/FreeModbus/modbus/functions/mbfuncother.d ./Drivers/FreeModbus/modbus/functions/mbfuncother.o ./Drivers/FreeModbus/modbus/functions/mbfuncother.su ./Drivers/FreeModbus/modbus/functions/mbutils.cyclo ./Drivers/FreeModbus/modbus/functions/mbutils.d ./Drivers/FreeModbus/modbus/functions/mbutils.o ./Drivers/FreeModbus/modbus/functions/mbutils.su

.PHONY: clean-Drivers-2f-FreeModbus-2f-modbus-2f-functions

