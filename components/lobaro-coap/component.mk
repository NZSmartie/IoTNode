#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component_common.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

# The Source dirs have been adopted from lobaro-coap/CMakeLists.txt
COMPONENT_SRCDIRS := lobaro-coap/interface/debug lobaro-coap/interface/mem lobaro-coap/interface/network lobaro-coap/interface lobaro-coap/option-types lobaro-coap

COMPONENT_ADD_INCLUDEDIRS := lobaro-coap

# Use C99 standard, and disable warnings that prevent compilation of 3rd party library (Yeah i know....)
CFLAGS += -std=c99 -Wno-enum-compare -Wno-format -Wno-format-extra-args -Wno-pointer-sign -Wno-unused-variable -Wno-unused-but-set-variable