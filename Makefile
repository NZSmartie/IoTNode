#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := IoTNode

CPPFLAGS += -D_GLIBCXX_USE_C99 # Work around for https://github.com/espressif/esp-idf/issues/1445
CXXFLAGS += -std=c++14 \
			-DLWIP_NETBUF_RECVINFO=1 \
			-fno-exceptions # we really don't want excceptions

include $(IDF_PATH)/make/project.mk
