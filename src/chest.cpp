#include "chest.h"
#include <thread>
#include <iostream>
#include <future>
#include <fstream>

// for exponential moving avarage
#define ABW_ALPHA 0.9

/////////////////////////// EndPt
ChestEndPt::ChestEndPt(): m_ostream(&std::cout, [](std::ostream*){}){};

void ChestEndPt::set_verbosity(int verbosity){
    m_verbose = verbosity;
}

void ChestEndPt::set_output_file(const std::string& filename){
    m_output_file = filename;
    m_ostream = std::move(std::unique_ptr<std::ostream, std::function<void(std::ostream*)>>
                (new std::ofstream(filename), std::default_delete<std::ostream>()));
    if (!dynamic_cast<std::ofstream&>(*m_ostream).is_open()){
        throw std::runtime_error("Failed to open file " + filename);
    }
}

void ChestEndPt::set_output_format(bool is_yaml){
    m_yaml_output = is_yaml;
}

/////////////////////////// Reciever
ChestReceiver::ChestReceiver(const ABReceiver& abw_receiver):
m_abw_receiver(abw_receiver.clone()){};

ChestReceiver::ChestReceiver(std::unique_ptr<ABReceiver>& abw_receiver):
m_abw_receiver(std::move(abw_receiver)) {};

void ChestReceiver::run(){
    if (m_abw_receiver->validate()){
        auto abet_res = std::async(std::launch::async, [this](){m_abw_receiver->run();});
    }
}

void ChestReceiver::cleanup(){
    m_abw_receiver -> cleanup();
}

/////////////////////////// Sender
ChestSender::ChestSender(const ABSender& abw_sender, Pinger& pinger,
                         const LossBase& losser, int measurment_gap):
m_abw_sender(abw_sender.clone()), m_pinger(pinger.to_unique_ptr()), m_losser(losser.clone()),
m_measurment_gap(measurment_gap), m_curr_abw_est(0)
{}

ChestSender::ChestSender(std::unique_ptr<ABSender>& abw_sender, Pinger& pinger,
                const LossBase& losser, int measurment_gap):
m_abw_sender(std::move(abw_sender)), m_pinger(pinger.to_unique_ptr()), m_losser(losser.clone()),
m_measurment_gap(measurment_gap), m_curr_abw_est(0)
{}


void ChestSender::cleanup(){
    m_abw_sender->cleanup();
}


void ChestSender::print_statistics(int runnum){
    if (m_yaml_output){
        print_stats_yaml(runnum);
    } else {
        print_stats_default(runnum);
    }
}


void ChestSender::print_stats_yaml(int runnum) const{
    static bool start = true;
    if (start){
        //ostr << "ChestRes:\n";
        start = false;
    }
    timeval iter_time = time_from_start();
    *m_ostream << "-   runnum    : " << runnum << '\n';
    *m_ostream << "    time      : " << iter_time.tv_sec << '.' << iter_time.tv_usec / 1000 <<  '\n';
    *m_ostream << "    abw       : " << m_curr_abw_est / 1000000.0  << '\n';
    *m_ostream << "    lastRtt   : " << m_ping_stats.get_last_rtt() / 1000. << '\n';
    *m_ostream << "    sRtt      : " << m_ping_stats.get_srtt() / 1000. << '\n';
    *m_ostream << "    jitter    : " << m_ping_stats.get_jitter() / 1000. << '\n';
    *m_ostream << "    loss_total: " << m_losser->get_total_loss_percentage() << '\n';
    auto local_loss = m_losser->get_local_loss_percentage();
    if (local_loss >= 0){
        *m_ostream << "    loss_local: " << local_loss << '\n';
    } else {
        *m_ostream << "    loss_local: null\n";
    }
    *m_ostream << std::endl;
}


void ChestSender::print_stats_default(int runnum) const{
    if (runnum != -1){
        timeval iter_time = time_from_start();
        *m_ostream << iter_time.tv_sec << '.' << iter_time.tv_usec / 1000 << ":"; 
        *m_ostream << "~~~Printing statistics for run " << runnum << "~~~\n";
    }
    *m_ostream << "Available bw estimation: " << m_curr_abw_est / 1000000.0 << " mbit/sec\n";
    *m_ostream << "Last RTT: " << m_ping_stats.get_last_rtt() / 1000. << "ms";
    *m_ostream << "; smoothed RTT: " << m_ping_stats.get_srtt() / 1000. << "ms";
    *m_ostream << "; jitter: " << m_ping_stats.get_jitter() / 1000. << "ms\n";
    *m_ostream << "Total loss percentage: " << m_losser->get_total_loss_percentage() << "%\n";
    auto local_loss = m_losser->get_local_loss_percentage();
    if (local_loss >= 0){
        *m_ostream << "Local loss percentage: " << local_loss << "%\n";
    }
    *m_ostream << std::endl;
}


void ChestSender::run(){
    setup();
    std::unique_ptr<std::list<MeasurementBundle>> 
    measurement_list = std::make_unique<std::list<MeasurementBundle>>();
    int runnum = 1;
    while(true){
        auto ping_res = std::async(std::launch::async, 
        [this](){ 
            return m_pinger->ping(); 
        });
        auto abet_res = std::async(std::launch::async, 
        [this, &measurement_list](){ 
            return abw_single_round(measurement_list.get()); 
        });
        
        abet_res.get();
        process_abw_round(measurement_list.get());
        process_ping_res(ping_res.get());
        
        print_statistics(runnum);
        measurement_list->clear();
        usleep(m_measurment_gap);   //FIXME: intra-stream sleep time
        runnum += 1;
    }
}


void ChestSender::setup(){
    setup_abw();
    gettimeofday(&m_time_start, 0);
    std::cerr << "Chest prepared!" << std::endl;
}


void ChestSender::setup_abw(){
    if (!m_abw_sender->validate()){
        throw std::runtime_error("Failed to validate");
    }

    try{
        m_abw_sender->setupRun();
    } catch (...)
    {
        std::cerr << "Failed to setup" << std::endl;
        cleanup();
        throw;
    }
}


void ChestSender::abw_single_round(std::list<MeasurementBundle>* mb_list){
    m_abw_sender->resetRound();
    bool done = false;
    std::list<MeasurementBundle> tmp_mb_list;
    while (!done){
        if (!m_abw_sender->doOneMeasurementRound(&tmp_mb_list)){
            //std::cerr << "!! Error collecting measurements from receiver" << std::endl;
            continue;
        }
        mb_list->insert(mb_list->end(), tmp_mb_list.begin(), tmp_mb_list.end());  // save results

        done = m_abw_sender->processOneRoundRes(&tmp_mb_list);   // clears tmp_mb_list
        
        //if (!done){
        //    sleepExponentially();   // retry
        //}
    }
    return;
}


void ChestSender::process_abw_round(std::list<MeasurementBundle> * mb_list){
    //std::cout << "Attempts for round:" << mb_list->size() << std::endl;
    m_curr_abw_est = m_abw_sender->get_current_estimation();
    //m_curr_abw_est = ABW_ALPHA * m_abw_sender->get_current_estimation() + (1 - ABW_ALPHA) * m_curr_abw_est;   // exponential moving average
    m_losser->process_answer(*mb_list);
    return;
}


void ChestSender::process_ping_res(const PingRes& ping_res){
    m_ping_stats.process_ping_res(ping_res, -1, false);
    m_losser->process_answer(ping_res);
}


timeval ChestSender::time_from_start() const{
    timeval abs_time, curr_time;
    gettimeofday(&abs_time, 0);
    timersub(&abs_time, &m_time_start, &curr_time);
    return curr_time;
}
