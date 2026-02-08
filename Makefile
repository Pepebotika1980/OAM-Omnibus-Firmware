# Project Name
TARGET = OAM_Omnibus

# Sources
CPP_SOURCES = main.cpp time_machine_hardware.cpp

# Library Locations
# We assume the user has DaisyExamples cloned nearby or we point to a standard location.
# Based on the previous repo structure, it was:
# LIBDAISY_DIR = ../DaisyExamples/libDaisy/
# DAISYSP_DIR = ../DaisyExamples/DaisySP/
# We will use this relative path assumption for now, or standard install paths.
LIBDAISY_DIR = ../temp_time_machine/DaisyExamples/libDaisy
DAISYSP_DIR = ../temp_time_machine/DaisyExamples/DaisySP/

# Enable LGPL modules (Compressor)
USE_DAISYSP_LGPL = 1


# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
