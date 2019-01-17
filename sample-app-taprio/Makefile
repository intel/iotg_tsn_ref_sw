CC?=gcc
OPT=-O2 -g
EXTRA_CFLAGS+=-fstack-protector-strong -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2
EXTRA_CFLAGS+=-Wformat -Wformat-security
EXTRA_LDFLAGS+=-pie -z noexecstack -z relro -z now
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses $(EXTRA_CFLAGS)
CFLAGS=$(OPT) -Wno-parentheses $(EXTRA_CFLAGS)
LDLIBS=-lrt -lm -pthread
LDFLAGS+=$(EXTRA_LDFLAGS)

all: sample-app-taprio

sample-app-taprio.o: sample-app-taprio.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c sample-app-taprio.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) sample-app-taprio
	$(RM) `find . -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
