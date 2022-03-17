#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for POLLRDHUP
#endif
#include <netpoll.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief accepts all waiting connections and stores the socket
 *        file descriptor in @param pfds; if there are no available
 *        pfds left then the connection is closed
 *
 * @param sockfd - server socket file descriptor
 *
 * @param pfd - pointer to poll file descriptor array
 *
 * @param plen - length of pfd array
 *
 * @return number of newly connected clients
 *
 */
static int _tcp_acceptconn(int sockfd, struct pollfd *pfds, int plen);

/**
 * @brief closes the socket file descriptor and cleans the pfd struct of
 *        a given poll file descriptor object
 *
 * @param pfd - pointer to poll file descriptor to be closed/cleaned
 *
 * @return 0 on success, nonzero on error
 *
 */
static int _tcp_closepfd(struct pollfd *pfd);

/**
 * @brief closes all sockets in the pollfd array
 *
 * @param pfd - pointer to poll file descriptor array
 *
 * @param plen - length of pfd array
 *
 * @return 0 on success; nonzero on failure
 *
 */
static int _tcp_shutdown(struct pollfd *pfds, int plen);

int
tcp_write_handler(int fd, char *buf, uint writelen)
{
    uint total_write = 0;
    int  ret         = 0;

    if (NULL == buf)
    {
        fprintf(stderr, "! tcp_write_handler: NULL read buffer\n");
        goto ERR;
    }

    while (total_write < writelen)
    {
        ret = write(fd, buf, writelen);
        if (0 > ret)
        {
            perror("error in _write_handler");
            ret = -1;
            goto ERR;
        }
        total_write += ret;
    }

    ret = total_write;
ERR:
    return ret;
}

int
tcp_read_handler(int fd, void *buf, uint readlen)
{
    int  ret        = 0;
    uint total_read = 0;

    if (NULL == buf)
    {
        fprintf(stderr, "! tcp_read_handler: NULL read buffer\n");
        goto ERR;
    }

    while (total_read < readlen)
    {
        ret = read(fd, &((char *)buf)[total_read], readlen - total_read);
        if (0 > ret)
        {
            perror("error in _read_handler");
            ret = -1;
            goto ERR;
        }

        total_read += ret;
    }

    ret = total_read;
ERR:
    return ret;
}

int
tcp_netpoll(int sockfd, reventhandler rh, int maxcon, int timeout)
{
    struct pollfd pfds[++maxcon]; // acounts for server socket in pfds
    int           pret    = 0;
    int           nfds    = 1;
    int           ret     = 0;
    int           currfds = 0;

    if (NULL == rh)
    {
        fprintf(stderr, "! tcp_netpoll: NULL revent handler\n");
        goto ERR;
    }

    memset(pfds, 0, sizeof(pfds));
    pfds[0].fd     = sockfd;
    pfds[0].events = POLLIN;

    netpoll_keepalive = 1;
    while (netpoll_keepalive)
    {
#ifndef NDEBUG
        fprintf(stderr, "[*] polling...\n");
#endif // NDEBUG

        pret = poll(pfds, nfds, timeout);

        if (EINTR == errno)
        {
            perror("! tcp_netpoll: poll interrupted, closing poller...\n");
            ret = 0;
            goto ERR;
        }

        if (0 > pret)
        {
            perror("! tcp_netpoll: poll error\n");
            ret = -1;
            goto ERR;
        }

#ifndef NDEBUG
        if (0 == pret)
        {
            fprintf(stderr, "[*] poll timed out\n");
        }
#endif // NDEBUG

        currfds = nfds;

        for (int i = 0; i < currfds; i++)
        {
            switch (pfds[i].revents)
            {
                case POLLIN:
                    if (sockfd == pfds[i].fd)
                    {
                        printf("[*] received connection\n");
                        nfds += _tcp_acceptconn(pfds[i].fd, pfds, maxcon);
                    }
                    else
                    {
#ifndef NDEBUG
                        printf("[*] data received from client\n");
#endif // NDEBUG
                        rh(pfds[i].fd);
                    }
                    break;
                case POLLERR:
                    if (sockfd == pfds[i].fd)
                    {
                        fprintf(stderr,
                                "! tcp_netpoll: error with server socket, "
                                "shutting down\n");
                        goto ERR;
                    }
                    else
                    {
                        fprintf(stderr,
                                "! tcp_netpoll: error with socket %i\n",
                                pfds[i].fd);
                        _tcp_closepfd(&pfds[i]);
                        nfds--;
                    }
                    break;
                case POLLRDHUP | POLLIN:
                    printf("[*] Client %i ended connection\n", pfds[i].fd);
                    _tcp_closepfd(&pfds[i]);
                    nfds--;
                    break;
                case 0: // caused by poll timeout
                    break;
                default:
                    fprintf(stderr,
                            "! tcp_netpoll: unexecpected event = %i\n",
                            pfds[i].revents);
            } // switch
        }     // while
    }

ERR:
    _tcp_shutdown(pfds, maxcon);
    return ret;
}

void
tcp_printsockaddr(struct sockaddr_storage *in)
{
    char     addr[INET6_ADDRSTRLEN];
    uint16_t port;

    if (NULL == in)
    {
        fprintf(stderr, "! tcp_printsockaddr: NULL sockaddr struct\n");
        goto ERR;
    }

    switch (in->ss_family)
    {

        case AF_INET:
            inet_ntop(in->ss_family,
                      &((struct sockaddr_in *)in)->sin_addr,
                      addr,
                      sizeof(addr));
            port = ntohs(((struct sockaddr_in *)in)->sin_port);
            break;

        case AF_INET6:
            inet_ntop(in->ss_family,
                      &((struct sockaddr_in6 *)in)->sin6_addr,
                      addr,
                      sizeof(addr));
            port = ntohs(((struct sockaddr_in6 *)in)->sin6_port);
            break;
    }
    fprintf(stderr,
            "[*] saddr = family:%d (2=AF_INET, 10=AF_INET6)\n\t%s:%d\n",
            in->ss_family,
            addr,
            port);

ERR:
    return;
}

int
tcp_socketsetup(uint16_t port, int ipDomain, int maxpend)
{

#ifndef NDEBUG
    fprintf(stderr, "[*] converse_tcp listening on %i:%hu\n", ipDomain, port);
#endif // NDEBUG

    char             p[6] = { 0 };
    int              err  = 0;
    struct addrinfo  hints;
    struct addrinfo *res    = NULL;
    int              sockfd = 0;
    int              ret    = 0;

    snprintf(p, sizeof(p), "%i", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = ipDomain;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    err               = getaddrinfo(NULL, p, &hints, &res);
    if (0 != err)
    {
        perror("! tcp_socketsetup: getaddrinfo error");
        sockfd = -1;
        goto ERR;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (0 > sockfd)
    {
        perror("! tcp_socketsetup: socket error");
        sockfd = -1;
        goto ERR;
    }

    // set sockopt - this is so we can restart the server immediately
    // after killing it

    int optval = 1;
    err = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (0 != err)
    {
        perror("! tcp_socketsetup: setsockopt error");
        sockfd = -1;
        goto ERR;
    }

    int flags = 0;

    flags = fcntl(sockfd, F_GETFL, 0);
    err   = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (0 > err)
    {
        perror("! tcp_socketsetup: fcntl error");
        sockfd = -1;
        goto ERR;
    }

    err = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (0 != err)
    {
        perror("! tcp_socketsetup: bind error");
        sockfd = -1;
        goto ERR;
    }

    err = listen(sockfd, maxpend);
    if (0 != err)
    {
        perror("! tcp_socketsetup: listen error");
        sockfd = -1;
        goto ERR;
    }

    ret    = sockfd;
    sockfd = -1;
ERR:
    freeaddrinfo(res);
    res = NULL;
    err = close(sockfd);
    if (0 > err && EBADF != errno)
    {
        perror("! tcp_socketsetup: couldn't close server socket after error");
    }
    return ret;
}

static int
_tcp_closepfd(struct pollfd *pfd)
{
    int ret = 0;

    if (NULL == pfd)
    {
        fprintf(stderr, "! _tcp_closepfd: NULL pfd pointer\n");
        goto ERR;
    }

    ret = close(pfd->fd);
    if (0 != ret)
    {
        perror("! tcp_closepfd: error closing file descriptor\n");
        goto ERR;
    }
    memset(pfd, 0, sizeof(struct pollfd));

ERR:
    return ret;
}

static int
_tcp_acceptconn(int sockfd, struct pollfd *pfds, int plen)
{

    int ret = 0;

    if (NULL == pfds)
    {
        fprintf(stderr, "! _tcp_acceptconn: NULL pfds array\n");
        goto RET;
    }

    int                     confd      = 0;
    struct sockaddr_storage client     = { 0 };
    socklen_t               client_len = sizeof(client);

    while (0 <= confd)
    {
        confd = accept(sockfd, (struct sockaddr *)&client, &client_len);
        if (0 > confd)
        {
            if (EWOULDBLOCK != errno && EAGAIN != errno)
            {
                perror("! server: accept error");
            }
            else
            {
                // if EWOULDBLOCK or EAGAIN there are no more connections to
                // accept
                goto RET;
            }
        } // if
        else
        {
            fprintf(stderr, "[*] connection from:\n");
            tcp_printsockaddr(&client);
            int i = 0;
            for (i = 0; i < plen; i++)
            {
                if (0 == pfds[i].fd)
                {
                    pfds[i].fd     = confd;
                    pfds[i].events = POLLIN | POLLRDHUP;
                    ret++;
                    break;
                }
            } // for
            if (plen == i)
            {
                fprintf(stderr, "! server: max connections reached.\n");
                close(confd);
                goto RET;
            }
        } // else
    }     // while

RET:
    return ret;
}

static int
_tcp_shutdown(struct pollfd *pfds, int plen)
{
    int ret = 0;

    printf("[*] shutting down poller...\n");

    if (NULL == pfds)
    {
        fprintf(stderr, "! _tcp_shutdown: NULL pfds array\n");
        goto ERR;
    }

    for (int i = 0; i < plen; i++)
    {
        if (0 != pfds[i].fd)
        {
            _tcp_closepfd(&pfds[i]);
        }
    }

ERR:
    return ret;
}
