#include <map>
#include <vector>

#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "datatypes.h"
#include "TradeStrategy.h"
#include "priceutils.h"

Trader::Trader(TradeStrategy* strategy, TraderType type, TraderId id, PriceTicks funds, Quantity stocks, Quantity maxInv)
    : strategy(strategy),
    type(type),
    id(id),
    funds(funds),
    stocks(stocks),
    maxInventory(maxInv)
{
    lockedFunds = 0;
    lockedStocks = 0;

    stats.startEquity = funds + stocks * toPriceTicks(20.0); //Starting value is 20
}

TraderId Trader::getId() const
{
	return id;
}

PriceTicks Trader::getFunds() const
{
	return funds;
}

Quantity Trader::getStocks() const
{
	return stocks;
}

const std::map<PriceTicks, std::vector<OrderId>>& Trader::getActiveOrderIds() const
{
	return ordersByPrice;
}

size_t Trader::getOrderCount() const 
{ 
    return idToPrice.size(); 
}

TraderStats Trader::getStats() const
{
    return stats;
}

TraderType Trader::getType() const
{
    return type;
}

void Trader::changeFunds(PriceTicks funds)
{
    this->funds += funds;
}

void Trader::changeStocks(Quantity stocks)
{
    this->stocks += stocks;
}

void Trader::lockFunds(PriceTicks funds)
{
    this->funds -= funds;
    this->lockedFunds += funds;
}

void Trader::lockStocks(Quantity stocks)
{
    this->stocks -= stocks;
    this->lockedStocks += stocks;
}

void Trader::unlockFunds(PriceTicks funds)
{
    this->funds += funds;
    this->lockedFunds -= funds;
}

void Trader::unlockStocks(Quantity stocks)
{
    this->stocks += stocks;
    this->lockedStocks -= stocks;
}

void Trader::changeLockedFunds(PriceTicks funds)
{
    this->lockedFunds += funds;
}

void Trader::changeLockedStocks(Quantity stocks)
{
    this->lockedStocks += stocks;
}

void Trader::update(LimitOrderBook& LOB, Clock& clock)
{
	strategy->decide(*this, LOB, clock);
}

void Trader::addActiveOrderId(OrderId id, PriceTicks price) {
    idToPrice[id] = price;

    ordersByPrice[price].push_back(id);
}

void Trader::removeActiveOrderId(OrderId id) 
{
    auto itID = idToPrice.find(id);
    if (itID == idToPrice.end()) return;

    PriceTicks price = itID->second;

    auto itPrice = ordersByPrice.find(price);
    if (itPrice != ordersByPrice.end()) {
        std::erase(itPrice->second, id);

        if (itPrice->second.empty()) {
            ordersByPrice.erase(itPrice);
        }
    }

    idToPrice.erase(itID);
}

void Trader::onOrderFinished(OrderId id) 
{
	removeActiveOrderId(id);
}

void Trader::clearHalfOrders(LimitOrderBook& LOB) {
    if (idToPrice.empty()) return;

    int numToCancel = idToPrice.size() / 2;

    for (int i = 0; i < numToCancel; i++) {
        auto it = idToPrice.begin();
        if (it == idToPrice.end()) break;

        OrderId id = it->first;

        LOB.cancelOrder(id);
    }
}

void Trader::onTradeFilled(LimitOrderBook& LOB, Side side, PriceTicks fillPrice, Quantity fillQty)
{
    if (side == Side::BUY)
    {
        PriceTicks oldValue = 0;
        PriceTicks newValue = 0;
        if (mul_overflow_i64(stats.avgEntry, stats.oldStocks, oldValue)) oldValue = 0;
        if (mul_overflow_i64(fillPrice, fillQty, newValue)) newValue = 0;
        stats.avgEntry = (oldValue + newValue) / static_cast<PriceTicks>(stats.oldStocks + fillQty);
    }
    else
    {
        if (fillPrice > stats.avgEntry)
        {
            stats.wins++;
        }

        stats.sellFills++;
    }

    PriceTicks totalFunds = funds + lockedFunds;
    stats.position = stocks + lockedStocks;

    PriceTicks stockValue = 0;
    PriceTicks mark = LOB.midPrice();
    if (mul_overflow_i64(mark, stats.position, stockValue)) stockValue = 0;
    stats.pnl = (totalFunds + stockValue) - stats.startEquity;

    stats.oldStocks = stocks + lockedStocks;
}