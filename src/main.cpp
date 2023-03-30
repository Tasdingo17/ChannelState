#include "chest.h"
#include "abet/yaz/yaz.h"
#include <iostream>

void usage(const char *proggie)
{
    std::cerr << "usage: " << proggie << " <-R|-S <dest addr>>" << std::endl;

    std::cerr << "   if sender (-S <destaddr>):" << std::endl;
    std::cerr << "      (default destination address: 127.0.0.1" << std::endl;
    std::cerr << "      -l <int>   minimum packet size (default: 200)" << std::endl;

    std::cerr << "      -c <int>   initial packet size (bytes; default: 1500)" << std::endl;
    std::cerr << "      -i <int>   initial packet spacing (microseconds; default: " << MIN_SPACE << ")" << std::endl;

    std::cerr << "      -n <int>   packet stream length (default: 50)" << std::endl;
    std::cerr << "      -m <int>   number of streams per measurement (default: 1)" << std::endl;
    std::cerr << "      -r <float> set convergence resolution (default: 500.0 kb/s)" << std::endl;
    std::cerr << "      -s <int>   mean inter-stream spacing (default: 50 milliseconds)" << std::endl;

    std::cerr << "   for both sender and receiver:" << std::endl;
    std::cerr << "      -p <port>  specify control port (" << DEST_CTRL_PORT << ")" << std::endl;
    std::cerr << "      -P <port>  specify probe port (" << DEST_PORT << ")" << std::endl;
    std::cerr << "      -v         increase verbosity" << std::endl;
    std::cerr << "      -u         use round-robin scheduler for threads(?) *****" << std::endl;
#if HAVE_PCAP_H
    std::cerr << "      -x <str>   pcap interface name (no default)" << std::endl;
#endif
}


bool is_root(){
    return geteuid() == 0;
}


int main(int argc, char **argv)
{
    if (!is_root()){
        std::cerr << "ChEst requires root privileges" <<  std::endl;
        return 1;
    }
    
    int c;

    unsigned short dest_control = DEST_CTRL_PORT;
    unsigned short dest_port = DEST_PORT;

    std::string dstip = "127.0.0.1";
    bool sender = false;
    bool receiver = false;
    int min_pkt_size = 200;
    int init_pkt_size = 1500;
    int init_spacing = MIN_SPACE;
    int stream_length = 50;
    int n_streams = 1;
    int inter_stream_spacing = 50000;
    int verbose = 0;
    float resolution = 500000.0;
#if HAVE_PCAP_H
    std::string pcap_dev = "";
#endif
    bool sched_up = false;

    while ((c = getopt(argc, argv, "c:i:l:m:n:p:P:RS:r:s:vux:")) != EOF)
    {
        switch(c)
        {
        case 'i':
            init_spacing = atoi(optarg);
            break;
        case 'c':
            init_pkt_size = atoi(optarg);
            break;
        case 'l':
            min_pkt_size = atoi(optarg);
            break;
        case 'm':
            n_streams = atoi(optarg);
            break;
        case 'n':
            stream_length = atoi(optarg);
            break;
        case 'p':
            dest_control = atoi(optarg);
            break;
        case 'P':
            dest_port = atoi(optarg);
            break;
        case 'r':
            resolution = atof(optarg) * 1000.0; // input as kbps - conv to bps
            break;
        case 'R':
            receiver = true;
            sender = false;
            break;
        case 'S':
            dstip = optarg;
            receiver = false;
            sender = true;
            break;
        case 's':
            inter_stream_spacing = atoi(optarg) * 1000; // input as millisec, internal as microsec
            break;
        case 'u':
            sched_up = true;
            break;
        case 'v':
            verbose++;
            break;
#if HAVE_PCAP_H
        case 'x':
            pcap_dev = optarg;
            break;
#endif
        default:
            usage(argv[0]);
            exit (-1);
        }
    }

    std::unique_ptr<ABSender> ab_sender;
    std::unique_ptr<ABReceiver> ab_receiver;
    std::unique_ptr<ChestEndPt> chest;

    if (sender)
    {
        if (verbose)
            std::cout << "## starting sender ##" << std::endl;

        std::unique_ptr<YazSender> ys = std::make_unique<YazSender>();

        ys->setMinPktSize(min_pkt_size);
        ys->setStreamLength(stream_length);
        ys->setStreams(n_streams);
        ys->setInterStreamSpacing(inter_stream_spacing);
        ys->setTarget(dstip.c_str());
        ys->setResolution(resolution);
        ys->setInitialSpacing(init_spacing);
        ys->setInitialPktSize(init_pkt_size);

        ys->setCtrlDest(dest_control);
        ys->setProbeDest(dest_port);
        ys->setVerbosity(verbose);
    #if HAVE_PCAP_H
        ys->setPcapDev(pcap_dev);
    #endif
        ys->setCtrlDest(dest_control);
        ys->setProbeDest(dest_port);
        ys->setVerbosity(verbose);
    #if HAVE_PCAP_H
        ys->setPcapDev(pcap_dev);
    #endif

        ab_sender = std::move(ys);
    }
    else if (receiver)
    {
        if (verbose)
            std::cout << "## starting sender ##" << std::endl;

        std::unique_ptr<YazReceiver> ys = std::make_unique<YazReceiver>();
        ys->setCtrlDest(dest_control);
        ys->setProbeDest(dest_port);
        ys->setVerbosity(verbose);
    #if HAVE_PCAP_H
        ys->setPcapDev(pcap_dev);
    #endif

        ab_receiver = std::move(ys);
    }
    else
    {
        std::cerr << "Must use -R or -S to specify as receiver or sender." << std::endl;
        return (0);
    }


#if HAVE_SCHED_SETSCHEDULER
    if (sched_up)
    {
        // try to raise our scheduling priority.  
        int maxprio = sched_get_priority_max(SCHED_RR);
        struct sched_param sp = { maxprio };
        if (sched_setscheduler(0, SCHED_RR, &sp) == 0)
        {
            sched_getparam(0, &sp);
            std::cout << "using real-time round-robin scheduler";
            if (verbose > 1)
                std::cout << "scheduler/param: " << sched_getscheduler(0) << '/' << sp.sched_priority;
            std::cout << "." << std::endl;
        }
        else
        {
            std::cout << "using default scheduler." << std::endl;
        }
    }
#endif

    if (sender){
        Pinger pinger(dstip.c_str());
        LossDumb losser;
        chest = std::make_unique<ChestSender>(*ab_sender, pinger, losser);
    } else {
        chest = std::make_unique<ChestReceiver>(*ab_receiver);
    }

    chest->run();
    google::protobuf::ShutdownProtobufLibrary();
    return (0);
}