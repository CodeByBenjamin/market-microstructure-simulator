#pragma once

#include "TradeStrategy.h"

class MarketMaker : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& lob, Clock& clock) override;
};