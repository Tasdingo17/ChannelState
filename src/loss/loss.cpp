#include "loss.h"
#include <iostream>

double LossDumb::get_loss_percentage() const{
    return 100. * m_nlost / m_nsamples;
}

void LossDumb::process_answer(const PingRes& ping_res){
    m_nsamples += 1;
    if (ping_res.rtt == -1){
        m_nlost += 1;
    }
}

void LossDumb::process_answer(const std::list<MeasurementBundle>& mb_list){
    for (const auto& mb: mb_list){
        //std::cout << "!!!Losser debug! mb.m_remote_nlost:" << mb.m_remote_nlost;
        //std::cout << " mb_remote_nsamples:" << mb.m_remote_nsamples << std::endl;
        m_nsamples += mb.m_remote_nsamples;
        m_nlost += mb.m_remote_nlost;
    }
}

std::unique_ptr<LossBase> LossDumb::clone() const{
    return std::make_unique<LossDumb>(*this);
}