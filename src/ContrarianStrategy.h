#pragma once

#include "TradeStrategy.h"

class ContrarianStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) override;
};