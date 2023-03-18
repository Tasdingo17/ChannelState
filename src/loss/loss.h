#ifndef __Losser__
#define __Losser__

#include "../ping/pinger.h"
#include "../abet/abet.h"
#include <memory>
#include <list>

class LossBase{
public:
    virtual double get_loss_percentage() const = 0;
    virtual std::unique_ptr<LossBase> clone() const = 0;
    virtual void process_answer(const PingRes& ping_res) = 0;
    virtual void process_answer(const std::list<MeasurementBundle>& mb) = 0;
    virtual ~LossBase() = default;
};

class LossDumb: public LossBase{
public:
    virtual double get_loss_percentage() const override;
    virtual void process_answer(const PingRes& ping_res) override;
    virtual void process_answer(const std::list<MeasurementBundle>& mb_list) override;
    virtual std::unique_ptr<LossBase> clone() const override;
private:
    unsigned int m_nlost;
    unsigned int m_nsamples;
};

#endif