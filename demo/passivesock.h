#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h>



extern int errno;
unsigned short portbase = 0;    /* port base, for non-root servers */

/* -----------------------------------------------------------
 * errexit - print an error message and exit
 * -----------------------------------------------------------
 */
int errexit(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}



/* ------------------------------------------------------------
 * passivesock - allocate & bing server socket using TCP or UDP
 * ------------------------------------------------------------
 */
int passivesock(const char *service, const char *transport, int qlen)
/* service      - service associated with the desired port
 * transport    - transport protocol to use ("tcp" or "udp")
 * qlen         - maximum server request queue length
 */
{
    struct servent *pse;        /* pointer to service information entry */
    struct protoent *ppe;       /* pointer to protocol information entry */
    struct sockaddr_in sin;     /* an Internet endpoint address         */
    int     s, type;            /* socket descorptor and socket type    */

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

    /* Map service name to port number */
    if (pse = getservbyname(service, transport))
        sin.sin_port = htons(ntohs((unsigned short)pse->s_port) + portbase);
    else if ((sin.sin_port = htons((unsigned short)atoi(service))) == 0)
            errexit("can't get \"%s\" service entry\n", service);

    /* Map protocol name to protocol number */
    if ((ppe = getprotobyname(transport)) == 0)
        errexit("can't get \"%s\" protocol entry\n", transport);

    /* Use protocol to choose a socket type */
    if (strcmp(transport, "udp") == 0)
        type = SOCK_DGRAM;
    else 
        type = SOCK_STREAM;

    /* Allocate a socket */
    s = socket(PF_INET, type, ppe->p_proto);
    if (s < 0)
        errexit("can't creat socket: %s\n", strerror(errno));

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0 )
        errexit("can't bind to %s\n", service, strerror(errno));
    printf("\t[Info] Binding...\n");

    /* Listen the socket */
    if (type == SOCK_STREAM && listen(s, qlen) < 0)
        errexit("cant't listen on %s port: %s\n", service, strerror(errno));
    printf("\t[Info] Listening...\n");

    return s;
}


int passiveTCP(char *service,int qlen)
{
    return passivesock(service, "tcp", qlen);
}












