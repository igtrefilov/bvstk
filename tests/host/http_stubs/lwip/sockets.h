#ifndef TEST_HTTP_SOCKETS_H
#define TEST_HTTP_SOCKETS_H
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
int lwip_select(int n, fd_set *read, fd_set *write, fd_set *except, struct timeval *time);
int lwip_recv(int fd, void *buf, size_t len, int flags);
int lwip_send(int fd, const void *buf, size_t len, int flags);
#define lwip_socket socket
#define lwip_setsockopt setsockopt
#define lwip_bind bind
#define lwip_close close
#define lwip_listen listen
#define lwip_accept accept
#endif
