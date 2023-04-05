#ifndef __ChEst__
#define __ChEst__

#include "abet/abet.h"
#include "ping/pinger.h"
#include "loss/loss.h"
#include <memory>
#include <iostream>
#include <functional>

// micriseconds
#define DEFAULT_MEASURMENT_GAP 100000

class ChestEndPt{
public:
    ChestEndPt();
    virtual void run() = 0;
    void set_verbosity(int);
    void set_output_file(const std::string& filename);
    void set_output_format(bool is_yaml=false);
    virtual ~ChestEndPt() = default;
protected:
    int m_verbose;
    std::string m_output_file;
    bool m_yaml_output;
    std::unique_ptr<std::ostream, std::function<void(std::ostream*)>> m_ostream;
};


class ChestReceiver: public ChestEndPt{
public:
    ChestReceiver(const ABReceiver& abw_receiver);
    ChestReceiver(std::unique_ptr<ABReceiver>& abw_receiver);
    virtual void run() override;
private:
    void cleanup();
    std::unique_ptr<ABReceiver> m_abw_receiver;
};


class ChestSender : public ChestEndPt{
public:
    ChestSender(const ABSender& abw_sender, Pinger& pinger,
                const LossBase& losser, int measurment_gap=DEFAULT_MEASURMENT_GAP);
    ChestSender(std::unique_ptr<ABSender>& abw_sender, Pinger& pinger,
                const LossBase& losser, int measurment_gap=DEFAULT_MEASURMENT_GAP);
    virtual void run() override;
    void print_statistics(int runnum=-1);

    const ABSender* get_abw_sender() const;
    const Pinger* get_pinger() const;
    const LossBase* get_losser() const;
private:
    std::unique_ptr<ABSender> m_abw_sender;
    std::unique_ptr<Pinger> m_pinger;
    std::unique_ptr<LossBase> m_losser;
    int m_measurment_gap;
    PingStat m_ping_stats;
    float m_curr_abw_est;   // bytes/sec
    timeval m_time_start;

    void chest_sender_single_round(std::unique_ptr<std::list<MeasurementBundle>>&, int runnum=-1);
    void abw_single_round(std::list<MeasurementBundle> *);
    void setup();
    void setup_abw();
    void cleanup();
    void process_abw_round(std::list<MeasurementBundle> *);
    void process_ping_res(const PingRes& ping_res);
    void print_stats_yaml(int runnum) const;
    void print_stats_default(int runnum) const;
    timeval time_from_start() const;
};

#endif