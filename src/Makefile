OS     := $(shell uname -s)
CFLAGS := -Wall -Wextra
TARGET := prog

ifdef DEBUG
  CFLAGS += -O0 -g
else
  CFLAGS += -O2 #-DNDEBUG
  LDFLAGS += -s
endif

ifeq "$(OS)" "Linux"
  CFLAGS += -DHAVE_EPOLL
endif

all: $(TARGET)

.PHONY: clean

loop.o: loop.c loop.h

unix.o: unix.c unix.h

utils.o: utils.c utils.h

proctitle.o: proctitle.c proctitle.h

device.o: device.c device.h

$(TARGET): loop.o unix.o utils.o proctitle.o device.o

clean:
	rm -f $(TARGET) *.o

