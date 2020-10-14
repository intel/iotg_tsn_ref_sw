CC?=gcc

OPT=-O2 -g
EXTRA_CFLAGS += -fstack-protector-strong -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2 \
		-Wformat -Wformat-security -Wformat-overflow -Wno-parentheses \
		-Wno-missing-field-initializers -Wextra
CFLAGS = $(OPT) $(EXTRA_CFLAGS) -Wall

LDFLAGS += -z noexecstack -z relro -z now -pie
EXTRA_LDFLAGS += -pthread -lbpf -lelf

OPCUATSN_FLAGS += -lopen62541 -ljson-c
OPCUATSN_SRC := $(wildcard opcua-tsn/*.c)

.PHONY: all

all: tsq txrx-tsn opcua-server

tsq.o: tsq.c
	$(CC) $(CFLAGS) -c $< -o $@

tsq: tsq.o
	$(CC) $(LDFLAGS) $^ -o $@
	$(RM) $@.o

txrx%.o: txrx%.c txrx%.h
	$(CC) $(CFLAGS) $(EXTRA_LDFLAGS) -c $< -o $@

txrx-tsn: txrx.o txrx-afxdp.o txrx-afpkt.o
	$(CC) $(LDFLAGS) $^ $(EXTRA_LDFLAGS) -o txrx-tsn
	$(RM) $@.o $^

multicallback_server.o:
	$(CC) $(CFLAGS) $(EXTRA_LDFLAGS) $(OPCUATSN_FLAGS) -c $(OPCUATSN_SRC)

opcua-server: multicallback_server.o
	$(CC) $(LDFLAGS) $(EXTRA_LDFLAGS) $(OPCUATSN_FLAGS) $^ opcua*.o json*.o -o $@
	$(RM) $@.o $^ opcua_*.o json*.o

clean:
	$(RM) tsq txrx-tsn opcua-server *~ .[!.]*.un~ .[!.]*.o .[!.]*.a
	$(RM) `find . -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`