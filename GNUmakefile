# GNUmakefile

#########################################################################
# Makefile for cross-platform development "wrappers" test				#
# For use with GNU make and GNU C Compiler or Borland C++ (Kylix 3)								
#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# usage: gmake [os=target_os] [bcc=1]											
#
#########################################################################

# $Id$

# Macros
ifndef DEBUG
ifndef RELEASE
DEBUG	:=	1
endif
endif

ifdef bcc
 CC		:=	bc++
 CCPRE	:=	bcc
 CFLAGS	+=	-q -w -D__unix__
else
 CC		:=	gcc
 CCPRE	:=	gcc
 CFLAGS	+=	-Wall -O
endif

SLASH	=	/
OFILE	=	o

ifndef os
 os		=	$(shell uname)
endif
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
#os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')

ifdef bcc
 ODIR	:=	bcc.$(os)
else
 ODIR	:=	gcc.$(os)
endif

DELETE	=	rm -fv

CFLAGS	+= -D_THREAD_SAFE
ifeq ($(os),freebsd)	# FreeBSD
 LFLAGS	:=	-pthread
else
 ifeq ($(os),openbsd)	# OpenBSD
  LFLAGS	:=	-pthread
 else
  ifeq ($(os),netbsd)	# NetBSD
  CFLAGS	+= -D__unix__ -D_NEED_SEM -I/usr/pkg/include
  LFLAGS	:=	-lpth -lpthread -L/usr/pkg/lib
  else
   ifeq ($(os),qnx)	# QNX
    LFLAGS	:= 
   else			# Linux / Other UNIX
    ifdef bcc
     LFLAGS	:=	libpthread.a
    else
     LFLAGS	:=	-lpthread
    endif
   endif
  endif
 endif
endif

ifdef DEBUG
 ifdef bcc
  CFLAGS	+=	-v -y
 else
  CFLAGS	+=	-g
 endif
 CFLAGS	+=	-D_DEBUG
 ODIR	:=	$(ODIR).debug
else # RELEASE
 ODIR	:=	$(ODIR).release
endif

include objects.mk		# defines $(OBJS)
include targets.mk		# defines all and clean targets

ifeq ($(os),netbsd)
 OBJS	+= $(ODIR)$(SLASH)sem.$(OFILE)
endif

# Implicit C Compile Rule
$(ODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -o $@ -c $<

# Create output directories
$(ODIR):
	mkdir $(ODIR)

# Executable Build Rule
$(WRAPTEST): $(ODIR)/wraptest.o $(OBJS)
	@echo Linking $@
	@$(CC) $(CFLAGS) -o $@ $(LFLAGS) $^

