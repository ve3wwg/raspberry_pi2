######################################################################
#  librpi2/pispy/Makefile
######################################################################

include ../Makefile.incl

all:	pispy
	
pispy: pispy.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) pispy.o -o pispy $(LDFLAGS)
	sudo chown root pispy
	sudo chmod u+s pispy

clean:
	rm -f *.o core.*

clobber: clean
	rm -f pispy .errs.t

# End Makefile
