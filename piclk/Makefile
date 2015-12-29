######################################################################
# librpi2/piclk/Makefile
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	piclk
	
piclk: piclk.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) piclk.o -o piclk $(LDFLAGS)
	sudo chown root piclk
	sudo chmod u+s piclk

clean:
	rm -f *.o core.*

clobber: clean
	rm -f piclk .errs.t

# End Makefile
