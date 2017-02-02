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

COMPONENT_EMBED_TXTFILES := iotnode.crt
COMPONENT_EMBED_TXTFILES += iotnode.key

SHELL := /bin/bash
SECRETS := ${COMPONENT_PATH}/secrets
SECRETS := $(shell cat ${SECRETS} | sed -r 's/^\#.*$$//g; s/^([^=]+)$$/ -D\1/m; s/^([^=]+=)(.*)$$/ -D\1"\2"/' | tr -d '\n')

CFLAGS += $(SECRETS)

#
# OpenSSL Certificate Generation
#
# Since this project uses DTLS, a certificate is needed to present to clients.
# You may optionally provide your own private key and certificate simply by nameing the files accordingly in the main/ project folder
#
# Inspired by https://gist.github.com/ab/4570034
#

OPENSSL := openssl
DAYS := -days 365
KEY_TYPE := rsa:2048

CERT_KEY_FILE := iotnode.key
CERT_FILE := iotnode.crt

# Since the key is generated along with the crt, no need to specifiy a target for that as well
$(COMPONENT_PATH)/$(CERT_FILE).txt.o: $(COMPONENT_PATH)/$(CERT_FILE)

$(COMPONENT_PATH)/$(CERT_FILE):
	@echo Creating a self-signed key certificate pair...
	$(OPENSSL) req -x509 -nodes -newkey $(KEY_TYPE) -keyout $(COMPONENT_PATH)/$(CERT_KEY_FILE) -out $(COMPONENT_PATH)/$(CERT_FILE) $(DAYS)
