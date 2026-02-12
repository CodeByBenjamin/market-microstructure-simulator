#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <utility>
#include <cmath>
#include <string>
#include <chrono>
#include <iostream>

#include "datatypes.h"
#include "UIHelpers.h"
#include "LimitOrderBook.h"
#include "Clock.h"

static double roundToTick(double price, double tickSize = 0.01) {
	return std::round(price / tickSize) * tickSize;
}

const std::map<double, std::list<Order>, std::greater<double>>& LimitOrderBook::getBids() const
{
	return bids;
}

const std::map<double, std::list<Order>>& LimitOrderBook::getAsks() const
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
	{
		return 0;
	}

	if (side == Side::BUY)
	{
		for (auto it = bids.begin(); it != bids.end() && count < priceLevels; ++it, ++count) {
			onePriceVol = 0;
			for (const auto& order : it->second) onePriceVol += order.volume;
			if (onePriceVol > maxVol) maxVol = onePriceVol;
		}
	}
	else
	{
		for (auto it = asks.begin(); it != asks.end() && count < priceLevels; ++it, ++count) {
			onePriceVol = 0;
			for (const auto& order : it->second) onePriceVol += order.volume;
			if (onePriceVol > maxVol) maxVol = onePriceVol;
		}
	}

	return maxVol;
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

long LimitOrderBook::processOrder(const Order& incomingOrder, Clock& clock)
{
	Order order = incomingOrder;
	order.id = nextOrderId++;

	order.price = roundToTick(order.price);

	if (order.side == Side::BUY) {
		if (!asks.empty() && order.price >= asks.begin()->first)
			executeMatch(order, clock);
		else
			addLimitOrder(order);
	}
	else
	{
		if (!bids.empty() && order.price <= bids.begin()->first)
			executeMatch(order, clock);
		else
			addLimitOrder(order);
	}

	return order.id;
}

void LimitOrderBook::executeMatch(Order& incomingOrder, Clock& clock)
{
	if (incomingOrder.side == Side::BUY) 
	{
		while (incomingOrder.volume > 0 && !asks.empty())
		{
			auto priceLevelIt = asks.begin();

			if (priceLevelIt->first > incomingOrder.price) break;

			std::list<Order>& priceList = priceLevelIt->second;
			while (incomingOrder.volume > 0 && !priceList.empty())
			{
				Order& restingOrder = priceList.front();
				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(incomingOrder, restingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;



				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				if (restingOrder.volume == 0) {
					orderLookup.erase(restingOrder.id);
					priceList.pop_front();
				}
			}

			if (priceList.empty()) {
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

			std::list<Order>& priceList = priceLevelIt->second;
			while (incomingOrder.volume > 0 && !priceList.empty())
			{
				Order& restingOrder = priceList.front();
				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(restingOrder, incomingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				if (restingOrder.volume == 0) {
					orderLookup.erase(restingOrder.id);
					priceList.pop_front();
				}
			}

			if (priceList.empty()) {
				bids.erase(priceLevelIt);
			}
		}
	}

	if (incomingOrder.volume > 0) {
		addLimitOrder(incomingOrder);
	}
}

bool LimitOrderBook::cancelOrder(long orderId)
{
	auto mapIt = orderLookup.find(orderId);

	if (mapIt == orderLookup.end()) {
		return false;
	}

	std::list<Order>::iterator listIt = mapIt->second;
	const Order& orderToCancel = *listIt;

	if (orderToCancel.side == Side::BUY) {
		auto priceLevelIt = bids.find(orderToCancel.price);

		if (priceLevelIt != bids.end()) {
			priceLevelIt->second.erase(listIt);
			if (priceLevelIt->second.empty()) {
				bids.erase(priceLevelIt);
			}
		}
	}
	else {
		auto priceLevelIt = asks.find(orderToCancel.price);

		if (priceLevelIt != asks.end()) {
			priceLevelIt->second.erase(listIt);
			if (priceLevelIt->second.empty()) {
				asks.erase(priceLevelIt);
			}
		}
	}

	orderLookup.erase(mapIt);
	return true;
}

void LimitOrderBook::registerTrader(Trader* trader) {
	traders[trader->getId()] = trader;
}

void LimitOrderBook::addLimitOrder(Order incomingOrder)
{
	if (incomingOrder.side == Side::BUY) {
		auto priceLevelIt = bids.find(incomingOrder.price);

		if (priceLevelIt == bids.end()) {
			auto result = bids.emplace(incomingOrder.price, std::list<Order>{});
			priceLevelIt = result.first;
		}

		std::list<Order>::iterator newOrderIt = priceLevelIt->second.insert(
			priceLevelIt->second.end(),
			std::move(incomingOrder)
		);
		orderLookup.emplace(newOrderIt->id, newOrderIt);
	}
	else
	{
		auto priceLevelIt = asks.find(incomingOrder.price);

		if (priceLevelIt == asks.end()) {
			auto result = asks.emplace(incomingOrder.price, std::list<Order>{});
			priceLevelIt = result.first;
		}

		std::list<Order>::iterator newOrderIt = priceLevelIt->second.insert(
			priceLevelIt->second.end(),
			std::move(incomingOrder)
		);
		orderLookup.emplace(newOrderIt->id, newOrderIt);
	}
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