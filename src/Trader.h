#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <deque>

#include "datatypes.h"
#include "TradeStrategy.h"
#include "priceUtils.h"

enum TraderType
{
    Maker,
    Contrarian,
    Noise,
    Fundamental,
    Momentum
};

struct TraderStats {
    Quantity oldPosition = 0;
    PriceTicks avgEntry = 0;

    PriceTicks startEquity = 0;

    PriceTicks totalSellValue = 0;
    PriceTicks winSellValue = 0;
};

class Trader {
private:
    TraderType type;
    TradeStrategy* strategy;

    TraderId id;
    PriceTicks funds;
    PriceTicks lockedFunds;
    Quantity stocks;
    Quantity lockedStocks;

    std::map<PriceTicks, std::vector<OrderId>> ordersByPrice;
    std::unordered_map<OrderId, PriceTicks> idToPrice;
    std::deque<OrderId> activeOrderQueue;

    TraderStats stats;

public:
    Trader(TradeStrategy* strategy, TraderType type, TraderId id, PriceTicks funds, Quantity stocks);

    TraderId getId() const;
    PriceTicks getFunds() const;
    PriceTicks getLockedFunds() const;
    Quantity getStocks() const;
    Quantity getLockedStocks() const;
    const std::map<PriceTicks, std::vector<OrderId>>& getActiveOrderIds() const;
    size_t getOrderCount() const;
    TraderStats getStats() const;
    TraderType getType() const;

    void changeFunds(PriceTicks funds);
    void changeStocks(Quantity stocks);

    void lockFunds(PriceTicks funds);
    void lockStocks(Quantity stocks);
    void unlockFunds(PriceTicks funds);
    void unlockStocks(Quantity stocks);

    void changeLockedFunds(PriceTicks funds);
    void changeLockedStocks(Quantity stocks);

    void update(LimitOrderBook& LOB, Clock& clock);

    void addActiveOrderId(OrderId id, PriceTicks price);
    void removeActiveOrderId(OrderId id);
    void onOrderFinished(OrderId id);
    void clearOrdersPerc(LimitOrderBook& LOB, float perc);

    void onTradeFilled(Side side, PriceTicks fillPrice, Quantity fillQty);
};