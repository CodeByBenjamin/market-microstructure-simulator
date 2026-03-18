#pragma once

class Trader;
class LimitOrderBook;
class Clock;

class TradeStrategy
{
public:
	virtual ~TradeStrategy() = default;
	virtual void decide(Trader& trader, LimitOrderBook& lob, Clock& clock) = 0;
};