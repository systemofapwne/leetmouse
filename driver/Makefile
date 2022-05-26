obj-m += leetmouse.o
leetmouse-objs := usbmouse.o accel.o util.o

ccflags-y += -mhard-float -mpreferred-stack-boundary=4

depend:
	mkdir -p libfixmath/build
	cd libfixmath/build && cmake .. && make libfixmath
.PHONY : depend

clean:
	rm -rf libfixmath/build
.PHONY : clean

all: depend
	cp -n config.sample.h config.h
