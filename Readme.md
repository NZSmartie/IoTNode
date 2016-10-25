# IoT Node

(Name suggestions are welcome)

A base project IoT node for [Name of IoT project here]. Targeting ESP8266 modules using [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos).

## Goals

 - Implement a lightweight protocol for accessing node data 
   + [CoAP](http://coap.technology/) - Constrained Application Protocol
   + [MQTT](http://mqtt.org/) - A machine-to-machine (M2M)/"Internet of Things" connectivity protocol.
   + ...or any other suitable protocol that provides longer battery life 
 - Secure by default
   + None of that pesky "*default password*" crap
   + Take advantage of cerficates that are either geneated by users or provided by a Server/Hub  
 - Easy to configure 
   + A intuitive app on a mobile device is used to discover, adopt and configure nodes using (hopefully) Out-of-Band methods such as NFC

 ## Requirements

  - [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) - for compiling the source files
  - [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) - FreeRTOS
  - [mbed TLS](https://github.com/ARMmbed/mbedtls) A very nice TLS library for embedded applications
    + You'll need to clone the repository inside of `esp-open-rtos/extras/mbedtls` as the rtos includes optimal configuration for compiling mbedtls

## Setup

1. Modify `Makefile` so it corrently references `common.mk` from `esp-open-rtos`

2. Clone mbedtls into esp-open-rtos
   ```bash
   $ git clone https://github.com/ARMmbed/mbedtls esp-open-rtos/extras/mbedtls/mbedtls
   ```

3. Ensure PATH includes the xtensa gcc compiler's `/bin` folder

4. Running `make` in the project's root directory should build everyhting without any problems!

## TODO 

  - Clean up project structure
  - Add unit tests for TDD
  - Create HAL to support TDD
  - Experiment with CoAP or MQTT as protocols
    + CoAP is UDP and provides a familiar API to other RESTful applications (can be standalone)
    + MQTT is a Pub/Sub protocl designed to relay all information through a broker (Server instance)