######################################################################
#  Mtop
######################################################################

include ../Makefile.incl

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
