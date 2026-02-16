#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <stack>

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
	//Functioning
	std::map<double, PriceLevel, std::greater<double>> bids;
	std::map<double, PriceLevel> asks;
	std::unordered_map<long, Trader*> traders;

	std::unordered_map<long, size_t> orderLookup;

	std::vector<Order> orderPoll;
	std::stack<size_t> freeSlots;

	long nextOrderId = 1;

	double lastTradePrice = 0.0;
	std::vector<TradeRecord> pendingTrades;
	std::vector<double> midPriceRecords;

	long nextTradeId = 1;

	//Outside Info
	long totalBidVolume = 0;
	long totalAskVolume = 0;
public:
	LimitOrderBook();

	const std::map<double, PriceLevel, std::greater<double>>& getBids() const;
	const std::map<double, PriceLevel>& getAsks() const;
	const Trader* getTrader(long id) const;
	long getHighestVolume(Side side, size_t priceLevels) const;
	const Order* getOrder(long id) const;

	void update();

	const std::vector<double>& getMidPriceHistory() const;

	long processOrder(long traderId, double price, long volume, Side side, Clock& clock);
	void executeMatch(size_t index, Clock& clock);
	void addLimitOrder(size_t index);
	void cancelOrder(long orderId);

	void registerTrader(Trader* trader);

	void recordTrade(const Order& restingOrder, const Order& incomingOrder, long volume, double price, Clock& clock);
	std::vector<TradeRecord> flushTrades();
};