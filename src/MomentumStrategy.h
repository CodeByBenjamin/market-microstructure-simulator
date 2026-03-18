#pragma once

#include "TradeStrategy.h"

class MomentumStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& lob, Clock& clock) override;
};