######################################################################
# librpi2/pipwm/Makefile
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	pipwm
	
pipwm: pipwm.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) pipwm.o -o pipwm $(LDFLAGS)
	sudo chown root pipwm
	sudo chmod u+s pipwm

clean:
	rm -f *.o core.*

clobber: clean
	rm -f pipwm .errs.t

# End Makefile
