######################################################################
# Mtop
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

all:	mtop burn
	
mtop:	mtopmain.o 
	$(CXX) mtopmain.o -o mtop $(LDFLAGS)
	sudo chown root mtop
	sudo chmod u+s mtop

burn:	burn.o
	$(CXX) burn.o -o burn -lm

clean:
	rm -f *.o core.*

clobber: clean
	rm -f mtop burn .errs.t

mtop.o:	mtop.hpp

# End Makefile
