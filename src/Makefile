CC?=gcc

OPT=-O2 -g
EXTRA_CFLAGS += -fstack-protector-strong -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Wformat-overflow
CFLAGS=$(OPT) $(EXTRA_CFLAGS) -Wall -Wextra -Wno-parentheses -Wno-missing-field-initializers

LDFLAGS += -z noexecstack -z relro -z now -pie
EXTRA_LDFLAGS += -pthread -lbpf -lelf

.PHONY: all
all: tsq txrx-tsn

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

clean:
	$(RM) tsq txrx-tsn *~ .[!.]*.un~ .[!.]*.o .[!.]*.a
	$(RM) `find . -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
