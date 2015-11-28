######################################################################
#  kmodules/test
######################################################################

include ../../Makefile.incl

all:	trpidma
	
trpidma: trpidma.o $(TOPDIR)/lib/librpi2.a
	$(CXX) trpidma.o -o trpidma $(LDFLAGS)
	sudo chown root trpidma
	sudo chmod u+s trpidma

clean:
	rm -f *.o core.*

clobber: clean
	rm -f trpidma.o trpidma

$(TOPDIR)/lib/librpi2.a:
	$(MAKE) -C $(TOPDIR)/librpi2

# End Makefile
