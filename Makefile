# Project Name
TARGET = BlackBox

# Sources
CPP_SOURCES = BlackBox.cpp \
              hw.cpp \
              screen.cpp \
              processing.cpp

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

