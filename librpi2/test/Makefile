######################################################################
#  librpi2/test/Makefile
######################################################################

include ../../Makefile.incl

all:	testinfo testclock testfifo
	
testinfo: testinfo.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) testinfo.o -o testinfo $(LDFLAGS)
	sudo chown root testinfo
	sudo chmod u+s testinfo

testclock: testclock.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) testclock.o -o testclock $(LDFLAGS)
	sudo chown root testclock
	sudo chmod u+s testclock

testfifo: testfifo.o $(TOPDIR)/librpi2/librpi2.a
	$(CXX) testfifo.o -o testfifo $(LDFLAGS)
	sudo chown root testfifo
	sudo chmod u+s testfifo

clean:
	rm -f *.o core.*

clobber: clean
	rm -f testclock testfifo testinfo .errs.t

# End Makefile
