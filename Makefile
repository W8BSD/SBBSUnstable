# smblib/Makefile

#########################################################################
# Makefile for Synchronet Message Base Library (SMBLIB)					#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id$

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version
XPDEV	=	..\xpdev\		# Path to Cross-platform wrappers

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

SRC_ROOT = ..
# Cross platform/compiler definitions
!include ..\build\Common.bmake	# defines clean and output directory rules

CFLAGS = $(CFLAGS) -I$(XPDEV)

.path.c = .;$(XPDEV)

# SBBS DLL Link Rule
$(SMBLIB): $(OBJS)
    @echo Creating $< ...
	$(QUIET)$(DELETE) $@
	$(QUIET)for %f in ($(OBJS)) do $(QUIET)tlib $@ +%f


