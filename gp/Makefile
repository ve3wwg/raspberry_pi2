######################################################################
#  librpi2/gp/Makefile
######################################################################

include ../Makefile.incl

all:	gp
	
gp: gp.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) gp.o -o gp $(LDFLAGS)
	sudo chown root gp
	sudo chmod u+s gp

clean:
	rm -f *.o core.*

clobber: clean
	rm -f gp .errs.t

# End Makefile
