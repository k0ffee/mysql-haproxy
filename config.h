#define PORT                8306  /* TCP listening port */
#define LISTENQ             128   /* TCP Backlog for listen() */
#define BUFSIZE             128   /* character buffer */
#define DEFAULT_HTTP_CODE   502   /* default to bad HTTP status code */

#define MYSQL_HOST          "::1"
#define MYSQL_USER          "haproxy"
#define MYSQL_PASSWORD      ""

#ifdef __MYSQL__
#define GOOD_IO_RUNNING     "Yes" /* I/O thread */
#define GOOD_SQL_RUNNING    "Yes" /* SQL thread */
#define GOOD_SECONDS_BEHIND 4     /* maximum acceptable lag behind master */
#define QUERY               "show slave status"

#elif __GALERA__
#define GOOD_GALERA_STATUS  "4"   /* wsrep_local_state */
#define READ_ONLY_STATUS    "OFF"
#define QUERY_WSREP_STATE   "show global status where " \
                            "variable_name='wsrep_local_state'"
#define QUERY_READ_ONLY     "show global variables like 'read_only'"
#endif

#ifdef __APPLE__
#define MSG_NOSIGNAL 0x0    /* send(2) */
#endif
