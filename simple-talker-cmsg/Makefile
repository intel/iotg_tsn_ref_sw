ifndef OPENAVNU_DIR
	OPENAVNU_DIR = /opt/openavnu
endif
$(info $$OPENAVNU_DIR=$(OPENAVNU_DIR))
EXAMPLES_DIR = $(OPENAVNU_DIR)/examples
AVBLIB_DIR = $(OPENAVNU_DIR)/lib/common
AVBLIB_OBJS = avb_avtp.o avb_gptp.o
AVBLIB_TARGETS = $(addprefix $(AVBLIB_DIR)/,$(AVBLIB_OBJS))

MRPCLIENT_DIR = $(EXAMPLES_DIR)/common
MRPTALKER_OBJS = talker_mrp_client.o
MRPTALKER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPTALKER_OBJS))

DAEMONS_DIR = $(OPENAVNU_DIR)/daemons

SUBDIRS = $(AVBLIB_DIR) $(MRPCLIENT_DIR)

CC?=gcc
OPT=-O2 -g
EXTRA_CFLAGS+=-fstack-protector-strong -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2
EXTRA_CFLAGS+=-Wformat -Wformat-security
WARN=-Wall -Wextra -Wno-parentheses
CFLAGS=$(OPT) $(WARN) $(EXTRA_CFLAGS)
CPPFLAGS=-I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(AVBLIB_DIR) -I$(DAEMONS_DIR)/common
LDFLAGS=-z noexecstack -z relro -z now -pie
LDLIBS=-lrt -lm -pthread

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS) simple-talker-cmsg

$(SUBDIRS):
	make -C $@

simple-talker-cmsg: simple-talker-cmsg.o $(MRPTALKER_TARGETS) $(AVBLIB_TARGETS)

simple-talker-cmsg.o: simple-talker-cmsg.c

$(AVBLIB_DIR)/%.o: $(AVBLIB_DIR)/%.h $(AVBLIB_DIR)/%.c
	make -C $(AVBLIB_DIR) $@

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	$(RM) simple-talker-cmsg
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
