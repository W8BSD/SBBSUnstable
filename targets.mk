# targets.mk

# Make 'include file' defining targets for xpdel wrappers

# $Id$

# ODIR, DIRSEP, LIBFILE, EXEFILE, and DELETE must be pre-defined

WRAPTEST	= $(EXEODIR)$(DIRSEP)wraptest$(EXEFILE)

all: lib mtlib

lib:	$(OBJODIR) $(LIBODIR) $(XPDEV_LIB)

mtlib:	$(MTOBJODIR) $(LIBODIR) $(XPDEV-MT_LIB)
