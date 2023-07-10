CPPFLAGS=-I$(HOME)/usr/include
CFLAGS=-Wall -g3
CXXFLAGS=-std=c++17 -Wall -g3 -O0
LDFLAGS=
LIBS=

OBJS=AlarmClock.o FutureWaiter.o Socket.o ThreadPool.o demo-server.o

%.dep : %.cpp
	rm -f $@
	gcc $(CPPFLAGS) $(CXXFLAGS) -MM $< >$@~
	sed -e 's/\(.*\)\.o/\1\.o \1\.dep/' <$@~ >$@
	rm -f $@~

all: extend-cont

clean:
	rm *.dep *.o extend-cont

extend-cont: main.o $(OBJS)
	g++ $(LDFLAGS) -pthread -g3 main.o $(OBJS) $(LIBS) -o extend-cont

include AlarmClock.dep
include FutureWaiter.dep
include Socket.dep
include ThreadPool.dep
include main.dep
include demo-server.dep