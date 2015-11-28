######################################################################
#  librpi2/piclk/Makefile
######################################################################

include ../Makefile.incl

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
