################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/FreeModbus/modbus/rtu/mbcrc.c \
../Drivers/FreeModbus/modbus/rtu/mbrtu.c \
../Drivers/FreeModbus/modbus/rtu/mbrtu_m.c 

C_DEPS += \
./Drivers/FreeModbus/modbus/rtu/mbcrc.d \
./Drivers/FreeModbus/modbus/rtu/mbrtu.d \
./Drivers/FreeModbus/modbus/rtu/mbrtu_m.d 

OBJS += \
./Drivers/FreeModbus/modbus/rtu/mbcrc.o \
./Drivers/FreeModbus/modbus/rtu/mbrtu.o \
./Drivers/FreeModbus/modbus/rtu/mbrtu_m.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/FreeModbus/modbus/rtu/%.o Drivers/FreeModbus/modbus/rtu/%.su Drivers/FreeModbus/modbus/rtu/%.cyclo: ../Drivers/FreeModbus/modbus/rtu/%.c Drivers/FreeModbus/modbus/rtu/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/ascii" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/functions" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/include" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/rtu" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/tcp" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/port" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/modbus/port/rtt" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-FreeModbus-2f-modbus-2f-rtu

clean-Drivers-2f-FreeModbus-2f-modbus-2f-rtu:
	-$(RM) ./Drivers/FreeModbus/modbus/rtu/mbcrc.cyclo ./Drivers/FreeModbus/modbus/rtu/mbcrc.d ./Drivers/FreeModbus/modbus/rtu/mbcrc.o ./Drivers/FreeModbus/modbus/rtu/mbcrc.su ./Drivers/FreeModbus/modbus/rtu/mbrtu.cyclo ./Drivers/FreeModbus/modbus/rtu/mbrtu.d ./Drivers/FreeModbus/modbus/rtu/mbrtu.o ./Drivers/FreeModbus/modbus/rtu/mbrtu.su ./Drivers/FreeModbus/modbus/rtu/mbrtu_m.cyclo ./Drivers/FreeModbus/modbus/rtu/mbrtu_m.d ./Drivers/FreeModbus/modbus/rtu/mbrtu_m.o ./Drivers/FreeModbus/modbus/rtu/mbrtu_m.su

.PHONY: clean-Drivers-2f-FreeModbus-2f-modbus-2f-rtu

