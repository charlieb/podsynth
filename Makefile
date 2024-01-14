# Project Name
TARGET = polysynth

# Sources
CPP_SOURCES = main.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
USE_DAISYSP_LGPL=1
CPP_STANDARD ?= -std=gnu++20
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

