#pragma once

#include "TradeStrategy.h"

class FundamentalStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) override;
};