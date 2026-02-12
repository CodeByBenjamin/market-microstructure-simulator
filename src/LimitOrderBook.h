#pragma once

#include <map>
#include <unordered_map>
#include <list>
#include <vector>

#include "datatypes.h"
#include "Clock.h"
#include "Trader.h"

namespace sf {
    class RenderWindow;
    class Font;
}

class LimitOrderBook
{
private:
	std::map<double, std::list<Order>, std::greater<double>> bids;
	std::map<double, std::list<Order>> asks;
	std::unordered_map<long, Trader*> traders;

	std::unordered_map<long, std::list<Order>::iterator> orderLookup;

	long nextOrderId = 1;

	double lastTradePrice = 0.0;
	std::vector<TradeRecord> pendingTrades;
	std::vector<double> midPriceRecords;

	long nextTradeId = 1;
public:

	const std::map<double, std::list<Order>, std::greater<double>>& getBids() const;
	const std::map<double, std::list<Order>>& getAsks() const;
	const Trader* getTrader(long id) const;
	long getHighestVolume(Side side, size_t priceLevels) const;

	void update();

	const std::vector<double>& getMidPriceHistory() const;

	long processOrder(const Order& incomingOrder, Clock& clock);
	void executeMatch(Order& incomingOrder, Clock& clock);
	void addLimitOrder(Order incomingOrder);
	bool cancelOrder(long orderId);

	void registerTrader(Trader* trader);

	void recordTrade(const Order& restingOrder, const Order& incomingOrder, long volume, double price, Clock& clock);
	std::vector<TradeRecord> flushTrades();
};