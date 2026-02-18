#pragma once

#include <unordered_map>
#include <map>

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

	long id;
	double funds;
	long stocks;

	std::map<double, std::vector<long>> ordersByPrice;
	std::unordered_map<long, double> idToPrice;
public:
	Trader(TradeStrategy* strategy, long id, double funds, long stocks);

	long getId() const;
	double getFunds() const;
	double getStocks() const;
	const std::map<double, std::vector<long>>& getActiveOrderIds() const;
	size_t getOrderCount() const;

	void changeFunds(double funds);
	void changeStocks(long stocks);
	
	void update(LimitOrderBook& LOB, Clock& clock);

	void addActiveOrderId(long id, double price);
	void removeActiveOrderId(long id);
	void onOrderFinished(long id);
	void clearHalfOrders(LimitOrderBook& LOB);
};