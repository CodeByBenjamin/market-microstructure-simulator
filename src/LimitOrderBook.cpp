#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <cmath>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "Trader.h"
#include "UIHelpers.h"

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
	long levelVol = 0;
	long maxVol = 0;

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

bool LimitOrderBook::processOrder(long* id, long traderId, double price, long volume, Side side, Clock& clock)
{
	Order order = { 0, traderId, price, volume, side, clock.now() };
	order.price = roundToTick(order.price);

	Trader* trader = traders[traderId];

	if (order.side == Side::BUY)
	{
		if (trader->getFunds() < price * volume)
			return false;

		trader->changeFunds(-price * volume);
	}
	else
	{
		if (trader->getStocks() < volume)
			return false;

		trader->changeStocks(-volume);
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

	if (id)
		*id = order.id;

	executeMatch(index, clock);
	return true;
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

				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(incomingOrder, restingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				priceLevelIt->second.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					traders[restingOrder.traderId]->onOrderFinished(restingOrder.id);

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

				long tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(restingOrder, incomingOrder, tradeVolume, priceLevelIt->first, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				priceLevelIt->second.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0)
				{
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					traders[restingOrder.traderId]->onOrderFinished(restingOrder.id);

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
		 freeSlots.push(index);
	}
}

bool LimitOrderBook::cancelOrder(long orderId)
{
	auto indexIt = orderLookup.find(orderId);
	if (indexIt == orderLookup.end())
		return false;

	Order& orderToCancel = orderPoll[indexIt->second];

	if (orderId != orderToCancel.id || orderToCancel.volume == 0)
	{
		return false;
	}

	Trader* trader = traders[orderToCancel.traderId];

	if (orderToCancel.side == Side::BUY)
	{
		trader->changeFunds(orderToCancel.price * orderToCancel.volume);
	}
	else
	{
		trader->changeStocks(orderToCancel.volume);
	}

	if (orderToCancel.side == Side::BUY)
	{
		const auto& priceLevelIt = bids.find(orderToCancel.price);
		if (priceLevelIt != bids.end())
		{
			priceLevelIt->second.levelVolume -= orderToCancel.volume;
		}
	}
	else
	{
		const auto& priceLevelIt = asks.find(orderToCancel.price);
		if (priceLevelIt != asks.end())
		{
			priceLevelIt->second.levelVolume -= orderToCancel.volume;
		}
	}

	orderToCancel.volume = 0;
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

	double refund = (bidOrder.price - price) * volume;
	double cashExchanged = price * volume;

	if (buyer) {
		buyer->changeStocks(volume);
		buyer->changeFunds(refund);
	}

	if (seller) {
		seller->changeFunds(cashExchanged);
	}
}

std::vector<TradeRecord> LimitOrderBook::flushTrades()
{
	std::vector<TradeRecord> trades = std::move(pendingTrades);
	pendingTrades.clear();
	return trades;
}