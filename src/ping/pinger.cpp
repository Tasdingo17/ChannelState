#include "pinger.h"
#include <stdexcept>

void resolve_addr(const char* hostname, struct addrinfo** addrinfo_list);


void clear_addrinfo(struct addrinfo* addrinfo_list){
    if (addrinfo_list != NULL) {
        freeaddrinfo(addrinfo_list);
    }
}


/**
 * Returns a timestamp with microsecond resolution.
 */
static uint64_t utime(void) {
    struct timeval now;
    return gettimeofday(&now, NULL) != 0
        ? 0
        : now.tv_sec * 1000000 + now.tv_usec;

}


static uint16_t compute_checksum(const char *buf, size_t size) {
    /* RFC 1071 - http://tools.ietf.org/html/rfc1071 */

    size_t i;
    uint64_t sum = 0;

    for (i = 0; i < size; i += 2) {
        sum += *(uint16_t *)buf;
        buf += 2;
    }
    if (size - i > 0)
        sum += *(uint8_t *)buf;

    while ((sum >> 16) != 0)
        sum = (sum & 0xffff) + (sum >> 16);

    return (uint16_t)~sum;
}


Pinger::Pinger(const char* _hostname, int _ping_gap, int _ping_timeout): 
    hostname(_hostname), ping_gap(_ping_gap), ping_timeout(_ping_timeout)
{
    struct addrinfo* addrinfo_list;
    resolve_addr(_hostname, &addrinfo_list);
    make_socket(addrinfo_list);
    clear_addrinfo(addrinfo_list);
    print_host();
}


std::string Pinger::get_hostname() const{
    return hostname;
}


int Pinger::get_ping_gap() const{
    return ping_gap;
}


Pinger::~Pinger(){
    close(sockfd);
}


void resolve_addr(const char* hostname, struct addrinfo** addrinfo_list){
    int error;
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;
    error = getaddrinfo(hostname,
                        NULL,
                        &hints,
                        addrinfo_list);   
    if (error != 0) {
        clear_addrinfo(*addrinfo_list);
        throw std::runtime_error(std::string("getaddrinfo:") + std::string(gai_strerror(error)));
    }
    return;
};


// use first succesful address for socket and fill address field
void Pinger::make_socket(struct addrinfo* addrinfo_list){
    struct addrinfo* addrinfo;
    for (addrinfo = addrinfo_list; addrinfo != NULL; addrinfo = addrinfo->ai_next) {
        sockfd = socket(addrinfo->ai_family,
                        addrinfo->ai_socktype,
                        addrinfo->ai_protocol);
        if (sockfd >= 0) {  // use first succesfull addr
            break;
        }
    }
    if ((int)sockfd < 0) {
        close(sockfd);
        clear_addrinfo(addrinfo_list);
        throw std::runtime_error("Failed to create socket for all addresses");
    }

    set_addr(addrinfo);
    // make socket non-blocking for timeout feature
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
        clear_addrinfo(addrinfo_list);
        throw std::runtime_error("Failed to make socket non-blocking");
    }
}

void Pinger::set_addr(struct addrinfo* addrinfo){
    memcpy(&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
    dst_addr_len = (socklen_t)addrinfo->ai_addrlen;
}


struct icmp create_request(sockaddr_storage addr, int id, int seq){
    struct icmp request;
    request.icmp_type = ICMP_ECHO;
    request.icmp_code = 0;
    request.icmp_cksum = 0;
    request.icmp_id = htons(id);
    request.icmp_seq = htons(seq);
    request.icmp_cksum = compute_checksum((char *)&request, sizeof(request));
    return request;
}


PingRes Pinger::ping(int seq, int id){
    int delay = -1;
    bool bad_checksum = false;
    if (id == -1){
        id = (uint16_t)getpid();
    }
    struct icmp request = create_request(addr, id, seq);

    if (sendto(sockfd, (char *)&request, sizeof(request), 0, 
               (struct sockaddr *)&addr, (int)dst_addr_len) <= 0){
        close(sockfd);
        throw std::runtime_error(strerror(errno));
    }
    int start_time = utime();

    // wait and process reply
    for (;;) {
        char msg_buf[MESSAGE_BUFFER_SIZE];
        char packet_info_buf[MESSAGE_BUFFER_SIZE];
        struct iovec msg_buf_struct = { msg_buf, sizeof(msg_buf) };
        struct msghdr msg = { NULL, 0,
                              &msg_buf_struct, 1,
                              packet_info_buf, sizeof(packet_info_buf),
                              0 };
        size_t msg_len;
        //cmsghdr_t *cmsg;
        size_t ip_hdr_len;
        struct icmp *reply;
        int reply_id;
        int reply_seq;
        uint16_t reply_checksum;
        uint16_t checksum;
        int error = (int)recvmsg(sockfd, &msg, 0);
        delay = utime() - start_time;

        if (error < 0) {
            if (errno == EAGAIN) {
                if (delay > ping_timeout) {
                    printf("timeout exceeded");
                    return PingRes(-1);
                } else {
                    /* No data available yet, try to receive again. */
                    continue;
                }
            } else {
                perror("recvmsg");
                break;
            }
        }
        msg_len = error;
        // For IPv4, we must take the length of the IP header into account.
        ip_hdr_len = ((*(uint8_t *)msg_buf) & 0x0F) * 4;

        reply = (struct icmp *)(msg_buf + ip_hdr_len);
        reply_id = ntohs(reply->icmp_id);
        reply_seq = ntohs(reply->icmp_seq);

        
        // Verify that this is indeed an echo reply packet.
        if (!(addr.ss_family == AF_INET && reply->icmp_type == ICMP_ECHO_REPLY)){
            continue;
        }

        // Verify the ID and sequence number to make sure that the reply is associated with the current request.
        if (reply_id != id || reply_seq != seq) {
            continue;
        }


        reply_checksum = reply->icmp_cksum;
        reply->icmp_cksum = 0;        
        // Verify the checksum.
        checksum = compute_checksum(msg_buf + ip_hdr_len, msg_len - ip_hdr_len);
        bad_checksum = reply_checksum != checksum;
        break;
    }

    return PingRes(delay, bad_checksum);
}

namespace ping_handler{
    bool stop_ping = false;
}

void stop_ping(int signo){
    ping_handler::stop_ping = true;
}

void Pinger::ping_continuously(){
    PingRes tmp_res;
    PingStat stats;
    ping_handler::stop_ping = false;
    uint16_t id = (uint16_t)getpid();
    auto prev_handler = signal(SIGINT, stop_ping);  // break from loop after sigint
    for (unsigned seq = 0; !ping_handler::stop_ping; seq++) {
        tmp_res = ping(seq, id);
        stats.process_ping_res(tmp_res, seq);
        if ((0 <= tmp_res.rtt) && (tmp_res.rtt < ping_gap)){
            usleep(ping_gap - tmp_res.rtt);
        }
    }
    stats.print_statistics();
    signal(SIGINT, prev_handler);   // return default handler
}


void Pinger::print_host() const{
    char addr_str[56] = "<unknown>"; // 56 is ipv6 lenght
    inet_ntop(addr.ss_family,
              addr.ss_family == AF_INET6
                  ? (void *)&((struct sockaddr_in6 *)&addr)->sin6_addr
                  : (void *)&((struct sockaddr_in *)&addr)->sin_addr,
              addr_str,
              sizeof(addr_str));

    printf("PING %s (%s)\n", hostname.c_str(), addr_str);
}

/////////////////// PingRes
PingRes::PingRes(int _rtt, bool _checksum): rtt(_rtt), bad_checksum(_checksum) {};


/////////////////// PingStat
PingStat::PingStat(): srtt(0), jitter(0), lost(0), total(0), curr_rtt(0), prev_rtt(0) {};

void PingStat::process_ping_res(const PingRes& res, int seq){
    total += 1;
    if (res.rtt == -1){
        lost += 1;
        printf("Request seq=%d lost\n", seq);
        return;
    } else if (res.bad_checksum){
        lost += 1;
        printf("Request seq=%d bad checksum\n", seq);
        return;
    }
    printf("Request seq=%d rtt=%.3f ms\n", seq, res.rtt / 1000.0);
    prev_rtt = curr_rtt;
    curr_rtt = res.rtt;
    update_jitter();
    update_srtt();
}

/* https://datatracker.ietf.org/doc/html/rfc1889#page-71 */
void PingStat::update_jitter(){
    int diff = curr_rtt - prev_rtt;
    if (diff < 0) diff = -diff;
    jitter += (int) ((diff - jitter) * 1./16.);
}

void PingStat::update_srtt(){
    double ALPHA = 0.9;
    if (srtt == 0){
        srtt = curr_rtt;
        return;
    }
    srtt = (int) (ALPHA * srtt + (1-ALPHA) * curr_rtt);
}

int PingStat::get_jitter() const{
    return jitter;
}

int PingStat::get_srtt() const{
    return srtt;
}

// percentage
double PingStat::get_loss() const{
    return 100.* lost / total; 
}

void PingStat::print_statistics() const{
    fprintf(stdout, "Ping statistics:\n");
    fprintf(stdout, "Total packets: %d, lost packets: %d, loss percentage: %.3f %%\n",
            total, lost, get_loss());
    fprintf(stdout, "sRTT: %.3f ms, jitter: %.3f ms\n", srtt / 1000., jitter / 1000.);
}
