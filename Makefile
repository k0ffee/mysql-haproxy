#
# This uses bmake.
#

CC  = clang
CFLAGS += -Wall -Wextra -Werror
OS != uname

UMASK = 77

.if ${OS}   == Darwin
    GROUP = _www
.if exists (/opt/pkg/include/mysql)
    CFLAGS += -I/opt/pkg/include/mysql -L/opt/pkg/lib        # pkgsrc
.else
    CFLAGS += -I/usr/local/Cellar/mysql/5.7.21/include/mysql # homebrew
    CFLAGS += -L/usr/local/Cellar/mysql/5.7.21/lib           # homebrew
.endif
.elif ${OS} == FreeBSD
    GROUP = www
    CFLAGS += -I/usr/local/include/mysql -L/usr/local/lib/mysql
.elif ${OS} == Linux
    GROUP = www-data
    CC = cc
    CFLAGS += -I/usr/include/mysql -L/usr/lib/x86_64-linux-gnu
.endif

LIBS = -lmysqlclient

.PHONY: all mysql galera

all: mysql-test

mysql: mysql-test

mysql-test! mysql-test.c
	umask ${UMASK} && ${CC} ${CFLAGS} ${LIBS} -D__MYSQL__ -o $@ $<
	chmod 600 $<
	chmod 710 $@
	-chgrp ${GROUP} $@

galera: galera-test

galera-test! galera-test.c
	umask ${UMASK} && ${CC} ${CFLAGS} ${LIBS} -D__GALERA__ -o $@ $<
	chmod 600 $<
	chmod 710 $@
	-chgrp ${GROUP} $@

.PHONY: clean clean-mysql clean-galera

clean: clean-mysql clean-galera

clean-mysql:
	rm -f mysql-test

clean-galera:
	rm -f galera-test
