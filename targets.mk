# targets.mk

# Make 'include file' defining targets for Synchronet project

# $Id$

# LIBODIR, EXEODIR, DIRSEP, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBS		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)sbbs$(SOFILE)
FTPSRVR		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)ftpsrvr$(SOFILE)
WEBSRVR		= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)websrvr$(SOFILE)
MAILSRVR	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)mailsrvr$(SOFILE)
SERVICES	= $(LIBODIR)$(DIRSEP)$(LIBPREFIX)services$(SOFILE)
SBBSCON		= $(EXEODIR)$(DIRSEP)sbbs$(EXEFILE)
SBBSMONO	= $(EXEODIR)$(DIRSEP)sbbsmono$(EXEFILE)
JSEXEC		= $(EXEODIR)$(DIRSEP)jsexec$(EXEFILE)
NODE		= $(EXEODIR)$(DIRSEP)node$(EXEFILE)
BAJA		= $(EXEODIR)$(DIRSEP)baja$(EXEFILE)
FIXSMB		= $(EXEODIR)$(DIRSEP)fixsmb$(EXEFILE)
CHKSMB		= $(EXEODIR)$(DIRSEP)chksmb$(EXEFILE)
SMBUTIL		= $(EXEODIR)$(DIRSEP)smbutil$(EXEFILE)
SBBSECHO	= $(EXEODIR)$(DIRSEP)sbbsecho$(EXEFILE)
ECHOCFG		= $(EXEODIR)$(DIRSEP)echocfg$(EXEFILE)
ADDFILES	= $(EXEODIR)$(DIRSEP)addfiles$(EXEFILE)
FILELIST	= $(EXEODIR)$(DIRSEP)filelist$(EXEFILE)
MAKEUSER	= $(EXEODIR)$(DIRSEP)makeuser$(EXEFILE)
ANS2ASC		= $(EXEODIR)$(DIRSEP)ans2asc$(EXEFILE)
ASC2ANS		= $(EXEODIR)$(DIRSEP)asc2ans$(EXEFILE)
SEXYZ		= $(EXEODIR)$(DIRSEP)sexyz$(EXEFILE)

UTILS		= $(BUILD_DEPENDS)$(FIXSMB) $(BUILD_DEPENDS)$(CHKSMB) \
			  $(BUILD_DEPENDS)$(SMBUTIL) $(BUILD_DEPENDS)$(BAJA) $(BUILD_DEPENDS)$(NODE) \
			  $(BUILD_DEPENDS)$(SBBSECHO) $(BUILD_DEPENDS)$(ECHOCFG) $(BUILD_DEPENDS) \
			  $(BUILD_DEPENDS)$(ADDFILES) $(BUILD_DEPENDS)$(FILELIST) $(BUILD_DEPENDS)$(MAKEUSER) \
			  $(BUILD_DEPENDS)$(ANS2ASC) $(BUILD_DEPENDS)$(ASC2ANS) 

all:	dlls utils console mono

console:	xpdev-mt smblib \
		$(MTOBJODIR) $(LIBODIR) $(EXEODIR) \
		dlls \
		$(SBBSCON) $(JSEXEC)

utils:	smblib xpdev-mt xpdev ciolib-mt uifc-mt \
		$(LIBODIR) $(OBJODIR) $(MTOBJODIR) $(EXEODIR) \
		$(UTILS)

dlls:	smblib xpdev-mt \
		$(MTOBJODIR) $(LIBODIR) \
		$(SBBS) $(FTPSRVR) $(MAILSRVR) $(SERVICES)

mono:	xpdev-mt smblib \
		$(MTOBJODIR) $(EXEODIR) \
		$(SBBSMONO)


# Library dependencies
$(SBBS): 
$(FTPSRVR): 
$(WEBSRVR):
$(MAILSRVR):
$(SERVICES): 
$(SBBSCON): $(XPDEV-MT_LIB) $(SMBLIB)
$(SBBSMONO): $(XPDEV-MT_LIB) $(SMBLIB)
$(JSEXEC): $(XPDEV-MT_LIB) $(SMBLIB)
$(NODE): $(XPDEV_LIB)
$(BAJA): $(XPDEV_LIB) $(SMBLIB)
$(FIXSMB): $(XPDEV_LIB) $(SMBLIB)
$(CHKSMB): $(XPDEV_LIB) $(SMBLIB)
$(SMBUTIL): $(XPDEV_LIB) $(SMBLIB)
$(SBBSECHO): $(XPDEV_LIB) $(SMBLIB)
$(ECHOCFG): $(XPDEV-MT_LIB) $(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT)
$(ADDFILES): $(XPDEV_LIB)
$(FILELIST): $(XPDEV_LIB)
$(MAKEUSER): $(XPDEV_LIB)
$(ANS2ASC):
$(ASC2ANS):
$(SEXYZ): $(XPDEV_LIB) $(SMBLIB)
