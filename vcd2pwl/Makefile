######################################################################
# librpi2/vcd2pwl/Makefile
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	vcd2pwl
	
vcd2pwl: vcd2pwl.o $(TOPDIR)/lib/librpi2.a
	$(CXX) vcd2pwl.o -o vcd2pwl $(LDFLAGS)
	sudo chown root vcd2pwl
	sudo chmod u+s vcd2pwl

clean:
	rm -f *.o core.*

clobber: clean
	rm -f vcd2pwl .errs.t

# End Makefile
