bin_PROGRAMS = tsq txrx-tsn

if WITH_OPCUA
bin_PROGRAMS += opcua-server
endif

tsq_SOURCES = src/tsq.c

txrx_tsn_SOURCES = src/txrx.c src/txrx-afpkt.c

if WITHXDP
txrx_tsn_SOURCES += src/txrx-afxdp.c
endif

if ! WITHXDPTBS
EXTRA_CFLAGS_NOXDPTBS = -Wno-unused-but-set-parameter -Wunused-but-set-variable
endif

opcua_server_SOURCES=src/opcua-tsn/multicallback_server.c   \
			src/opcua-tsn/json_helper.c	\
			src/opcua-tsn/opcua_common.c	\
			src/opcua-tsn/opcua_custom.c	\
			src/opcua-tsn/opcua_datasource.c\
			src/opcua-tsn/opcua_publish.c	\
			src/opcua-tsn/opcua_subscribe.c
txrx_tsn_LDADD = $(libbpf_LIBS) $(libelf_LIBS) -lpthread
opcua_server_LDADD = $(open62451_LIBS) $(libjson_LIBS) $(libbpf_LIBS) $(libelf_LIBS) -lpthread

AM_CPPFLAGS = -O2 -g -fstack-protector-strong -fPIE -fPIC -D_FORTIFY_SOURCE=2 \
		-Wformat -Wformat-security -Wformat-overflow -Wno-parentheses \
		-Wno-missing-field-initializers -Wextra -Wall -fno-common \
		$(open62451_CFLAGS) $(libjson_CFLAGS) $(libbpf_CFLAGS) $(libelf_CFLAGS) $(ENABLEXDP_CPPFLAGS) $(EXTRA_CFLAGS_NOXDPTBS)
AM_LDFLAGS = -Wl,-z,noexecstack,-z,relro,-z,now -pie
