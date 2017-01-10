#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

# Add additional subfolders to the build bath
COMPONENT_SRCDIRS += interfaces

SHELL := /bin/bash
SECRETS := ${COMPONENT_PATH}/secrets
SECRETS := $(shell cat ${SECRETS} | sed -r 's/^\#.*$$//g; s/^([^=]+)$$/ -D\1/m; s/^([^=]+=)(.*)$$/ -D\1"\2"/' | tr -d '\n')

CFLAGS += $(SECRETS)