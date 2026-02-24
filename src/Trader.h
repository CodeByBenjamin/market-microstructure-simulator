#pragma once

#include <unordered_map>
#include <map>
#include <vector>

#include "datatypes.h"
#include "TradeStrategy.h"

enum TraderType
{
	Sentiment,
	Contrarian,
	LongTerm,
	Whale
};

class Trader {
private:
	TradeStrategy* strategy;

	TraderId id;
	PriceTicks funds;
	Quantity stocks;

	std::map<PriceTicks, std::vector<OrderId>> ordersByPrice;
	std::unordered_map<OrderId, PriceTicks> idToPrice;
public:
	Trader(TradeStrategy* strategy, TraderId id, PriceTicks funds, Quantity stocks);

	TraderId getId() const;
	PriceTicks getFunds() const;
	Quantity getStocks() const;
	const std::map<PriceTicks, std::vector<OrderId>>& getActiveOrderIds() const;
	size_t getOrderCount() const;

	void changeFunds(PriceTicks funds);
	void changeStocks(Quantity stocks);
	
	void update(LimitOrderBook& LOB, Clock& clock);

	void addActiveOrderId(OrderId id, PriceTicks price);
	void removeActiveOrderId(OrderId id);
	void onOrderFinished(OrderId id);
	void clearHalfOrders(LimitOrderBook& LOB);
};