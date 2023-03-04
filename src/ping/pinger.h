#ifndef __Pinger__
#define __Pinger__

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE /* for additional type definitions */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>            /* fcntl() */
#include <netdb.h>            /* getaddrinfo() */
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>        /* inet_XtoY() */
#include <netinet/in.h>       /* IPPROTO_ICMP */
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>  /* struct icmp */
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string>

// in microseconds
#define DEFAULT_PING_GAP 1000000
#define DEFAULT_PING_TIMEOUT 1000000

#define ICMP_HEADER_LENGTH 8
#define MESSAGE_BUFFER_SIZE 1024

#ifndef ICMP_ECHO
    #define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHO_REPLY
    #define ICMP_ECHO_REPLY 0
#endif

typedef int socket_t;
typedef struct msghdr msghdr_t;
typedef struct cmsghdr cmsghdr_t;


struct PingRes {
public:
    int rtt; // microseconds
    bool bad_checksum;
    PingRes(int _rtt=-1, bool _checksum=false);
};


class Pinger{
public:
    explicit Pinger(const char* _hostname, int _ping_gap=DEFAULT_PING_GAP,
                    int _ping_timeout=DEFAULT_PING_TIMEOUT);
    Pinger(const Pinger& other) = delete;
    Pinger& operator=(const Pinger& other) = delete;
    Pinger(Pinger&&) = default;
    Pinger& operator=(Pinger&& other) = default;
    ~Pinger();

    PingRes ping(int seq=0, int id=-1);     // return rtt in microseconds
    void ping_continuously();
    int get_ping_gap() const;
    std::string get_hostname() const;
    void print_host_ip() const;
private:
    std::string hostname;
    int ping_gap;
    int ping_timeout;   // after that packet is considered lost

    socket_t sockfd;
    struct sockaddr_storage addr;
    socklen_t dst_addr_len;
    void make_socket(struct addrinfo* addrinfo_list);
    void set_addr(struct addrinfo* adrrinfo);
};


#endif