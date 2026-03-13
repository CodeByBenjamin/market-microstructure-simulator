#pragma once

#include "TradeStrategy.h"

class MomentumStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) override;
};