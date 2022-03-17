#include <stdint.h>
#include <sys/socket.h>
#include <poll.h>

/**
 * will keep the netpoll active until set to zero; setting this to zero
 * will free all resources used by the current poller; there is no need to
 * reinitialize this variable as it is set to 1 everytime tcp_netpoll is
 * called
 */
volatile int netpoll_keepalive;

/**
 * @brief function poitner to be defined in the caller and provided to
 *        tcp_netpoll; allows that caller to handle poll events and makes
 *        this library more portable
 *
 * @param sfd - socket file descriptor for the connection that casued
 *        the event
 *
 * @return nothing
 *
 */
typedef void (*reventhandler)(int sfd);

/**
 * @brief helper function to open a listening socket on @param port and
 *        @param ip domain (AF_INET or AF_INET6)
 *
 * @param port - port to listen on
 *
 * @param ipDomain - specifies ip protocol should use the defined AF_INET or
 *        AF_INET6
 *
 * @param maxpend - maximum number of pending connections on listen(2)
 *
 *
 * @return socket file descriptor or -1 on error
 *
 */
int tcp_socketsetup(uint16_t port, int ipDomain, int maxpend);

/**
 * @brief helper function that pretty prints a sock_addr storage struct
 *
 * @param in - sockaddr_storage to be printed
 *
 * @return nothing
 *
 */
void tcp_printsockaddr(struct sockaddr_storage *in);

/**
 * @brief helper function that polls for incoming connections and data
 *        from clients
 *
 * @param sockfd - server socket file descriptor
 *
 * @param eventhandler - a function pointer that accepts a void ponter for
 *        arguments; points a to a caller defined function that will handle
 *        events for poll notably
 *
 * @param maxcon - maximum number of connections
 *
 * @param timeout - time (in seconds) that poll will wait for an event
 *
 *
 * @return nothing
 *
 */
int tcp_netpoll(int sockfd, reventhandler rh, int maxcon, int timeout);

/**
 * @brief handles partial reads from a file descriptor provided the amount
 *        of expected data is known
 *
 * @param fd - file descriptor to read from
 *
 * @param buf - buffer to store the read data; THIS BUFFER SHOULD BE
 *        AT LEAST @param readlen LONG, THIS FUNCTION WILL NOT SAVE
 *        YOU FROM BUFFER OVERFLOWS
 *
 * @param readlen - length of data expected from @param fd
 *
 * @return total bytes written on success; -1 on error
 *
 */
int tcp_read_handler(int fd, void *buf, uint readlen);

/**
 * @brief handles partial writes to a file descriptor
 *
 * @param fd - file descriptor tow write to
 *
 * @param buf - buffer that contains that data to be sent
 *
 * @param writelen - amount of data to be written to @param fd
 *
 * @return total bytes written on success; -1 on error
 *
 */
int tcp_write_handler(int fd, char *buf, uint writelen);
