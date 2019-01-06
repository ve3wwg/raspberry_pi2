######################################################################
#  librpi2/gp/Makefile
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	gp
	
gp: gp.o $(TOPDIR)/lib/librpi2.a
	$(CXX) gp.o -o gp $(LDFLAGS)
	sudo chown root gp
	sudo chmod u+s gp

clean:
	rm -f *.o core.*

clobber: clean
	rm -f gp .errs.t

# End Makefile
