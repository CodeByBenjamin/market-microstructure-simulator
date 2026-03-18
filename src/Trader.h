#pragma once

#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

#include "datatypes.h"

class TradeStrategy;
class LimitOrderBook;
class Clock;

enum TraderType
{
    Maker,
    Momentum,
    Contrarian,
    Fundamental,
    Noise
};

struct TraderStats {
    Quantity oldPosition = 0;
    PriceTicks avgEntry = 0;
    PriceTicks startEquity = 0;
    PriceTicks totalSellValue = 0;
    PriceTicks winSellValue = 0;
};

// Simulation trader/account abstraction.
// Owns portfolio state and active-order bookkeeping.
// Strategy is referenced as a non-owning pointer.
class Trader {
private:
    TraderType type;

    // Non-owning strategy. Lifetime is managed by the simulation.
    TradeStrategy* strategy;

    TraderId id;
    PriceTicks funds;
    PriceTicks lockedFunds;
    Quantity stocks;
    Quantity lockedStocks;

    // Active orders grouped by price level for trader-side bookkeeping.
    std::map<PriceTicks, std::vector<OrderId>> ordersByPrice;

    // Active order ID -> resting price.
    std::unordered_map<OrderId, PriceTicks> idToPrice;

    // Submission-order queue of active order IDs.
    std::deque<OrderId> activeOrderQueue;

    TraderStats stats;

public:
    // ----- Construction -----
    Trader(TradeStrategy* strategy, TraderType type, TraderId id, PriceTicks funds, Quantity stocks);

    // ----- Trader state accessors -----
    [[nodiscard]] TraderId getId() const;
    [[nodiscard]] PriceTicks getFunds() const;
    [[nodiscard]] PriceTicks getLockedFunds() const;
    [[nodiscard]] Quantity getStocks() const;
    [[nodiscard]] Quantity getLockedStocks() const;
    [[nodiscard]] const std::map<PriceTicks, std::vector<OrderId>>& getActiveOrdersByPrice() const;
    [[nodiscard]] size_t getOrderCount() const;
    [[nodiscard]] const TraderStats& getStats() const;
    [[nodiscard]] TraderType getType() const;

    // ----- Portfolio state mutation -----
    void changeFunds(PriceTicks deltaFunds);
    void changeStocks(Quantity deltaStocks);

    void lockFunds(PriceTicks amount);
    void lockStocks(Quantity amount);
    void unlockFunds(PriceTicks amount);
    void unlockStocks(Quantity amount);

    void changeLockedFunds(PriceTicks deltaFunds);
    void changeLockedStocks(Quantity deltaStocks);

    // ----- Simulation lifecycle -----
    void update(LimitOrderBook& lob, Clock& clock);

    // ----- Active order bookkeeping -----
    void addActiveOrderId(OrderId id, PriceTicks price);
    void removeActiveOrderId(OrderId id);
    void onOrderFinished(OrderId id);
    void clearOrdersPerc(LimitOrderBook& lob, float perc);

    // ----- Fill/accounting callbacks -----
    void onTradeFilled(Side side, PriceTicks fillPrice, Quantity fillQty);
};