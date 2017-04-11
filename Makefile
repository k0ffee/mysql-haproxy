#
# This uses bmake.
#
NAME = mysql-test
CC   = clang
OS  != uname
UMASK = 77
.if ${OS}   == Darwin
    GROUP = _www
    CFLAGS += -I/opt/pkg/include/mysql -L/opt/pkg/lib
.elif ${OS} == FreeBSD
    GROUP = www
    CFLAGS += -I/usr/local/include/mysql -L/usr/local/lib/mysql
.elif ${OS} == Linux
    GROUP = www-data
    CC = cc
    CFLAGS += -I/usr/include/mysql -L/usr/lib/x86_64-linux-gnu
.endif

all: build test

build:
	umask ${UMASK}
	${CC} ${CFLAGS} -lmysqlclient -o ${NAME} ${NAME}.c
	chmod 600 ${NAME}.c
	chmod 710 ${NAME}
	chgrp ${GROUP} ${NAME}

test:
