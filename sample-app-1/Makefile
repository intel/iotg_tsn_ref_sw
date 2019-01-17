EXTRA_CFLAGS+=-fstack-protector-strong -fPIE -fPIC -Wformat -Wformat-security
EXTRA_CFLAGS+=-O2 -D_FORTIFY_SOURCE=2
CFLAGS=-Wall $(EXTRA_CFLAGS)
LDFLAGS=-z noexecstack -z relro -z now -pie
LDLIBS=-lopen62541
PROGS=sample-app-1 opc-ua-simple-client

all: $(PROGS)

sample-app-1: sample-app-1.o

opc-ua-simple-client: opc-ua-simple-client.o

clean:
	$(RM) *~ $(addsuffix .o,$(PROGS)) $(PROGS)
