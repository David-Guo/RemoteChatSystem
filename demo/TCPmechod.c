#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>

#include "passivesock.h"
#define QLEN    32
#define BUFSIZE 4096

//extern int errno;
//int errexit(const char *format, ...);
//int passiveTCP(const char *service, int qlen);
int echo(int fd);

/* ----------------------------------------------------
 * main - Singal process Concurrent TCP server for ECHO
 * ----------------------------------------------------
 */

int main(int argc, char *argv[])
{
    char *service = "echo";     /* service name or port number  */
    struct sockaddr_in fsin;    /* thr from address of a client */
    int msock;                  /* master server socket         */
    fd_set rfds;                /* read file descroptor set     */
    fd_set afds;                /* active file descriptor set   */
    socklen_t alen;                   /* form-address length          */
    int fd, nfds;

    switch (argc) {
        case 1:
            break;
        case 2:
            service = argv[1];
            break;
        default:
            errexit("usage: TCPmechod [port]\n");
    }

    msock = passiveTCP(service, QLEN);

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    while (1) {
        memcpy(&rfds, &afds, sizeof(rfds));

        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            errexit("select: %s\n", strerror(errno));
        if (FD_ISSET(msock, &rfds)) {
            int ssock;

            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *) &fsin, &alen);
            printf("\t[Info] Receive connection form %s\n", inet_ntoa(fsin.sin_addr));
            
            if (ssock < 0)
                errexit("accept: %s\n", strerror(errno));
            FD_SET(ssock, &afds);
        }

    for (fd = 0; fd < nfds; ++fd)
        if (fd != msock && FD_ISSET(fd, &rfds))
            if (echo(fd) == 0) {
                (void) close(fd);
                FD_CLR(fd, &afds);
                printf("\t[Info] Close connect from fd: %d", fd);
            }
    }

}



/* ----------------------------------------------------
 * echo - echo one buffer of data, returning byte count
 * ----------------------------------------------------
 */

int echo(int fd)
{
    char  buf[BUFSIZ];
    int   cc;

    cc = read(fd, buf, sizeof(buf));
    if (cc < 0)
        errexit("echo read: %s\n", strerror(errno));
    if (cc && write(fd, buf, cc) < 0)
        errexit("echo write: %s\n", strerror(errno));

    return cc;
}
