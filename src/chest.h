#ifndef __ChEst__
#define __ChEst__

#include "abet/abet.h"
#include "ping/pinger.h"
#include "loss/loss.h"
#include <memory>

// micriseconds
#define DEFAULT_MEASURMENT_GAP 100000

class ChestEndPt{
public:
    virtual void run() = 0;
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
    void print_statistics(int runnum=-1, bool yaml=false);
    //void compute_loss();
private:
    std::unique_ptr<ABSender> m_abw_sender;
    std::unique_ptr<Pinger> m_pinger;
    std::unique_ptr<LossBase> m_losser;
    int m_measurment_gap;
    PingStat m_ping_stats;
    float m_curr_abw_est;   // bytes/sec
    timeval m_time_start;

    void abw_single_round(std::list<MeasurementBundle> *);
    void setup();
    void setup_abw();
    void cleanup();
    void process_abw_round(std::list<MeasurementBundle> *);
    void process_ping_res(const PingRes& ping_res);
    void print_stats_yaml(int runnum);
    timeval time_from_start() const;
};

#endif