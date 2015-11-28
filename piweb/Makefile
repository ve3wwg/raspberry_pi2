# Makefile for piweb

include ../Makefile.incl

LDFLAGS	= -lrpi2 -levent -lpthread

OBJS	= webmain.o worker.o piweb.o

piweb: 	libevent $(OBJS)
	$(CXX) $(OBJS) -o piweb $(LDFLAGS)
	sudo chown root piweb
	sudo chmod u+s piweb

libevent::
	@( dpkg -s libevent-dev >/dev/null 2>&1 \
	|| (echo "*** 'sudo apt-get install libevent-dev' before building piweb ***" && exit 1))

cores:	
	rm -f core core.* 

clean:	cores
	rm -f *.o

clobber: clean cores
	rm -f piweb

# End Makefile
