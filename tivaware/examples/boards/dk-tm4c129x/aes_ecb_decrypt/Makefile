#******************************************************************************
#
# Makefile - Rules for building the AES128 and AES256 ECB Decryption Demo.
#
# Copyright (c) 2013-2016 Texas Instruments Incorporated.  All rights reserved.
# Software License Agreement
# 
# Texas Instruments (TI) is supplying this software for use solely and
# exclusively on TI's microcontroller products. The software is owned by
# TI and/or its suppliers, and is protected under applicable copyright
# laws. You may not combine this software with "viral" open-source
# software in order to form a larger program.
# 
# THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
# NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
# NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
# CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
# DAMAGES, FOR ANY REASON WHATSOEVER.
# 
# This is part of revision 2.1.3.156 of the DK-TM4C129X Firmware Package.
#
#******************************************************************************

#
# Defines the part type that this project uses.
#
PART=TM4C129XNCZAD

#
# The base directory for TivaWare.
#
ROOT=../../../..

#
# Include the common make definitions.
#
include ${ROOT}/makedefs

#
# Where to find source files that do not live in this directory.
#
VPATH=../drivers
VPATH+=../../../../utils

#
# Where to find header files that do not live in the source directory.
#
IPATH=..
IPATH+=../../../..

#
# The default rule, which causes the AES128 and AES256 ECB Decryption Demo to be built.
#
all: ${COMPILER}
all: ${COMPILER}/aes_ecb_decrypt.axf

#
# The rule to clean out all the build products.
#
clean:
	@rm -rf ${COMPILER} ${wildcard *~}

#
# The rule to create the target directory.
#
${COMPILER}:
	@mkdir -p ${COMPILER}

#
# Rules for building the AES128 and AES256 ECB Decryption Demo.
#
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/aes_ecb_decrypt.o
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/frame.o
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/kentec320x240x16_ssd2119.o
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/pinout.o
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/startup_${COMPILER}.o
${COMPILER}/aes_ecb_decrypt.axf: ${COMPILER}/uartstdio.o
${COMPILER}/aes_ecb_decrypt.axf: ${ROOT}/grlib/${COMPILER}/libgr.a
${COMPILER}/aes_ecb_decrypt.axf: ${ROOT}/driverlib/${COMPILER}/libdriver.a
${COMPILER}/aes_ecb_decrypt.axf: aes_ecb_decrypt.ld
SCATTERgcc_aes_ecb_decrypt=aes_ecb_decrypt.ld
ENTRY_aes_ecb_decrypt=ResetISR
CFLAGSgcc=-DTARGET_IS_TM4C129_RA0

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
