# IoT Node

(Name suggestions are welcome)

A base project IoT node for [Name of IoT project here]. Targeting ESP8266 modules using [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos).

## Goals

 - Implement a lightweight protocol for accessing node data 
   + *(temperature, humidity, etc...)*
 - Secure by default
   + *None of that pesky* ***"default password"*** *crap*
 - Easy to configure 

 ## Requirements

  - [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) - for compiling the source files
  - [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) - FreeRTOS
  - [mbed TLS](https://github.com/ARMmbed/mbedtls) A very nice TLS library for embedded applications
    + You'll need to clone the repository inside of `esp-open-rtos/extras/mbedtls` as the rtos includes optimal configuration for compiling mbedtls
    
       ```bash
       $ git clone https://github.com/ARMmbed/mbedtls esp-open-rtos/extras/mbedtls/mbedtls```

## Setup

Modify `Makefile` so it corrently references `common.mk` from `esp-open-rtos`

Ensure PATH includes the xtensa gcc compiler's `/bin` folder

Simply running `make` in this directory should build everyhting without any problems     