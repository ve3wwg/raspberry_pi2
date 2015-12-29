######################################################################
# librpi2
#
# Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
# by Warren Gay VE3WWG
# LGPL2 V2.1
######################################################################

include ../Makefile.incl

.PHONY:	all clean clobber

OBJS	= matrix.o max7219.o piutils.o mailbox.o gpio.o mtop.o \
          dmamem.o dma.o logana.o vcdout.o
INCS	= matrix.hpp max7219.hpp piutils.hpp mailbox.hpp gpio.hpp \
          mtop.hpp dmamem.hpp dma.hpp logana.hpp vcdout.hpp

all:	../lib/librpi2.a

../lib/librpi2.a: $(OBJS)
	@rm -fr ../lib/librpi2.a
	@mkdir -p ../lib
	$(AR) cr ../lib/librpi2.a $(OBJS)

clean:
	rm -f *.o core.*

clobber: clean
	rm -fr ../lib
	rm -f .errs.t 

matrix.o: ../include/matrix.hpp ../include/piutils.hpp
max7219.o: ../include/max7219.hpp ../include/gpio.hpp ../include/piutils.hpp
piutils.o: ../include/piutils.hpp
gpio.o: ../include/gpio.hpp ../include/piutils.hpp
mtop.o:	../include/mtop.hpp ../include/matrix.hpp ../include/max7219.hpp ../include/gpio.hpp
logana.o: logana.cpp ../include/logana.hpp mailbox.o
vcdout.o: vcdout.cpp ../include/vcdout.hpp

# End Makefile
