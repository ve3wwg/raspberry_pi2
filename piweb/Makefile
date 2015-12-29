# Makefile for piweb
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1

include ../Makefile.incl

.PHONY:	libevent cores clean clobber

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
