######################################################################
#  librpi2/pipwm/Makefile
######################################################################

include ../Makefile.incl

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
