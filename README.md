# mysql-haproxy

Those two daemons test a running MySQL for
parameters like general availability, replication lag,
or the database system being read-only.

As an example `mysql-test.c` binds to an IPv4 address
and is written for traditional MySQL setups using
asynchronous replication.

`galera-test.c` is written for MySQL Galera, listens
and queries via IPv6, and tests its target for being
in read-only mode.

In HAproxy query this as an HTTP service, it will
return status code "200" if everything is fine.

Example HAproxy configuration snippet:

```
listen api-ro
    bind     0.0.0.0:3307 
    balance  first # use first healthy node in list
    default-server port 8306 inter 1s downinter 5s fall 20 rise 20 slowstart 20s
    server   db-3  db-3:3306 check
    server   db-2  db-2:3306 check backup
    server   db-1  db-1:3306 backup
```

Here all connections are pointed to host `db-3`, if that one
doesn't return HTTP status "200 OK", points all new
connections to host `db-2`, again tests if that one returns
"200 OK", and if not, as a matter of last resort, turns to
database host `db-1`, regardless of its HTTP status.

# Offsets

Useful command while using offsets, they might be different in MySQL versions:

```
mysql -e 'show slave status' | tr '\t' '\n' | awk '{print NR-1, $1}' | less
```
