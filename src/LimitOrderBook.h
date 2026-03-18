#pragma once

#include <map>
#include <optional>
#include <stack>
#include <unordered_map>
#include <vector>

#include "datatypes.h"

class Trader;
class Clock;

extern PriceTicks tradePriceSum;
extern Quantity tradeCount;

namespace sf {
    class RenderWindow;
    class Font;
}

// Central matching engine for the simulator.
// Owns order storage and book state.
// Traders are registered as non-owning pointers; lifetime is managed externally.
class LimitOrderBook {
private:
    std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>> bids;
    std::map<PriceTicks, PriceLevel> asks;

    // Non-owning trader registry. Lifetime is managed by the simulation.
    std::unordered_map<TraderId, Trader*> traders;

    // Active order ID -> index in orderPool.
    std::unordered_map<OrderId, size_t> orderLookup;

    // Active and recycled order storage.
    std::vector<Order> orderPool;

    // Reusable vacant indices in orderPool.
    std::stack<size_t> freeSlots;

    // Orders submitted this step, processed in randomized order.
    std::vector<size_t> pendingOrderIndices;

    OrderId nextOrderId = 1;
    TradeId nextTradeId = 1;

    PriceTicks lastTradePrice = 0;

    // Trades generated since last flushTrades().
    std::vector<TradeRecord> pendingTrades;

    std::vector<PriceTicks> midPriceRecords;

    void executeMatch(size_t index, Clock& clock);
    void addLimitOrder(size_t index);
    void recordTrade(const Order& bidOrder, const Order& askOrder, PriceTicks price, Quantity volume, Clock& clock);

public:
    // ----- Construction -----
    LimitOrderBook();

    // ----- Market state accessors -----
    [[nodiscard]] std::optional<PriceTicks> bestBid() const;
    [[nodiscard]] std::optional<PriceTicks> bestAsk() const;
    [[nodiscard]] PriceTicks midPrice() const;

    [[nodiscard]] const std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>>& getBids() const;
    [[nodiscard]] const std::map<PriceTicks, PriceLevel>& getAsks() const;
    [[nodiscard]] const Trader* getTrader(TraderId id) const;
    [[nodiscard]] const std::unordered_map<TraderId, Trader*>& getTraders() const;
    [[nodiscard]] Quantity getHighestVolume(Side side, size_t priceLevels) const;
    [[nodiscard]] const Order* getOrder(OrderId id) const;
    [[nodiscard]] const std::vector<PriceTicks>& getMidPriceHistory() const;

    // ----- Simulation lifecycle -----
    void update();
    void processOrders(Clock& clock);

    // ----- Order entry / cancellation -----
    [[nodiscard]] OrderResult registerOrder(TraderId traderId, PriceTicks price, Quantity volume, Side side, Clock& clock);
    [[nodiscard]] bool cancelOrder(OrderId orderId);

    // ----- Trader registry -----
    void registerTrader(Trader* trader);

    // ----- Trade output -----
    [[nodiscard]] std::vector<TradeRecord> flushTrades();
};