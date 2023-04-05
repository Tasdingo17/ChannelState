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
#include <csignal>
#include <memory>

// in microseconds
#define DEFAULT_PING_GAP 1000000
#define DEFAULT_PING_TIMEOUT 1000000

#define ICMP_HEADER_LENGTH 8
#define MESSAGE_BUFFER_SIZE 128

#ifndef ICMP_ECHO
    #define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHO_REPLY
    #define ICMP_ECHO_REPLY 0
#endif

typedef int socket_t;
typedef struct msghdr msghdr_t;
typedef struct cmsghdr cmsghdr_t;


class PingRes {
public:
    int rtt; // microseconds
    bool bad_checksum;
    PingRes(int _rtt=-1, bool _checksum=false);
};


class PingStat{
public:
    PingStat();
    void process_ping_res(const PingRes& res, int seq, bool verbose=true);
    void print_statistics() const;
    int get_last_rtt() const;
    int get_srtt() const;
    int get_jitter() const;
    double get_loss() const;
private:
    int srtt;   // smoothed_rtt
    int jitter;
    int lost;
    int total;

    int curr_rtt;
    int prev_rtt;
    void update_jitter();
    void update_srtt();
};


class Pinger{
public:
    explicit Pinger(const char* _hostname,
                    int _ping_timeout=DEFAULT_PING_TIMEOUT);
    Pinger(const Pinger& other) = delete;
    Pinger& operator=(const Pinger& other) = delete;
    Pinger(Pinger&&);
    Pinger& operator=(Pinger&& other);
    virtual ~Pinger();

    PingRes ping(int seq=0, int id=-1);     // return rtt in microseconds
    std::string get_hostname() const;
    void print_host() const;
    virtual std::unique_ptr<Pinger> to_unique_ptr();
private:
    std::string hostname;
    int ping_timeout;   // after that packet is considered lost

    socket_t sockfd;
    struct sockaddr_storage addr;
    socklen_t dst_addr_len;
    void make_socket(struct addrinfo* addrinfo_list);
    void set_addr(struct addrinfo* adrrinfo);
};


class ContinuosPinger: public Pinger{
public:
    explicit ContinuosPinger(const char* _hostname, int _ping_gap=DEFAULT_PING_GAP,
                             int _ping_timeout=DEFAULT_PING_TIMEOUT);
    void ping_continuously();
    int get_ping_gap() const;
    virtual std::unique_ptr<Pinger> to_unique_ptr() override;
private:
    int ping_gap;
};


#endif