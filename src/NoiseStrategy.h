#pragma once

#include "TradeStrategy.h"

class NoiseStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& lob, Clock& clock) override;
};