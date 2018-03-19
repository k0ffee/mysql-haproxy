#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <my_global.h>     /* MySQL client headers */
#include <mysql.h>

#include "config.h"

int main(void) {
    int       list_s;                            /* listening socket */
    int       conn_s;                            /* connection socket */
    struct    sockaddr_in6 servaddr;             /* socket address structure */
    const int reuseaddr    = 1;                  /* socket options */
    const int port         = PORT;               /* port number */
    int       status       = DEFAULT_HTTP_CODE;  /* HTTP status code */
    const int PROTOCOL_TCP = MYSQL_PROTOCOL_TCP; /* use TCP/IP */

    if ((list_s = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating listening socket: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_addr   = in6addr_any;
    servaddr.sin6_port   = htons(port);

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
        }

        mysql_options(conn_m, MYSQL_OPT_PROTOCOL, &PROTOCOL_TCP);

        if (mysql_real_connect(conn_m, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD,
            NULL, 0, NULL, 0) == NULL) {
            status = 503;
        }

        if (mysql_query(conn_m, QUERY_WSREP_STATE)) {
            status = 503;
        }

        MYSQL_RES *result = mysql_store_result(conn_m);

        if (result == NULL) {
            status = 503;
        }

        if (status == DEFAULT_HTTP_CODE) {

            /*
             * We are connected to database.
             */

            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
                if (strcmp(GOOD_GALERA_STATUS, row[1]) == 0) {
                    status = 200;
                } else {
                    status = 500;
                }
            }

            /*
             * Test if MySQL is in "read only" mode.
             */

            if (status == 200) {

                if (mysql_query(conn_m, QUERY_READ_ONLY)) {
                    status = 503;
                }

                MYSQL_RES *result = mysql_store_result(conn_m);

                if (result == NULL) {
                    status = 503;
                }

                while ((row = mysql_fetch_row(result))) {
                    if (strcmp(READ_ONLY_STATUS, row[1]) == 0) {
                        status = 200;
                    } else {
                        status = 500;
                    }
                }
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
}
