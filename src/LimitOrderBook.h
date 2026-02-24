#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <stack>
#include <optional>

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
	std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>> bids;
	std::map<PriceTicks, PriceLevel> asks;
	std::unordered_map<TraderId, Trader*> traders;

	std::unordered_map<OrderId, size_t> orderLookup;

	std::vector<Order> orderPoll;
	std::stack<size_t> freeSlots;

	long nextOrderId = 1;

	int64_t lastTradePrice = 0;
	std::vector<TradeRecord> pendingTrades;
	std::vector<PriceTicks> midPriceRecords;

	long nextTradeId = 1;

	//Outside Info
	long totalBidVolume = 0;
	long totalAskVolume = 0;
public:
	LimitOrderBook();

	std::optional<PriceTicks> bestBid() const;
	std::optional<PriceTicks> bestAsk() const;
	PriceTicks midPrice() const;

	const std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>>& getBids() const;
	const std::map<PriceTicks, PriceLevel>& getAsks() const;
	const Trader* getTrader(TraderId id) const;
	Quantity getHighestVolume(Side side, size_t priceLevels) const;
	const Order* getOrder(OrderId id) const;

	void update();

	const std::vector<PriceTicks>& getMidPriceHistory() const;

	OrderResult processOrder(TraderId traderId, PriceTicks price, Quantity volume, Side side, Clock& clock);
	bool executeMatch(size_t index, Clock& clock);
	void addLimitOrder(size_t index);
	bool cancelOrder(OrderId orderId);

	void registerTrader(Trader* trader);

	void recordTrade(const Order& restingOrder, const Order& incomingOrder, Quantity volume, PriceTicks price, Clock& clock);
	std::vector<TradeRecord> flushTrades();
};