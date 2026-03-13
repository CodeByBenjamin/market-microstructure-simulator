#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "datatypes.h"
#include "TradeStrategy.h"
#include "priceutils.h"

Trader::Trader(TradeStrategy* strategy, TraderType type, TraderId id, PriceTicks funds, Quantity stocks)
    : strategy(strategy),
    type(type),
    id(id),
    funds(funds),
    stocks(stocks)
{
    lockedFunds = 0;
    lockedStocks = 0;

    stats.startEquity = funds + stocks * toPriceTicks(20.0);

    stats.avgEntry = (stocks > 0) ? toPriceTicks(20.0) : 0;
    stats.oldPosition = stocks;
}

TraderId Trader::getId() const
{
    return id;
}

PriceTicks Trader::getFunds() const
{
    return funds;
}

PriceTicks Trader::getLockedFunds() const
{
    return lockedFunds;
}

Quantity Trader::getStocks() const
{
    return stocks;
}

Quantity Trader::getLockedStocks() const
{
    return lockedStocks;
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

void Trader::addActiveOrderId(OrderId id, PriceTicks price)
{
    idToPrice[id] = price;
    ordersByPrice[price].push_back(id);
    activeOrderQueue.push_back(id);   // newest goes to back
}

void Trader::removeActiveOrderId(OrderId id)
{
    auto itID = idToPrice.find(id);
    if (itID == idToPrice.end())
        return;

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

void Trader::clearOrdersPerc(LimitOrderBook& LOB, float perc)
{
    if (idToPrice.empty() || activeOrderQueue.empty())
        return;

    int numToCancel = static_cast<int>(std::round(idToPrice.size() * perc));
    numToCancel = std::clamp(numToCancel, 0, static_cast<int>(idToPrice.size()));

    int cancelled = 0;

    while (cancelled < numToCancel && !activeOrderQueue.empty())
    {
        OrderId id = activeOrderQueue.front();
        activeOrderQueue.pop_front();

        auto it = idToPrice.find(id);
        if (it == idToPrice.end())
            continue;

        LOB.cancelOrder(id);
        removeActiveOrderId(id);
        ++cancelled;
    }
}

void Trader::onTradeFilled(Side side, PriceTicks fillPrice, Quantity fillQty)
{
    if (side == Side::BUY)
    {
        Quantity prevPos = stats.oldPosition;

        PriceTicks oldValue = 0;
        if (mul_overflow_i64(stats.avgEntry, prevPos, oldValue)) oldValue = 0;
        PriceTicks newValue = 0;
        if (mul_overflow_i64(fillPrice, fillQty, newValue)) newValue = 0;

        Quantity newPos = prevPos + fillQty;

        if (newPos > 0)
        {
            stats.avgEntry = (oldValue + newValue) / newPos;
        }

        stats.oldPosition = newPos;
    }
    else
    {
        PriceTicks value = 0;
        if (mul_overflow_i64(fillPrice, fillQty, value)) {
            std::cout << "FATAL: overflow in recordTrade\n";
            return;
        }

        if (fillPrice >= stats.avgEntry)
        {
            stats.winSellValue += value;
        }

        stats.totalSellValue += value;
        if (fillQty >= stats.oldPosition)
            stats.oldPosition = 0;
        else
            stats.oldPosition = stats.oldPosition - fillQty;
    }

    if (stats.oldPosition == 0)
        stats.avgEntry = 0;
}