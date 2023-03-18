#include "chest.h"
#include <thread>
#include <iostream>
#include <future>

/////////////////////////// Reciever
ChestReceiver::ChestReceiver(const ABReceiver& abw_receiver):
m_abw_receiver(abw_receiver.clone()){};

ChestReceiver::ChestReceiver(std::unique_ptr<ABReceiver>& abw_receiver):
m_abw_receiver(std::move(abw_receiver)) {};

void ChestReceiver::run(){
    std::cout << "In ChestReceiver::run" << std::endl;
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
m_measurment_gap(measurment_gap)
{}

ChestSender::ChestSender(std::unique_ptr<ABSender>& abw_sender, Pinger& pinger,
                const LossBase& losser, int measurment_gap):
m_abw_sender(std::move(abw_sender)), m_pinger(pinger.to_unique_ptr()), m_losser(losser.clone()),
m_measurment_gap(measurment_gap)
{}


void ChestSender::cleanup(){
    m_abw_sender->cleanup();
}


void ChestSender::print_statistics(int runnum){
    // TODO: case for -1 and other
    if (runnum != -1){
        std::cout << "~~~Printing statistics for run " << runnum << "~~~" << std::endl;
    }
    std::cout << "Available bw estimation: " << m_abw_sender->get_current_estimation() / 1000.0;
    std::cout << " mbit/sec" << std::endl;
    std::cout << "Last RTT: " << m_ping_stats.get_last_rtt() / 1000. << "ms";
    std::cout << "; smoothed RTT: " << m_ping_stats.get_srtt() / 1000. << "ms";
    std::cout << "; jitter: " << m_ping_stats.get_jitter() / 1000. << "ms" << std::endl;
    std::cout << "Loss percentage: " << m_losser->get_loss_percentage() << '%' << std::endl;
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
        //process_ping_res(ping_res.get());
        
        auto abet_res = std::async(std::launch::async, 
        [this, &measurement_list](){ 
            return abw_single_round(measurement_list.get()); 
        });
        abet_res.get();
        process_abw_round(measurement_list.get());
        
        print_statistics(runnum);
        measurement_list->clear();
        usleep(m_measurment_gap);   //FIXME: intra-stream sleep time
        runnum += 1;
    }
}


void ChestSender::setup(){
    setup_abw();
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
    if (!m_abw_sender->doOneMeasurementRound(mb_list))
    {
        std::cerr << "!! persistent error collecting measurements from receiver" << std::endl;
        throw -1;
    }
    return;
}


void ChestSender::process_abw_round(std::list<MeasurementBundle> * mb_list){
    m_abw_sender->processOneRoundRes(mb_list);
    m_losser->process_answer(*mb_list);
    return;
}


void ChestSender::process_ping_res(const PingRes& ping_res){
    m_ping_stats.process_ping_res(ping_res, -1, false);
    m_losser->process_answer(ping_res);
}
