# -*- Mode: text -*-
#==============================================================================
# FILE: diaglib.mak
#
# SERVICE: Diagnostics
#
# DESCRIPTION:
#
# Copyright 2012-2015 Qualcomm Technologies, Inc. All rights reserved.
# Qualcomm Technologies Proprietary and Confidential
#==============================================================================

default: all

RELPATHTOROOT = ../../../../..

TARGET = libdiag
MAKEFILETYPE = shared
MAKEFILENAME = diaglib.mak

C_SRC = \
   diag_lsm.c\
   diag_lsm_dci.c\
   diag_lsm_event.c\
   diag_lsm_log.c\
   diag_lsm_msg.c\
   diag_lsm_pkt.c\
   diagsvc_malloc.c\
   msg_arrays_i.c\
   ts_linux.c\
   diag_logger.c\
   diag_qshrink4_db_parser.c\

NEEDINC=\
   vendor/qcom/proprietary/diag/include\
   vendor/qcom/proprietary/common/inc\

NEEDLIB =\

CFLAGS += -m32
CFLAGS += -DFEATURE_LOG_STDOUT
LDFLAGS += -m32

BTBUILD=vendor/qcom/proprietary/test/ril/btbuild
include $(RELPATHTOROOT)/$(BTBUILD)/bt.driver.mki

-include ./${MAKEFILENAME}.depend.mki

#======================================================================
# Add Mannual dependencies and makefile overrides here
#======================================================================

#EOF
