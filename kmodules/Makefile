# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1

include ../Makefile.incl

.PHONY: all install clean clobber

all:
	@if [ ! -d "$(LINUX)" ] ; then \
		echo "Variable LINUX=$(LINUX), where there is" ; \
		echo "no directory." ; \
		echo " " ; \
	fi
	@if [ -d "$(LINUX)" ] ; then \
		$(MAKE) -$(MAKEFLAGS) -C $(LINUX) M=$$PWD/rpidma ; \
		$(MAKE) -$(MAKEFLAGS) -C ./test ; \
	else \
		echo "Set LINUX= to directory of kernel sources to" ; \
		echo "build rpidma.ko kernel module, which is " ; \
		echo "needed for pispy utility. (Skipping the" ; \
		echo "build of kernel module(s))." ; \
	fi

install:
	@if [ -f "$(TOPDIR)/kmodules/rpidma/rpidma.ko" ] ; then \
		sudo $(MAKE) -$(MAKEFLAGS) -C $(LINUX) M=$$PWD/rpidma modules_install ; \
		echo "------------------------------------------------------------" ; \
		echo "To automatically have rpidma.ko load at boot time, you" ; \
		echo "must edit /etc/modules and add the line:" ; \
		echo " " ; \
		echo "rpidma" ; \
		echo " " ; \
		echo "To the list (and save the file).  This will have Linux" ; \
		echo "load it each time the kernel boots. " ; \
		echo " " ; \
		echo "Alternatively, you can manually load it as follows:" ; \
		echo "" ; \
		echo "  # cd $$(cd $(TOPDIR)/kmodules/rpidma; pwd)" ; \
		echo "  # sudo insmod ./rpidma.ko" ; \
		echo "------------------------------------------------------------" ; \
	else \
		echo "No kernel modules installed." ; \
	fi

clean:

clobber:
	$(MAKE) -$(MAKEFLAGS) -C ./rpidma -f Makefile.aux clobber
