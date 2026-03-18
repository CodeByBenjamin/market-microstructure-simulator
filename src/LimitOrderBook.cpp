#include <map>
#include <vector>
#include <algorithm>
#include <iostream>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "Trader.h"
#include "UIHelpers.h"
#include "priceutils.h"
#include "rng.h"

// LimitOrderBook matching engine
// Matching model:
// - Incoming orders are queued in pendingOrderIndices
// - Orders are processed in randomized order per simulation step
// - Matching occurs against best opposing price levels
// - Filled/cancelled orders remain in level.orderEntries and are skipped lazily
// - orderPool uses slot recycling via freeSlots for O(1) reuse

// Preallocate common simulation capacities to reduce reallocations
LimitOrderBook::LimitOrderBook() {
	orderPool.reserve(100000);
	pendingOrderIndices.reserve(200);
}

// ----- Market state accessors -----

std::optional<PriceTicks> LimitOrderBook::bestBid() const {
	if (bids.empty()) {
		return std::nullopt;
	}
	return bids.begin()->first;
}

std::optional<PriceTicks> LimitOrderBook::bestAsk() const {
	if (asks.empty()) {
		return std::nullopt;
	}
	return asks.begin()->first;
}

PriceTicks LimitOrderBook::midPrice() const {
	if (bids.empty() && asks.empty()) {
		return midPriceRecords.empty() ? toPriceTicks(20.0) : midPriceRecords.back();
	}
	if (bids.empty()) {
		return asks.begin()->first;
	}
	if (asks.empty()) {
		return bids.begin()->first;
	}
	return (bids.begin()->first + asks.begin()->first) / 2;
}

const std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>>& LimitOrderBook::getBids() const {
	return bids;
}

const std::map<PriceTicks, PriceLevel>& LimitOrderBook::getAsks() const {
	return asks;
}

const Trader* LimitOrderBook::getTrader(TraderId id) const {
	const auto it = traders.find(id);
	if (it != traders.end()) {
		return it->second;
	}
	return nullptr;
}

const std::unordered_map<TraderId, Trader*>& LimitOrderBook::getTraders() const {
	return traders;
}

// Returns highest volume among top N price levels on given side
Quantity LimitOrderBook::getHighestVolume(Side side, size_t priceLevels) const {
	Quantity levelVol = 0;
	Quantity maxVol = 0;

	int count = 0;

	if (side == Side::BUY) {
		if (bids.empty()) {
			return 0;
		}

		for (auto it = bids.begin(); it != bids.end() && count < priceLevels; ++it, ++count) {

			levelVol = it->second.levelVolume;

			if (levelVol == 0) {
				count--;
				continue;
			}

			if (levelVol > maxVol) {
				maxVol = levelVol;
			}
		}
	}
	else {
		if (asks.empty()) {
			return 0;
		}

		for (auto it = asks.begin(); it != asks.end() && count < priceLevels; ++it, ++count) {

			levelVol = it->second.levelVolume;

			if (levelVol == 0)
			{
				count--;
				continue;
			}

			if (levelVol > maxVol) {
				maxVol = levelVol;
			}
		}
	}

	return maxVol;
}

const Order* LimitOrderBook::getOrder(OrderId id) const {
	const auto it = orderLookup.find(id);

	if (it == orderLookup.end()) {
		return nullptr;
	}

	if (orderPool[it->second].id != id) {
		return nullptr;
	}

	return &orderPool[it->second];
}

// ----- Simulation updates -----

void LimitOrderBook::update() {
	midPriceRecords.push_back(midPrice());
}

const std::vector<PriceTicks>& LimitOrderBook::getMidPriceHistory() const {
	return midPriceRecords;
}

// ----- Order entry and matching -----

// Registers new order and performs basic risk checks (funds/stocks).
// Order is queued for matching during processOrders().
OrderResult LimitOrderBook::registerOrder(TraderId traderId, PriceTicks price, Quantity volume, Side side, Clock& clock) {
	Order order = { 0, traderId, price, volume, side, clock.now() };

	auto it = traders.find(traderId);
	if (it == traders.end() || !it->second) {
		return { 0, RejectReason::NoTrader };
	}
	Trader* trader = it->second;

	if (order.side == Side::BUY) {
		PriceTicks cost;
		if (mul_overflow_i64(price, volume, cost)) {
			return { 0, RejectReason::Overflow };
		}
		if (trader->getFunds() < cost) {
			return { 0, RejectReason::InsufficientFunds };
		}

		trader->lockFunds(cost);
	}
	else {
		if (trader->getStocks() < volume) {
			return { 0, RejectReason::InsufficientStocks };
		}

		trader->lockStocks(volume);
	}

	order.id = nextOrderId++;
	size_t index = 0;

	if (freeSlots.empty()) {
		orderPool.push_back(order);
		index = orderPool.size() - 1;
	}
	else {
		index = freeSlots.top();
		freeSlots.pop();
		orderPool.at(index) = order;
	}

	pendingOrderIndices.push_back(index);

	return { order.id, RejectReason::None };
}

// Processes queued orders in randomized order to avoid ordering bias
void LimitOrderBook::processOrders(Clock& clock) {
	while (!pendingOrderIndices.empty()) {
		std::uniform_int_distribution<size_t> dist(0, pendingOrderIndices.size() - 1);
		size_t pos = dist(rng);

		size_t orderIndex = pendingOrderIndices[pos];

		pendingOrderIndices[pos] = pendingOrderIndices.back();
		pendingOrderIndices.pop_back();

		executeMatch(orderIndex, clock);
	}
}

// Matches incoming order against opposing book.
// Traverses price levels best-to-worst.
// Stale entries are skipped using order ID validation.
void LimitOrderBook::executeMatch(size_t index, Clock& clock) {
	Order& incomingOrder = orderPool.at(index);

	if (incomingOrder.side == Side::BUY) {
		while (incomingOrder.volume > 0 && !asks.empty()) {
			auto priceLevelIt = asks.begin();
			if (priceLevelIt->first > incomingOrder.price) {
				break;
			}

			auto& level = priceLevelIt->second;

			while (incomingOrder.volume > 0 && level.nextToMatch < level.orderEntries.size()) {
				Order& restingOrder = orderPool.at(level.orderEntries[level.nextToMatch].index);

				if (restingOrder.volume <= 0 || restingOrder.id != level.orderEntries[level.nextToMatch].id) {
					level.nextToMatch++;
					continue;
				}

				Quantity tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(incomingOrder, restingOrder, priceLevelIt->first, tradeVolume, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				level.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0) {
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					auto it = traders.find(restingOrder.traderId);
					if (it != traders.end() && it->second) {
						it->second->onOrderFinished(restingOrder.id);
					}

					level.nextToMatch++;
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
	else {
		while (incomingOrder.volume > 0 && !bids.empty()) {
			auto priceLevelIt = bids.begin();
			if (priceLevelIt->first < incomingOrder.price) {
				break;
			}

			auto& level = priceLevelIt->second;

			while (incomingOrder.volume > 0 && level.nextToMatch < level.orderEntries.size()) {
				Order& restingOrder = orderPool.at(level.orderEntries[level.nextToMatch].index);

				if (restingOrder.volume <= 0 || restingOrder.id != level.orderEntries[level.nextToMatch].id) {
					level.nextToMatch++;
					continue;
				}

				Quantity tradeVolume = std::min(incomingOrder.volume, restingOrder.volume);

				recordTrade(restingOrder, incomingOrder, priceLevelIt->first, tradeVolume, clock);
				lastTradePrice = priceLevelIt->first;

				restingOrder.volume -= tradeVolume;
				incomingOrder.volume -= tradeVolume;

				level.levelVolume -= tradeVolume;

				if (restingOrder.volume == 0) {
					orderLookup.erase(restingOrder.id);
					freeSlots.push(level.orderEntries[level.nextToMatch].index);

					auto it = traders.find(restingOrder.traderId);
					if (it != traders.end() && it->second) {
						it->second->onOrderFinished(restingOrder.id);
					}

					level.nextToMatch++;
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

// Cancels active order and releases locked trader resources
bool LimitOrderBook::cancelOrder(OrderId orderId) {
	auto orderIndexIt = orderLookup.find(orderId);
	if (orderIndexIt == orderLookup.end()) {
		return false;
	}
	Order& orderToCancel = orderPool[orderIndexIt->second];

	if (orderId != orderToCancel.id || orderToCancel.volume == 0) {
		return false;
	}

	auto traderIt = traders.find(orderToCancel.traderId);
	if (traderIt == traders.end() || !traderIt->second) {
		return false;
	}
	Trader* trader = traderIt->second;

	if (orderToCancel.side == Side::BUY) {
		PriceTicks refund = 0;
		if (mul_overflow_i64(orderToCancel.price, orderToCancel.volume, refund)) {
			return false;
		}
		trader->unlockFunds(refund);
	}
	else {
		trader->unlockStocks(orderToCancel.volume);
	}

	if (orderToCancel.side == Side::BUY) {
		auto it = bids.find(orderToCancel.price);
		if (it != bids.end()) {
			it->second.levelVolume -= orderToCancel.volume;

			if (it->second.levelVolume <= 0) {
				bids.erase(it);
			}
		}
	}
	else {
		auto it = asks.find(orderToCancel.price);
		if (it != asks.end()) {
			it->second.levelVolume -= orderToCancel.volume;

			if (it->second.levelVolume <= 0) {
				asks.erase(it);
			}
		}
	}

	orderToCancel.volume = 0;
	trader->onOrderFinished(orderId);
	freeSlots.push(orderIndexIt->second);
	orderLookup.erase(orderId);

	return true;
}

// Adds unmatched order to book as resting liquidity
void LimitOrderBook::addLimitOrder(size_t index) {
	Order& incomingOrder = orderPool.at(index);

	if (incomingOrder.side == Side::BUY) {
		auto& level = bids[incomingOrder.price];
		
		if (level.priceLabel.empty()) {
			level.priceLabel = UIHelper::formatPrice(incomingOrder.price);
		}

		level.orderEntries.push_back({ incomingOrder.id, index });
		level.levelVolume += incomingOrder.volume;
	}
	else {
		auto& level = asks[incomingOrder.price];

		if (level.priceLabel.empty()) {
			level.priceLabel = UIHelper::formatPrice(incomingOrder.price);
		}

		level.orderEntries.push_back({ incomingOrder.id, index });
		level.levelVolume += incomingOrder.volume;
	}

	orderLookup[incomingOrder.id] = index;
}

// ----- Trader registry and trade recording -----

void LimitOrderBook::registerTrader(Trader* trader) {
	if (trader) {
		traders[trader->getId()] = trader;
	}
}

// Records trade and updates buyer/seller balances and positions
void LimitOrderBook::recordTrade(const Order& bidOrder, const Order& askOrder, PriceTicks price, Quantity volume, Clock& clock) {
	TradeRecord tradeRecord = {};
	tradeRecord.buyerOrderId = bidOrder.id;
	tradeRecord.sellerOrderId = askOrder.id;
	tradeRecord.timeStamp = clock.now();
	tradeRecord.tradeId = nextTradeId++;
	tradeRecord.price = price;
	tradeRecord.volume = volume;

	auto buyerIt = traders.find(bidOrder.traderId);
	if (buyerIt == traders.end() || !buyerIt->second) {
		std::cerr << "LimitOrderBook::recordTrade error: missing buyer trader for traderId="
			<< bidOrder.traderId << '\n';
		return;
	}
	Trader* buyer = buyerIt->second;

	auto sellerIt = traders.find(askOrder.traderId);
	if (sellerIt == traders.end() || !sellerIt->second) {
		std::cerr << "LimitOrderBook::recordTrade error: missing seller trader for traderId="
			<< askOrder.traderId << '\n';
		return;
	}
	Trader* seller = sellerIt->second;

	PriceTicks refund;
	if (mul_overflow_i64(bidOrder.price - price, volume, refund)) {
		std::cerr << "LimitOrderBook::recordTrade error: overflow computing refund\n";
		return;
	}

	PriceTicks cashExchanged;
	if (mul_overflow_i64(price, volume, cashExchanged)) {
		std::cerr << "LimitOrderBook::recordTrade error: overflow computing cash exchanged\n";
		return;
	}

	tradeCount++;

	tradePriceSum += price;

	if (buyer) {
		PriceTicks lockedUsed = 0;
		if (mul_overflow_i64(bidOrder.price, volume, lockedUsed)) {
			std::cerr << "LimitOrderBook::recordTrade error: overflow computing locked funds used\n";
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

	pendingTrades.push_back(tradeRecord);
}

// Returns and clears trades generated since last flush
std::vector<TradeRecord> LimitOrderBook::flushTrades() {
	std::vector<TradeRecord> trades = std::move(pendingTrades);
	pendingTrades.clear();
	return trades;
}