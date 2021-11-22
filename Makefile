CXX:=arm-linux-gnueabihf-g++
LIBDIR:=/home/uidr3473/tools/arm-bcm2708/cross-pi-gcc-8.3.0-2/arm-linux-gnueabihf/libc

CFLAGS:=-O3 -Iglad/include
LDFLAGS:=-lEGL -l GLESv2 -lgbm -ldrm
LDFLAGS+=-Wl,-rpath-link=$(LIBDIR)/lib:$(LIBDIR)/usr/lib
OBJS:=glad/src/glad.o glad/src/glad_egl.o main.o
TARGET:=interpolation

all: $(TARGET)

.PHONY: clean

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
