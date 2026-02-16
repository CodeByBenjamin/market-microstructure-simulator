#pragma once

#include "LimitOrderBook.h"
#include "TradeStrategy.h"
#include "Trader.h"
#include "Clock.h"

class RandomStrategy : public TradeStrategy
{
public:
	void decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) override;
};