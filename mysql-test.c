#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <my_global.h>      /* MySQL client headers */
#include <mysql.h>

#define PORT                8306  /* TCP listening port */
#define LISTENQ             128   /* TCP Backlog for listen() */
#define BUFSIZE             128   /* character buffer */
#define DEFAULT_HTTP_CODE   502   /* default to bad HTTP status code */
#define GOOD_IO_RUNNING     "Yes" /* I/O thread */
#define GOOD_SQL_RUNNING    "Yes" /* SQL thread */
#define GOOD_SECONDS_BEHIND 4     /* maximum acceptable lag behind master */

#define MYSQL_HOST          "10.4.1.43"
#define MYSQL_USER          "haproxy"
#define MYSQL_PASSWORD      ""
#define QUERY               "show slave status"

int main(int argc, char *argv[]) {
    int       list_s;                            /* listening socket */
    int       conn_s;                            /* connection socket */
    struct    sockaddr_in servaddr;              /* socket address structure */
    const int reuseaddr    = 1;                  /* socket options */
    const int port         = PORT;               /* port number */
    int       status       = DEFAULT_HTTP_CODE;  /* HTTP status code */
    const int PROTOCOL_TCP = MYSQL_PROTOCOL_TCP; /* use TCP/IP */

    if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating listening socket: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(MYSQL_HOST);
    servaddr.sin_port        = htons(port);

    if (setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR,
        (const void *) &reuseaddr, sizeof(int)) < 0) {
        fprintf(stderr, "Error calling setsockopt(): %d\n", errno);
        exit(EXIT_FAILURE);
    }

    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Error calling bind(): %d\n", errno);
        exit(EXIT_FAILURE);
    }

    if (listen(list_s, LISTENQ) < 0) {
        fprintf(stderr, "Error calling listen(): %d\n", errno);
        exit(EXIT_FAILURE);
    }

    for (;;) {

        if ((conn_s = accept(list_s, NULL, NULL)) < 0) {
            continue;
        }

        MYSQL *conn_m = mysql_init(NULL);

        /*
         * While doing error handling on MySQL connection we
         * deliberately set different new HTTP status code.
         *
         * HA-Proxy will be getting:
         *   200 (good)  - MySQL in good shape
         *   500 (error) - MySQL in bad shape
         *   503 (error) - connection failure to database
         *   502 (error) - something wrong with this program
         */

        if (conn_m == NULL) {
            status = 503;
            fprintf(stderr, "503 in mysql_init\n");
        }

        mysql_options(conn_m, MYSQL_OPT_PROTOCOL, &PROTOCOL_TCP);

        if (mysql_real_connect(conn_m, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD,
            NULL, 0, NULL, 0) == NULL) {
            status = 503;
            fprintf(stderr, "503 in mysql_real_connect: %s\n",
                    mysql_error(conn_m));
        }

        if (mysql_query(conn_m, QUERY)) {
            status = 503;
            fprintf(stderr, "503 in mysql_query\n");
        }
  
        MYSQL_RES *result = mysql_store_result(conn_m);
  
        if (result == NULL) {
            status = 503;
            fprintf(stderr, "503 in mysql_store_result\n");
        }

        if (status == DEFAULT_HTTP_CODE) {

            /*
             * Connected to database.
             */

            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
                char *slave_io_running      = row[10];
                char *slave_sql_running     = row[11];
                char *seconds_behind_master = row[32];

                /*
                 * Testing for running slave threads:
                 */

                if (strcmp(GOOD_IO_RUNNING, slave_io_running) != 0) {
                    status = 500;
                    fprintf(stderr, "500: in GOOD_IO_RUNNING\n");
                } else if (strcmp(GOOD_SQL_RUNNING, slave_sql_running) != 0) {
                    status = 500;
                    fprintf(stderr, "500: in GOOD_SQL_RUNNING\n");
                } else {

                    /*
                     * Testing for lag behind master:
                     */

                    char *end;
                    errno = 0;
                    const long ld = strtol(seconds_behind_master, &end, 10);
                    int   seconds;

                    if ('\0' == seconds_behind_master[0]) {
                        status = 500;
                        fprintf(stderr, "500: array empty\n");
                    } else if (end == seconds_behind_master) {
                        status = 500;
                        fprintf(stderr, "500: %s: not a number\n",
                                seconds_behind_master);
                    } else if ('\0' != *end) {
                        status = 500;
                        fprintf(stderr, "500: %s: extra characters: %s\n",
                                seconds_behind_master, end);
                    } else if ((LONG_MIN == ld || LONG_MAX == ld)
                               && ERANGE == errno) {
                        status = 500;
                        fprintf(stderr, "500: %s out of range of type long\n",
                                seconds_behind_master);
                    } else if (ld > INT_MAX) {
                        status = 500;
                        fprintf(stderr, "500: %ld greater than INT_MAX\n", ld);
                    } else if (ld < INT_MIN) {
                        status = 500;
                        fprintf(stderr, "500: %ld less than INT_MIN\n", ld);
                    } else if (ld < 0) {
                        status = 500;
                        fprintf(stderr, "500: %ld less than zero\n", ld);
                    } else {
                        seconds = (int)ld;

                        if (seconds <= GOOD_SECONDS_BEHIND) {
                            status = 200;
                        } else {
                            status = 500;
                            fprintf(stderr, "500: in GOOD_SECONDS_BEHIND\n");
                        }
                    }
                }
                break;
            }

            mysql_free_result(result);
            mysql_close(conn_m);
        }

        char message[BUFSIZE];

        switch (status) {
        case 200:
            strcpy(message, "HTTP/1.1 200 OK\r\n");
            strcat(message, "Content-length: 4\r\n");
            strcat(message, "Content-Type: text/plain\r\n\r\n");
            strcat(message, "OK\r\n");
            break;
        case 500:
            strcpy(message, "HTTP/1.1 500 Internal Server Error\r\n");
            break;
        case 503:
            strcpy(message, "HTTP/1.1 503 Temporarily unavailable\r\n");
            break;
        default:
            strcpy(message, "HTTP/1.1 502 Bad Gateway\r\n");
            break;
        }

        send(conn_s, message, strlen(message), MSG_NOSIGNAL);

        status = DEFAULT_HTTP_CODE;  /* resetting status for next round */

        close(conn_s); 
    }
    return 0;
}
