# Project Name
TARGET = seed

USE_DAISYSP_LGPL = 1

# APP_TYPE = BOOT_SRAM

# Sources
CPP_SOURCES = seed.cpp

# Library Locations
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

#C_INCLUDES += \
#-I../eurorack \

# Include terrarium.h
C_INCLUDES += -I../Terrarium -I../RTNeural
CPPFLAGS += -DRTNEURAL_DEFAULT_ALIGNMENT=8 -DRTNEURAL_NO_DEBUG=1