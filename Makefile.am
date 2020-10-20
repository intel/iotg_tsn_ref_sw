bin_PROGRAMS = tsq txrx-tsn opcua-server
tsq_SOURCES = src/tsq.c
txrx_tsn_SOURCES = src/txrx.c src/txrx-afpkt.c src/txrx-afxdp.c
opcua_server_SOURCES=src/opcua-tsn/multicallback_server.c   \
			src/opcua-tsn/json_helper.c	\
			src/opcua-tsn/opcua_common.c	\
			src/opcua-tsn/opcua_custom.c	\
			src/opcua-tsn/opcua_datasource.c\
			src/opcua-tsn/opcua_publish.c	\
			src/opcua-tsn/opcua_subscribe.c
txrx_tsn_LDADD = -lpthread -lbpf -lelf
opcua_server_LDADD = -lpthread -lbpf -lelf -lopen62541 -ljson-c

AM_CPPFLAGS = @OPEN62541_HEADERS@

AM_CFLAGS = -O2 -g -fstack-protector-strong -fPIE -fPIC -D_FORTIFY_SOURCE=2   \
		-Wformat -Wformat-security -Wformat-overflow -Wno-parentheses \
		-Wno-missing-field-initializers -Wextra -Wall
AM_LDFLAGS = -Wl,-z,noexecstack,-z,relro,-z,now -pie
