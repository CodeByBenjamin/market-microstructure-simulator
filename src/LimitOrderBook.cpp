#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <utility>
#include <cmath>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "Trader.h"

LimitOrderBook::LimitOrderBook()
{
	this->orderPoll.reserve(100000);
}

static double roundToTick(double price, double tickSize = 0.01) {
	return std::round(price / tickSize) * tickSize;
}

const std::map<double, PriceLevel, std::greater<double>>& LimitOrderBook::getBids() const
{
	return bids;
}

const std::map<double, PriceLevel>& LimitOrderBook::getAsks() const
{
	return asks;
}

const Trader* LimitOrderBook::getTrader(long id) const {
	auto it = traders.find(id);
	if (it != traders.end()) {
		return it->second;
	}
	return nullptr;
}

long LimitOrderBook::getHighestVolume(Side side, size_t priceLevels) const
{
	long onePriceVol = 0;
	long maxVol = 0;

	int count = 0;

	if (bids.empty() || asks.empty())
		return 0;

	if (side == Side::BUY)
	{
		for (auto it = bids.begin(); it != bids.end() && count < priceLevels; ++it, ++count) {
			onePriceVol = 0;
			for (const auto& entry : it->second.orderEntries)
			{
				const Order& restingOrder = orderPoll[entry.index];

				if (restingOrder.id == entry.id && restingOrder.price == it->first) {
					onePriceVol += restingOrder.volume;
				}
			}
			if (onePriceVol > maxVol) maxVol = onePriceVol;
		}
	}
	else
	{
		for (auto it = asks.begin(); it != asks.end() && count < priceLevels; ++it, ++count) {
			onePriceVol = 0;
			for (const auto& entry : it->second.orderEntries)
			{
				const Order& restingOrder = orderPoll[entry.index];

				if (restingOrder.id == entry.id && restingOrder.price == it->first) {
					onePriceVol += restingOrder.volume;
				}
			}
			if (onePriceVol > maxVol) maxVol = onePriceVol;
		}
	}

	return maxVol;
}

const Order* LimitOrderBook::getOrder(long id) const
{
	auto it = orderLookup.find(id);

	if (it == orderLookup.end())
		return nullptr;

	if (orderPoll[it->second].id != id) 
		return nullptr;

	return &orderPoll[it->second];
}

void LimitOrderBook::update() {
	double midPrice;

	if (bids.empty() && asks.empty()) {
		midPrice = midPriceRecords.empty() ? 20.0 : midPriceRecords.back();
	}
	else if (bids.empty()) {
		midPrice = asks.begin()->first;
	}
	else if (asks.empty()) {
		midPrice = bids.begin()->first;
	}
	else {
		midPrice = (bids.begin()->first + asks.begin()->first) / 2.0;
	}

	midPriceRecords.push_back(midPrice);
}

const std::vector<double>& LimitOrderBook::getMidPriceHistory() const
{
	return midPriceRecords;
}

long LimitOrderBook::processOrder(long traderId, double price, long volume, Side side, Clock& clock)
{
	Order order = { 0, traderId, price, volume, side, clock.now() };
	order.id = nextOrderId++;
	order.price = roundToTick(order.price);

	size_t index = 0;

	if (freeSlots.empty())
	{
		orderPoll.push_back(order);
		index = orderPoll.size() - 1;
	}
	else
	{
		index = freeSlots.top();
		freeSlots.pop();
		orderPoll.at(index) = order;
	}

	executeMatch(index, clock);

	return order.id;
}

void LimitOrderBook::executeMatch(size_t index, Clock& clock)
{
	Order& incomingOrder = orderPoll.at(index);

	if (incomingOrder.side == Side::BUY) 
	{
		while (incomingOrder.volume > 0 && !asks.empty())
		{
			auto priceLevelIt = asks.begin();

			if (priceLevelIt->first > incomingOrder.price) break;

			std::vector<OrderEntry>& priceList = priceLevelIt->second.orderEntries;
			while (incomingOrder.volume > 0 && priceLevelIt->second.nextToMatch < priceLevelIt->second.orderEntries.size())
			{
				Order& restingOrder = orderPoll.at(priceList[priceLevelIt->second.nextToMatch].index);
				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				if (tradeVolume == 0 || restingOrder.id != priceList[priceLevelIt->second.nextToMatch].id) {
					priceLevelIt->second.nextToMatch++;
					continue;
				}

				recordTrade(incomingOrder, restingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(priceList[priceLevelIt->second.nextToMatch].index);

					priceLevelIt->second.nextToMatch++;
				}

			}

			if (priceLevelIt->second.nextToMatch >= priceLevelIt->second.orderEntries.size()) {
				asks.erase(priceLevelIt);
			}
		}
	}
	else
	{
		while (incomingOrder.volume > 0 && !bids.empty())
		{
			auto priceLevelIt = bids.begin();

			if (priceLevelIt->first < incomingOrder.price) break;

			std::vector<OrderEntry>& priceList = priceLevelIt->second.orderEntries;
			while (incomingOrder.volume > 0 && priceLevelIt->second.nextToMatch < priceLevelIt->second.orderEntries.size())
			{
				Order& restingOrder = orderPoll.at(priceList[priceLevelIt->second.nextToMatch].index);
				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				if (tradeVolume == 0 || restingOrder.id != priceList[priceLevelIt->second.nextToMatch].id) {
					priceLevelIt->second.nextToMatch++;
					continue;
				}

				recordTrade(restingOrder, incomingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(priceList[priceLevelIt->second.nextToMatch].index);

					priceLevelIt->second.nextToMatch++;
				}
			}

			if (priceLevelIt->second.nextToMatch >= priceLevelIt->second.orderEntries.size()) {
				bids.erase(priceLevelIt);
			}
		}
	}

	if (incomingOrder.volume > 0) {
		addLimitOrder(index);
	}
	else {
		 freeSlots.push(index);
	}
}

void LimitOrderBook::cancelOrder(long orderId)
{
	auto indexIt = orderLookup.find(orderId);

	if (indexIt == orderLookup.end())
		return;

	Order& orderToCancel = orderPoll[indexIt->second];

	orderToCancel.volume = 0;
	orderLookup.erase(orderId);
	freeSlots.push(indexIt->second);
}

void LimitOrderBook::addLimitOrder(size_t index)
{
	Order& incomingOrder = orderPoll.at(index);

	if (incomingOrder.side == Side::BUY) {
		auto& level = bids[incomingOrder.price];
		level.orderEntries.push_back({ incomingOrder.id, index });
	}
	else
	{
		auto& level = asks[incomingOrder.price];
		level.orderEntries.push_back({ incomingOrder.id, index });
	}

	orderLookup[incomingOrder.id] = index;
}

void LimitOrderBook::registerTrader(Trader* trader) {
	traders[trader->getId()] = trader;
}

void LimitOrderBook::recordTrade(const Order& bidOrder, const Order& askOrder, long volume, double price, Clock& clock)
{
	TradeRecord tradeRecord = {};
	tradeRecord.buyerOrderId = bidOrder.traderId;
	tradeRecord.sellerOrderId = askOrder.traderId;
	tradeRecord.timeStamp = clock.now();
	tradeRecord.tradeId = nextTradeId++;
	tradeRecord.price = price;
	tradeRecord.volume = volume;
	pendingTrades.push_back(tradeRecord);

	Trader* buyer = traders[bidOrder.traderId];
	Trader* seller = traders[askOrder.traderId];

	double cashExchanged = price * volume;

	if (buyer) {
		buyer->changeFunds(-cashExchanged);
		buyer->changeStocks(volume);
	}

	if (seller) {
		seller->changeFunds(cashExchanged);
		seller->changeStocks(-volume);
	}
}

std::vector<TradeRecord> LimitOrderBook::flushTrades()
{
	std::vector<TradeRecord> trades = std::move(pendingTrades);
	pendingTrades.clear();
	return trades;
}