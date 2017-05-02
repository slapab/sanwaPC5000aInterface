##
## This file is part of the libopencm3 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

###############################################################################
## Defines possible hardware and software versions
###############################################################################
INTERFACE_VER1 := 1# Sources created for first working hardware
INTERFACE_VER2 := 2# Sources created for second version of hwardware 
###############################################################################

## assign required version for compilation if not defined
ifndef USING_INTERFACE_VER
USING_INTERFACE_VER := $(INTERFACE_VER1)
endif

## pass macro USING_INTERFACE_VER and also all possible version to the preprocessor
DEFS += -DUSING_INTERFACE_VER=$(USING_INTERFACE_VER) \
		-DINTERFACE_VER1=$(INTERFACE_VER1) \
		-DINTERFACE_VER2=$(INTERFACE_VER2) 
###############################################################################

BINARY = app_binary

SRCS += main.c \
		usb_cdc_dev.c \
		bm_dmm_protocol.c \
		check_data_req.c \
		ir_interface.c \
		systick_local.c \
		bsp.c

OBJS += $(SRCS:.c=.o)

CSTD = -std=gnu99

OPENCM3_DIR=../libopencm3
LDSCRIPT = ./stm32f103c8t6_app.ld


## Version correctness checking
ifeq (,$(filter $(INTERFACE_VER1) $(INTERFACE_VER2), $(USING_INTERFACE_VER)))
$(error Assigned unsupported version = "$(USING_INTERFACE_VER)"!)
else
$(info Using version: "$(USING_INTERFACE_VER)")
endif

include ./libopencm3.target.mk

