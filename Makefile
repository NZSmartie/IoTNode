#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := IoTNode

CXXFLAGS += -DLWIP_NETBUF_RECVINFO=1

include $(IDF_PATH)/make/project.mk
