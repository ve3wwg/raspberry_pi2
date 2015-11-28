######################################################################
#  librpi2/vcd2pwl/Makefile
######################################################################

include ../Makefile.incl

all:	vcd2pwl
	
vcd2pwl: vcd2pwl.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) vcd2pwl.o -o vcd2pwl $(LDFLAGS)
	sudo chown root vcd2pwl
	sudo chmod u+s vcd2pwl

clean:
	rm -f *.o core.*

clobber: clean
	rm -f vcd2pwl .errs.t

# End Makefile
