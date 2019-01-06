######################################################################
# librpi2/pispy/Makefile
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	pispy
	
pispy: pispy.o $(TOPDIR)/lib/librpi2.a
	$(CXX) pispy.o -o pispy $(LDFLAGS)
	sudo chown root pispy
	sudo chmod u+s pispy

clean:
	rm -f *.o core.*

clobber: clean
	rm -f pispy .errs.t .gtkwave.out captured.vcd

# End Makefile
