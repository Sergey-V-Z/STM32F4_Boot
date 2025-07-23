################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/FreeModbus/port/portcritical.c \
../Drivers/FreeModbus/port/portevent.c \
../Drivers/FreeModbus/port/portevent_m.c \
../Drivers/FreeModbus/port/portserial.c \
../Drivers/FreeModbus/port/portserial_m.c \
../Drivers/FreeModbus/port/porttimer.c \
../Drivers/FreeModbus/port/porttimer_m.c 

C_DEPS += \
./Drivers/FreeModbus/port/portcritical.d \
./Drivers/FreeModbus/port/portevent.d \
./Drivers/FreeModbus/port/portevent_m.d \
./Drivers/FreeModbus/port/portserial.d \
./Drivers/FreeModbus/port/portserial_m.d \
./Drivers/FreeModbus/port/porttimer.d \
./Drivers/FreeModbus/port/porttimer_m.d 

OBJS += \
./Drivers/FreeModbus/port/portcritical.o \
./Drivers/FreeModbus/port/portevent.o \
./Drivers/FreeModbus/port/portevent_m.o \
./Drivers/FreeModbus/port/portserial.o \
./Drivers/FreeModbus/port/portserial_m.o \
./Drivers/FreeModbus/port/porttimer.o \
./Drivers/FreeModbus/port/porttimer_m.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/FreeModbus/port/%.o Drivers/FreeModbus/port/%.su Drivers/FreeModbus/port/%.cyclo: ../Drivers/FreeModbus/port/%.c Drivers/FreeModbus/port/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/ascii" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/functions" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/include" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/rtu" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/tcp" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/port" -I"D:/Radio/Jobe/Ethernet_Power_Load_Controller/firmware/Contrrol/Control_PWR/Drivers/FreeModbus/app" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-FreeModbus-2f-port

clean-Drivers-2f-FreeModbus-2f-port:
	-$(RM) ./Drivers/FreeModbus/port/portcritical.cyclo ./Drivers/FreeModbus/port/portcritical.d ./Drivers/FreeModbus/port/portcritical.o ./Drivers/FreeModbus/port/portcritical.su ./Drivers/FreeModbus/port/portevent.cyclo ./Drivers/FreeModbus/port/portevent.d ./Drivers/FreeModbus/port/portevent.o ./Drivers/FreeModbus/port/portevent.su ./Drivers/FreeModbus/port/portevent_m.cyclo ./Drivers/FreeModbus/port/portevent_m.d ./Drivers/FreeModbus/port/portevent_m.o ./Drivers/FreeModbus/port/portevent_m.su ./Drivers/FreeModbus/port/portserial.cyclo ./Drivers/FreeModbus/port/portserial.d ./Drivers/FreeModbus/port/portserial.o ./Drivers/FreeModbus/port/portserial.su ./Drivers/FreeModbus/port/portserial_m.cyclo ./Drivers/FreeModbus/port/portserial_m.d ./Drivers/FreeModbus/port/portserial_m.o ./Drivers/FreeModbus/port/portserial_m.su ./Drivers/FreeModbus/port/porttimer.cyclo ./Drivers/FreeModbus/port/porttimer.d ./Drivers/FreeModbus/port/porttimer.o ./Drivers/FreeModbus/port/porttimer.su ./Drivers/FreeModbus/port/porttimer_m.cyclo ./Drivers/FreeModbus/port/porttimer_m.d ./Drivers/FreeModbus/port/porttimer_m.o ./Drivers/FreeModbus/port/porttimer_m.su

.PHONY: clean-Drivers-2f-FreeModbus-2f-port

