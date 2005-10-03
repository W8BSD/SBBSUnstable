SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem$(OFILE)
endif
MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpevent$(OFILE)

CFLAGS	+=	-DSOUNDCARD_H_IN=$(shell if [ -f /usr/include/sys/soundcard.h ] ; then echo 1 ; elif [ -f /usr/include/soundcard.h ] ; then echo 2 ; elif [ -f /usr/include/linux/soundcard.h ] ; then echo 3 ; else echo 0 ; fi) -I. $(XPDEV_CFLAGS)
MT_CFLAGS	+=	$(XPDEV-MT_CFLAGS)

SHLIBOPTS   :=  -shared
ifeq ($(os),darwin)
 MKSHLIB        :=  libtool -dynamic -framework System -lcc_dynamic
 MKSHPPLIB      :=  libtool -dynamic -framework System -lcc_dynamic -lstdc++
 SHLIBOPTS  :=
else
 ifeq ($(os),sunos)
  MKSHLIB       :=  /usr/ccs/bin/ld -G
  MKSHPPLIB     :=  /usr/ccs/bin/ld -G
  SHLIBOPTS :=
 else
  MKSHLIB       :=  $(CC)
  MKSHPPLIB     :=  $(CXX)
 endif
endif

# Executable Build Rule
$(WRAPTEST): $(OBJODIR)/wraptest.o $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $^ $(XPDEV-MT_LIBS)

$(XPDEV_LIB_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV_SHLIB_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@

$(XPDEV-MT_LIB_BUILD): $(MTOBJODIR) $(MTOBJS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(MTOBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV-MT_SHLIB_BUILD): $(MTOBJODIR) $(MTOBJS)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MTOBJS) $(SHLIBOPTS) -o $@

