# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1

include Makefile.incl

PROJS	= gp pipwm piclk mtop vcd2pwl
SETUID	= gp pispy pipwm piclk mtop

.PHONY:	all checkgcc pispy pispy_clean pispy_clobber pispy_install clean clobber install uninstall

all:	checkgcc
	$(MAKE) -$(MAKEFLAGS) -C ./librpi2
	for proj in $(PROJS) ; do \
		$(MAKE) -$(MAKEFLAGS) -C ./$$proj ; \
	done
	-@$(MAKE) -$(MAKEFLAGS) -C ./piweb

checkgcc:
	@if [ $$(ls /usr/bin/g++-*|tail -1) \< "/usr/bin/g++-4.7" ] ; then \
		echo "------------------------------------" ; \
		echo "You need to install g++-4.7 or newer" ; \
		echo "------------------------------------" ; \
		exit 1 ; \
	fi

pispy:	all
	$(MAKE) -$(MAKEFLAGS) -C ./pispy
	LINUX=$(LINUX) $(MAKE) -$(MAKEFLAGS) -C ./kmodules

pispy_clean:
	$(MAKE) -$(MAKEFLAGS) -C ./pispy clean
	LINUX=$(LINUX) $(MAKE) -$(MAKEFLAGS) -C ./kmodules clean

pispy_clobber:
	$(MAKE) -$(MAKEFLAGS) -C ./pispy clobber
	LINUX=$(LINUX) $(MAKE) -$(MAKEFLAGS) -C ./kmodules clobber

pispy_install: pispy
	@if [ $$(id -u) -ne 0 ] ; then echo ">>> USE sudo make pispy_install TO INSTALL <<<" ; exit 1; fi
	$(MAKE) -C ./kmodules install
	install -p -oroot -groot -m111 $(TOPDIR)/pispy/pispy $(PREFIX)/bin/.
	chmod u+s $(PREFIX)/bin/pispy
	sync

clean:
	for proj in $(PROJS) ; do \
		$(MAKE) -C ./$$proj clean ; \
	done

clobber:
	$(MAKE) -C ./librpi2 clobber
	for proj in $(PROJS) ; do \
		$(MAKE) -C ./$$proj clobber ; \
	done
	$(MAKE) -C ./kmodules clobber
	$(MAKE) -C ./piweb clobber
	$(MAKE) -C ./pispy clobber

install: all
	@if [ $$(id -u) -ne 0 ] ; then echo ">>> USE sudo make install TO INSTALL <<<" ; exit 1; fi
	@echo "Installing to PREFIX=$(PREFIX) :"
	for proj in $(PROJS) ; do \
		install -p -oroot -groot -m111 $(TOPDIR)/$$proj/$$proj $(PREFIX)/bin/. ; \
	done
	for ex in $(SETUID) ; do \
		chmod u+s $(PREFIX)/bin/$$ex ; \
	done
	install -p -oroot -groot -m444 $(TOPDIR)/lib/librpi2.a $(PREFIX)/lib/.
	mkdir -p $(PREFIX)/include/librpi2
	chmod 755 $(PREFIX)/include/librpi2
	install -p -oroot -groot -m444 -t $(PREFIX)/include/librpi2 $(TOPDIR)/include/*.hpp $(TOPDIR)/include/*.h
	sync

uninstall:
	@if [ $$(id -u) -ne 0 ] ; then echo ">>> USE sudo make uninstall TO UNINSTALL <<<" ; exit 1; fi
	@echo "Uninstalling from PREFIX=$(PREFIX) :"
	rm -f $(PREFIX)/bin/gp
	rm -f $(PREFIX)/bin/pispy
	rm -f $(PREFIX)/bin/pipwm
	rm -f $(PREFIX)/bin/piclk
	rm -f $(PREFIX)/bin/mtop
	rm -f $(PREFIX)/lib/librpi2.a
	rm -fr $(PREFIX)/include/librpi2
	@echo "------------------------------------------------------------"
	@echo "You must remove rpidma.ko kernel module manually."
	@echo "  1) Remove rpidma from /etc/modules"
	@echo "  2) Remove file(s) found by:"
	@echo "     # sudo find /lib/modules -name rpidma.ko"
	@echo "------------------------------------------------------------"
	sync
