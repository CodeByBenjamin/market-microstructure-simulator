#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <iostream>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "Trader.h"
#include "UIHelpers.h"
#include "priceutils.h"
#include "rng.h"

LimitOrderBook::LimitOrderBook()
{
	this->orderPoll.reserve(100000);
	this->ordersToProcess.reserve(200);
}

std::optional<PriceTicks> LimitOrderBook::bestBid() const {
	if (bids.empty()) return std::nullopt;
	return bids.begin()->first;
}

std::optional<PriceTicks> LimitOrderBook::bestAsk() const {
	if (asks.empty()) return std::nullopt;
	return asks.begin()->first;
}

PriceTicks LimitOrderBook::midPrice() const {
	if (bids.empty() && asks.empty())
		return midPriceRecords.empty() ? toPriceTicks(20.0) : midPriceRecords.back();
	if (bids.empty())
		return asks.begin()->first;
	if (asks.empty())
		return bids.begin()->first;
	return (bids.begin()->first + asks.begin()->first) / 2;
}

const std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>>& LimitOrderBook::getBids() const
{
	return bids;
}

const std::map<PriceTicks, PriceLevel>& LimitOrderBook::getAsks() const
{
	return asks;
}

const Trader* LimitOrderBook::getTrader(TraderId id) const {
	auto it = traders.find(id);
	if (it != traders.end()) {
		return it->second;
	}
	return nullptr;
}

const std::unordered_map<TraderId, Trader*>& LimitOrderBook::getTraders() const {
	return traders;
}

Quantity LimitOrderBook::getHighestVolume(Side side, size_t priceLevels) const
{
	Quantity levelVol = 0;
	Quantity maxVol = 0;

	int count = 0;

	if (bids.empty() || asks.empty())
		return 0;

	if (side == Side::BUY)
	{
		for (auto it = bids.begin(); it != bids.end() && count < priceLevels; ++it, ++count) {

			levelVol = it->second.levelVolume;

			if (levelVol == 0)
			{
				count--;
				continue;
			}

			if (levelVol > maxVol) maxVol = levelVol;
		}
	}
	else
	{
		for (auto it = asks.begin(); it != asks.end() && count < priceLevels; ++it, ++count) {

			levelVol = it->second.levelVolume;

			if (levelVol == 0)
			{
				count--;
				continue;
			}

			if (levelVol > maxVol) maxVol = levelVol;
		}
	}

	return maxVol;
}

const Order* LimitOrderBook::getOrder(OrderId id) const
{
	auto it = orderLookup.find(id);

	if (it == orderLookup.end())
		return nullptr;

	if (orderPoll[it->second].id != id) 
		return nullptr;

	return &orderPoll[it->second];
}

void LimitOrderBook::update() {
	PriceTicks midTicks = 0;

	if (bids.empty() && asks.empty()) {
		midTicks = midPriceRecords.empty() ? toPriceTicks(20.0) : midPriceRecords.back();
	}
	else if (bids.empty()) {
		midTicks = asks.begin()->first;
	}
	else if (asks.empty()) {
		midTicks = bids.begin()->first;
	}
	else {
		midTicks = (bids.begin()->first + asks.begin()->first) / 2;
	}

	midPriceRecords.push_back(midTicks);
}

const std::vector<PriceTicks>& LimitOrderBook::getMidPriceHistory() const
{
	return midPriceRecords;
}

OrderResult LimitOrderBook::registerOrder(TraderId traderId, PriceTicks price, Quantity volume, Side side, Clock& clock)
{
	Order order = { 0, traderId, price, volume, side, clock.now() };

	auto it = traders.find(traderId);
	if (it == traders.end() || !it->second)
		return { 0, RejectReason::NoTrader };
	Trader* trader = it->second;

	if (order.side == Side::BUY)
	{
		PriceTicks cost;
		if (mul_overflow_i64(price, volume, cost))
			return { 0, RejectReason::Overflow };
		if (trader->getFunds() < cost)
		{
			return { 0, RejectReason::InsufficientFunds };
		}

		trader->lockFunds(cost);
	}
	else
	{
		if (trader->getStocks() < volume)
		{
			return { 0, RejectReason::InsufficientStocks };
		}

		trader->lockStocks(volume);
	}

	order.id = nextOrderId++;
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

	ordersToProcess.push_back(index);

	return { order.id, RejectReason::None };
}

void LimitOrderBook::processOrders(Clock& clock)
{
	while (!ordersToProcess.empty()) {
		std::uniform_int_distribution<size_t> dist(0, ordersToProcess.size() - 1);
		size_t pos = dist(rng);

		size_t orderIndex = ordersToProcess[pos];

		ordersToProcess[pos] = ordersToProcess.back();
		ordersToProcess.pop_back();

		executeMatch(orderIndex, clock);
	}
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

			auto& level = priceLevelIt->second;

			while (incomingOrder.volume > 0 && priceLevelIt->second.nextToMatch < priceLevelIt->second.orderEntries.size())
			{
				Order& restingOrder = orderPoll.at(level.orderEntries[level.nextToMatch].index);

				if (restingOrder.volume <= 0 || restingOrder.id != level.orderEntries[level.nextToMatch].id) {
					level.nextToMatch++;
					continue;
				}

				Quantity tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(incomingOrder, restingOrder, priceLevelIt->first, tradeVolume, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				priceLevelIt->second.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					auto it = traders.find(restingOrder.traderId);
					if (it != traders.end() && it->second) it->second->onOrderFinished(restingOrder.id);

					priceLevelIt->second.nextToMatch++;
				}
			}

			if (level.nextToMatch >= level.orderEntries.size()) {
				asks.erase(priceLevelIt);
			}
			else {
				break;
			}
		}
	}
	else
	{
		while (incomingOrder.volume > 0 && !bids.empty())
		{
			auto priceLevelIt = bids.begin();
			if (priceLevelIt->first < incomingOrder.price) break;

			auto& level = priceLevelIt->second;

			while (incomingOrder.volume > 0 && priceLevelIt->second.nextToMatch < priceLevelIt->second.orderEntries.size())
			{
				Order& restingOrder = orderPoll.at(level.orderEntries[level.nextToMatch].index);

				if (restingOrder.volume <= 0 || restingOrder.id != level.orderEntries[level.nextToMatch].id) {
					level.nextToMatch++;
					continue;
				}

				Quantity tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(restingOrder, incomingOrder, priceLevelIt->first, tradeVolume, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				priceLevelIt->second.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					auto it = traders.find(restingOrder.traderId);
					if (it != traders.end() && it->second) it->second->onOrderFinished(restingOrder.id);

					priceLevelIt->second.nextToMatch++;
				}
			}

			if (level.nextToMatch >= level.orderEntries.size()) {
				bids.erase(priceLevelIt);
			}
			else {
				break;
			}
		}
	}

	if (incomingOrder.volume > 0) {
		addLimitOrder(index);
	}
	else {
		incomingOrder.volume = 0;

		auto it = traders.find(incomingOrder.traderId);
		if (it != traders.end() && it->second) {
			it->second->onOrderFinished(incomingOrder.id);
		}

		freeSlots.push(index);
	}
}

bool LimitOrderBook::cancelOrder(OrderId orderId)
{
	auto indexIt = orderLookup.find(orderId);
	if (indexIt == orderLookup.end())
		return false;

	Order& orderToCancel = orderPoll[indexIt->second];

	if (orderId != orderToCancel.id || orderToCancel.volume == 0)
	{
		return false;
	}

	auto itT = traders.find(orderToCancel.traderId);
	if (itT == traders.end() || !itT->second) return false;
	Trader* trader = itT->second;

	if (orderToCancel.side == Side::BUY)
	{
		PriceTicks refund = 0;
		if (mul_overflow_i64(orderToCancel.price, orderToCancel.volume, refund)) return false;
		trader->unlockFunds(refund);
	}
	else
	{
		trader->unlockStocks(orderToCancel.volume);
	}

	if (orderToCancel.side == Side::BUY)
	{
		auto it = bids.find(orderToCancel.price);
		if (it != bids.end())
		{
			it->second.levelVolume -= orderToCancel.volume;

			if (it->second.levelVolume <= 0)
				bids.erase(it);
		}
	}
	else
	{
		auto it = asks.find(orderToCancel.price);
		if (it != asks.end())
		{
			it->second.levelVolume -= orderToCancel.volume;

			if (it->second.levelVolume <= 0)
				asks.erase(it);
		}
	}

	orderToCancel.volume = 0;
	trader->onOrderFinished(orderId);
	freeSlots.push(indexIt->second);
	orderLookup.erase(orderId);

	return true;
}

void LimitOrderBook::addLimitOrder(size_t index)
{
	Order& incomingOrder = orderPoll.at(index);

	if (incomingOrder.side == Side::BUY) {
		auto& level = bids[incomingOrder.price];
		
		if (level.priceLabel.empty())
		{
			level.priceLabel = UIHelper::formatPrice(incomingOrder.price);
		}

		level.orderEntries.push_back({ incomingOrder.id, index });
		level.levelVolume += incomingOrder.volume;
	}
	else
	{
		auto& level = asks[incomingOrder.price];

		if (level.priceLabel.empty())
		{
			level.priceLabel = UIHelper::formatPrice(incomingOrder.price);
		}

		level.orderEntries.push_back({ incomingOrder.id, index });
		level.levelVolume += incomingOrder.volume;
	}

	orderLookup[incomingOrder.id] = index;
}

void LimitOrderBook::registerTrader(Trader* trader) {
	traders[trader->getId()] = trader;
}

void LimitOrderBook::recordTrade(const Order& bidOrder, const Order& askOrder, PriceTicks price, Quantity volume, Clock& clock)
{
	TradeRecord tradeRecord = {};
	tradeRecord.buyerOrderId = bidOrder.id;
	tradeRecord.sellerOrderId = askOrder.id;
	tradeRecord.timeStamp = clock.now();
	tradeRecord.tradeId = nextTradeId++;
	tradeRecord.price = price;
	tradeRecord.volume = volume;
	pendingTrades.push_back(tradeRecord);

	auto itT = traders.find(bidOrder.traderId);
	if (itT == traders.end() || !itT->second) {
		std::cout << "FATAL: missing buyer trader\n";
		return;
	}
	Trader* buyer = itT->second;

	itT = traders.find(askOrder.traderId);
	if (itT == traders.end() || !itT->second) {
		std::cout << "FATAL: missing seller trader\n";
		return;
	}
	Trader* seller = itT->second;

	PriceTicks refund;
	if (mul_overflow_i64(bidOrder.price - price, volume, refund)) {
		std::cout << "FATAL: overflow in recordTrade\n";
		return;
	}

	PriceTicks cashExchanged;
	if (mul_overflow_i64(price, volume, cashExchanged)) {
		std::cout << "FATAL: overflow in recordTrade\n";
		return;
	}

	tradeCount++;

	tradePriceSum += price;

	if (buyer) {
		PriceTicks lockedUsed = 0;
		if (mul_overflow_i64(bidOrder.price, volume, lockedUsed)) {
			std::cout << "FATAL: overflow in recordTrade\n";
			return;
		}

		buyer->changeLockedFunds(-lockedUsed);
		buyer->unlockFunds(refund);
		buyer->changeStocks(volume);

		buyer->onTradeFilled(BUY, price, volume);
	}

	if (seller) {
		seller->changeLockedStocks(-volume);
		seller->changeFunds(cashExchanged);

		seller->onTradeFilled(SELL, price, volume);
	}
}

std::vector<TradeRecord> LimitOrderBook::flushTrades()
{
	std::vector<TradeRecord> trades = std::move(pendingTrades);
	pendingTrades = {};
	return trades;
}