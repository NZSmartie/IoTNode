#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component_common.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

# Allows the build 
COMPONENT_SUBMODULES += cn-cbor

# The Source dirs have been adopted from lobaro-coap/CMakeLists.txt
COMPONENT_SRCDIRS := cn-cbor/src

# Helps to namespace the include path. I like my #include's to be easy to follow
COMPONENT_ADD_INCLUDEDIRS := cn-cbor/include

# Let the library know to use 
# Use C99 standard, and disable warnings that prevent compilation of 3rd party library (Yeah i know....)
CFLAGS += -std=gnu99