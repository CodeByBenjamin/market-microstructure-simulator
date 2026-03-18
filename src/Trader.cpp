#include <algorithm>
#include <cmath>
#include <iostream>

#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "TradeStrategy.h"
#include "priceutils.h"

// ----- Construction -----

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

// ----- Trader state accessors -----

TraderId Trader::getId() const {
    return id;
}

PriceTicks Trader::getFunds() const {
    return funds;
}

PriceTicks Trader::getLockedFunds() const {
    return lockedFunds;
}

Quantity Trader::getStocks() const {
    return stocks;
}

Quantity Trader::getLockedStocks() const {
    return lockedStocks;
}

const std::map<PriceTicks, std::vector<OrderId>>& Trader::getActiveOrdersByPrice() const {
    return ordersByPrice;
}

size_t Trader::getOrderCount() const {
    return idToPrice.size();
}

const TraderStats& Trader::getStats() const {
    return stats;
}

TraderType Trader::getType() const {
    return type;
}

// ----- Portfolio state mutation -----

void Trader::changeFunds(PriceTicks deltaFunds) {
    funds += deltaFunds;
}

void Trader::changeStocks(Quantity deltaStocks) {
    stocks += deltaStocks;
}

void Trader::lockFunds(PriceTicks amount) {
    funds -= amount;
    lockedFunds += amount;
}

void Trader::lockStocks(Quantity amount) {
    stocks -= amount;
    lockedStocks += amount;
}

void Trader::unlockFunds(PriceTicks amount) {
    funds += amount;
    lockedFunds -= amount;
}

void Trader::unlockStocks(Quantity amount) {
    stocks += amount;
    lockedStocks -= amount;
}

void Trader::changeLockedFunds(PriceTicks deltaFunds) {
    lockedFunds += deltaFunds;
}

void Trader::changeLockedStocks(Quantity deltaStocks) {
    lockedStocks += deltaStocks;
}

// ----- Simulation lifecycle -----

void Trader::update(LimitOrderBook& lob, Clock& clock) {
    if (strategy == nullptr) {
        return;
    }

    strategy->decide(*this, lob, clock);
}

// ----- Active order bookkeeping -----

void Trader::addActiveOrderId(OrderId id, PriceTicks price) {
    idToPrice[id] = price;
    ordersByPrice[price].push_back(id);
    activeOrderQueue.push_back(id);
}

void Trader::removeActiveOrderId(OrderId id) {
    const auto idIt = idToPrice.find(id);
    if (idIt == idToPrice.end()) {
        return;
    }

    PriceTicks price = idIt->second;

    auto priceIt = ordersByPrice.find(price);    
    if (priceIt != ordersByPrice.end()) {
        std::erase(priceIt->second, id);

        if (priceIt->second.empty()) {
            ordersByPrice.erase(priceIt);
        }
    }

    idToPrice.erase(idIt);
}

void Trader::onOrderFinished(OrderId id) {
    removeActiveOrderId(id);
}

void Trader::clearOrdersPerc(LimitOrderBook& lob, float perc) {
    if (idToPrice.empty() || activeOrderQueue.empty()) {
        return;
    }

    int numToCancel = static_cast<int>(std::round(idToPrice.size() * perc));
    numToCancel = std::clamp(numToCancel, 0, static_cast<int>(idToPrice.size()));

    int cancelled = 0;

    while (cancelled < numToCancel && !activeOrderQueue.empty()) {
        OrderId id = activeOrderQueue.front();
        activeOrderQueue.pop_front();

        auto it = idToPrice.find(id);
        if (it == idToPrice.end()) {
            continue;
        }

        if (!lob.cancelOrder(id)) {
            std::cerr << "Trader::clearOrdersPerc error: failed to cancel orderId=" << id << '\n';
            continue;
        }
        removeActiveOrderId(id);
        ++cancelled;
    }
}

// ----- Fill/accounting callbacks -----

void Trader::onTradeFilled(Side side, PriceTicks fillPrice, Quantity fillQty) {
    if (side == Side::BUY)
    {
        Quantity prevPos = stats.oldPosition;

        PriceTicks oldValue = 0;
        if (mul_overflow_i64(stats.avgEntry, prevPos, oldValue)) {
            oldValue = 0;
        }
        PriceTicks newValue = 0;
        if (mul_overflow_i64(fillPrice, fillQty, newValue)) {
            newValue = 0;
        }

        Quantity newPos = prevPos + fillQty;

        if (newPos > 0) {
            stats.avgEntry = (oldValue + newValue) / newPos;
        }

        stats.oldPosition = newPos;
    }
    else
    {
        PriceTicks value = 0;
        if (mul_overflow_i64(fillPrice, fillQty, value)) {
            std::cerr << "Trader::onTradeFilled error: overflow computing sell value\n";
            return;
        }

        if (fillPrice >= stats.avgEntry) {
            stats.winSellValue += value;
        }

        stats.totalSellValue += value;
        if (fillQty >= stats.oldPosition) {
            stats.oldPosition = 0;
        }
        else {
            stats.oldPosition -= fillQty;
        }
    }

    if (stats.oldPosition == 0) {
        stats.avgEntry = 0;
    }
}