#include "loss.h"
#include <iostream>

//////////////// LossDumb ///////////////////
double LossDumb::get_total_loss_percentage() const{
    if (m_nsamples != 0){
        return 100. * m_nlost / m_nsamples;
    } else {
        return 0;
    }
}

double LossDumb::get_local_loss_percentage() const{
    return -1;
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


//////////////// LossElr ///////////////////
LossElr::LossElr(unsigned consistency_threshold, int tau_nsteps): m_tau_nsteps(tau_nsteps),
m_nlost(0), m_nsamples(0), m_consistency_threshold(consistency_threshold) 
{};

std::unique_ptr<LossBase> LossElr::clone() const {
    return std::make_unique<LossElr>(*this);
}
 

// used for delays, in general case can lead to overflows
int get_millisec(const timeval& tv){
    if (tv.tv_sec == -1){
        return -1;  
    }
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}


std::vector<LossElr::PktCount>& LossElr::get_pkt_count_vec(int delay){
    if (m_probabilities.count(delay) == 0){
        m_probabilities[delay] = std::vector<LossElr::PktCount>(m_tau_nsteps * 2 + 1);  // defaut construct
    }
    return m_probabilities[delay];
}


// TOCHECK...
// Also save delays for all non-lost packets in mb
void LossElr::count_stats(const MeasurementBundle& mb){
    const std::vector<timeval>& delay_vec = mb.m_delays_vec;
    int pkt_idx = 0;
    int n_packets = delay_vec.size();
    for (const auto& tv : delay_vec){
        int delay = get_millisec(tv);
        if (delay == -1){
            continue;   // skip lost packet
        }
        m_delay_vec.push_back(delay);
        // ..choose bucket.. (possible feature for 'delay resolution') //
        //std::cout << "delay " << delay << std::endl;
        std::vector<PktCount>& pkt_count_vec = get_pkt_count_vec(delay);    // CHECK: mb reference to local var
        for (int i = -m_tau_nsteps; i <= m_tau_nsteps; i++){
            int tmp_idx = pkt_idx + i;  // TOCHECK
            if (tmp_idx < 0 || tmp_idx >= n_packets){
                continue;   
            }
            
            if (delay_vec[tmp_idx].tv_sec == -1){
                pkt_count_vec[i+m_tau_nsteps].nlost += 1;
            }
            pkt_count_vec[i+m_tau_nsteps].ntotal += 1;
        }
        pkt_idx += 1;
    }
}


void LossElr::process_answer(const std::list<MeasurementBundle>& mb_list){
    m_delay_vec.clear();    // clear previous round res
    for (const auto& mb : mb_list){
        // space for parallelism
        m_nsamples += mb.m_remote_nsamples;
        m_nlost += mb.m_remote_nlost;
        // maybe save time start and time end, but not now
        count_stats(mb);
    }
}

void LossElr::process_answer(const PingRes& ping_res){
    m_nsamples += 1;
    if (ping_res.rtt == -1){
        m_nlost += 1;
    }
}


double LossElr::get_total_loss_percentage() const {
    if (m_nsamples != 0){
        return 100. * m_nlost / m_nsamples;
    } else {
        return 0;
    }
}


// right-rectangle formula; pkt_idx - index in m_delay_vec
double LossElr::compute_integral(int pkt_idx) const{
    auto pkt_count_vec_it = m_probabilities.find(m_delay_vec[pkt_idx]);
    if (pkt_count_vec_it == m_probabilities.end()){
        std::cout << "Error: new delay while compute_integral" << std::endl;
        return 0;
    }
    const std::vector<PktCount>& stat_vec = pkt_count_vec_it->second;
    
    double small_sum = 0;
    for (const auto& pkt_count: stat_vec){
        if (pkt_count.ntotal == 0 || pkt_count.nlost == 0){
            continue;
        }
        small_sum += (1. * pkt_count.nlost) / pkt_count.ntotal ;
    }
    return small_sum;
}


/*Formula (tex-like): 
Elr(n, N+, N-) = 1/n * sum_{i=1}^n{ 1/(N+ + N- + 1) * integral_-N-^N+{l_c(r,Y_i)dr} };
Elr(n, N+, N-) = 1/n * 1/(N+ + N- + 1) * sum_{i=1}^n{ integral_-N-^N+{l_c(r,Y_i)dr} }; // big_sum,
integral_-N-^N+{l_c(r,Y)dr}} = sum_{j=-N-}^N+{ l_c(j, Y) * 1 };   // right-rectangle formula
l_c(j, Y) = m_probabilities[Y][j].nlost / m_probabilities[Y][j].ntotal;

N+ and N- are assumed equal to m_tau_nsteps
*/
double LossElr::get_local_loss_percentage() const{
    if (m_nlost < m_consistency_threshold){
        return -1;
    }
    double big_sum = 0; 
    for (int i = 0; i < m_delay_vec.size(); i++){
        big_sum += compute_integral(i);
    }

    big_sum /= (m_delay_vec.size() * (m_tau_nsteps * 2 + 1));
    return big_sum * 100;   // to percentage
}


void LossElr::print_probabilities() const{
    for (const auto& elem: m_probabilities){
        std::cout << "Delay: " << elem.first << "ms" << std::endl;
        std::cout << '[';
        for (const auto& pkt_cnt: elem.second){
            std::cout << '{' << pkt_cnt.nlost << ", " << pkt_cnt.ntotal << "}, ";
        }
        std::cout << ']' << std::endl;
    }
}


LossElr::~LossElr(){
    std::cout << "Total samples: " << m_nsamples << std::endl;
} 
