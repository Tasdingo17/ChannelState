#include "chest.h"
#include <thread>
#include <iostream>
#include <future>

// for exponential moving avarage
#define ABW_ALPHA 0.9

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


void ChestSender::print_statistics(int runnum, bool yaml){
    if (yaml){
        print_stats_yaml(runnum);
        return;
    }
    if (runnum != -1){
        timeval iter_time = time_from_start();
        std::cout << iter_time.tv_sec << '.' << iter_time.tv_usec / 1000 << ":"; 
        std::cout << "~~~Printing statistics for run " << runnum << "~~~\n";
    }
    std::cout << "Available bw estimation: " << m_curr_abw_est / 1000.0 << " mbit/sec\n";
    std::cout << "Last RTT: " << m_ping_stats.get_last_rtt() / 1000. << "ms";
    std::cout << "; smoothed RTT: " << m_ping_stats.get_srtt() / 1000. << "ms";
    std::cout << "; jitter: " << m_ping_stats.get_jitter() / 1000. << "ms\n";
    std::cout << "Total loss percentage: " << m_losser->get_total_loss_percentage() << "%\n";
    auto local_loss = m_losser->get_local_loss_percentage();
    if (local_loss >= 0){
        std::cout << "Local loss percentage: " << local_loss << "%\n";
    }
    std::cout << std::endl;
}

void ChestSender::print_stats_yaml(int runnum){
    static bool start = true;
    if (start){
        //std::cout << "ChestRes:\n";
        start = false;
    }
    timeval iter_time = time_from_start();
    std::cout << "-   runnum    : " << runnum << '\n';
    std::cout << "    time      : " << iter_time.tv_sec << '.' << iter_time.tv_usec / 1000 <<  '\n';
    std::cout << "    abw       : " << m_curr_abw_est / 1000.0  << '\n';
    std::cout << "    lastRtt   : " << m_ping_stats.get_last_rtt() / 1000. << '\n';
    std::cout << "    sRtt      : " << m_ping_stats.get_srtt() / 1000. << '\n';
    std::cout << "    jitter    : " << m_ping_stats.get_jitter() / 1000. << '\n';
    std::cout << "    loss_total: " << m_losser->get_total_loss_percentage() << '\n';
    auto local_loss = m_losser->get_local_loss_percentage();
    if (local_loss >= 0){
        std::cout << "    loss_local: " << local_loss << '\n';
    } else {
        std::cout << "    loss_local: null\n";
    }
    std::cout << std::endl;
}


void ChestSender::run(){
    setup();
    std::unique_ptr<std::list<MeasurementBundle>> 
    measurement_list = std::make_unique<std::list<MeasurementBundle>>();
    int runnum = 1;
    while(true){
        auto abet_res = std::async(std::launch::async, 
        [this, &measurement_list](){ 
            return abw_single_round(measurement_list.get()); 
        });
        auto ping_res = std::async(std::launch::async, 
        [this](){ 
            return m_pinger->ping(); 
        });
        
        
        abet_res.get();
        process_abw_round(measurement_list.get());
        process_ping_res(ping_res.get());
        
        print_statistics(runnum, true);
        measurement_list->clear();
        usleep(m_measurment_gap);   //FIXME: intra-stream sleep time
        runnum += 1;
        //if (runnum >= 20){
        //    break;
        //}
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
