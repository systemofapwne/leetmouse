obj-m += leetmouse.o
leetmouse-objs := usbmouse.o accel.o util.o libfixmath/build/libfixmath.o

depend:
	mkdir -p libfixmath/build
	cd libfixmath/build && cmake -DCMAKE_C_FLAGS="-DFIXMATH_NO_CTYPE" .. && make libfixmath
	cd libfixmath/build && ar x liblibfixmath.a
	cd libfixmath/build && ld -r *.c.o -o libfixmath.o_shipped
	touch libfixmath/build/.libfixmath.o.cmd
.PHONY : depend

clean:
	rm -rf libfixmath/build
.PHONY : clean

all: depend
	cp -n config.sample.h config.h
