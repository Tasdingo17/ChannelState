#ifndef __Losser__
#define __Losser__

#include "../ping/pinger.h"
#include "../abet/abet.h"
#include <memory>
#include <list>
#include <unordered_map>
#include <vector>

// packets
#define TAU_NSTEPS 5

// Elr stats consistency (lost packets)
#define ELR_CONSISTENCY_THRESHOLD 500

class LossBase{
public:
    virtual double get_total_loss_percentage() const = 0;
    virtual double get_local_loss_percentage() const = 0;
    virtual std::unique_ptr<LossBase> clone() const = 0;
    virtual void process_answer(const PingRes& ping_res) = 0;
    virtual void process_answer(const std::list<MeasurementBundle>& mb) = 0;
    virtual ~LossBase() = default;
};

class LossDumb: public LossBase{
public:
    LossDumb(): m_nlost(0), m_nsamples(0) {};
    virtual double get_total_loss_percentage() const override;
    virtual double get_local_loss_percentage() const override;
    virtual void process_answer(const PingRes& ping_res) override;
    virtual void process_answer(const std::list<MeasurementBundle>& mb_list) override;
    virtual std::unique_ptr<LossBase> clone() const override;
private:
    unsigned int m_nlost;
    unsigned int m_nsamples;
};


/* Elr but 'for packets': computing in (t - delta-; t + delta+] is very hard and could lead
* to bad performance, so we compute in [-Npackets,+Npackets].
*/ 
// Warning: stats can overflow (unsigned int) in about 2-3 months of continous work
class LossElr: public LossBase{
public:
    LossElr(unsigned consistency_threshold=ELR_CONSISTENCY_THRESHOLD, int tau_nsteps=TAU_NSTEPS);
    virtual double get_total_loss_percentage() const override;
    virtual double get_local_loss_percentage() const override;
    virtual std::unique_ptr<LossBase> clone() const override;
    virtual void process_answer(const std::list<MeasurementBundle>& mb_list) override;
    virtual void process_answer(const PingRes& ping_res) override;
    void print_probabilities() const;
    void fill_probs_random(unsigned int size=25);   // for debug

    struct PktCount{
        unsigned int nlost;
        unsigned int ntotal;
        PktCount(): nlost(0), ntotal(0){};
        PktCount(unsigned lost, unsigned total): nlost(lost), ntotal(total){};
    };
    // to yml format
    void serialize_to_file(const std::string& filename) const;

    // from yml format
    void deserialize_from_file(const std::string& filename);
private:
    unsigned int m_nlost;
    unsigned int m_nsamples;
    int m_tau_nsteps;  // packets
    unsigned int m_consistency_threshold;   // lost packets
    std::vector<int> m_delay_vec;

    /*OLD: delay(ms) : [t_send - 50*10, t_send -50*9, ..., t_send, t_send + 50, ... t_send + 50*10], elem {n_lost, n_total} */
    /*NEW: delay(ms) : [pkt_idx-5, pkt_idx-4, ..., pkt_idx, ..., pkt_idx+5], elem {n_lost, n_total}*/

    // Should be 'buckets' for delays but now every delay (ms) is a key
    using elr_probs = std::unordered_map<int, std::vector<PktCount>>;
    elr_probs m_probabilities;

    void count_stats(const MeasurementBundle& mb);
    std::vector<PktCount>& get_pkt_count_vec(int delay);
    double compute_integral(int pkt_idx) const;
};

#endif