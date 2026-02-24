#include <cmath>
#include <random>
#include <algorithm>
#include <cstdlib>

#include "datatypes.h"
#include "TrendStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"

void TrendStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    static std::mt19937 rng(std::random_device{}());
    const size_t depth = 100;

    if (clock.now() != 0 && clock.now() % 10 == 0 && trader.getOrderCount() > 10) {
        trader.clearHalfOrders(LOB);
    }

    auto const& midPriceHistory = LOB.getMidPriceHistory();
    if (midPriceHistory.empty())
        return;

    double sumTicks = 0.0;
    size_t count = 0;

    for (auto it = midPriceHistory.rbegin();
        it != midPriceHistory.rend() && count < depth;
        ++it, ++count)
    {
        sumTicks += (double)(*it);
    }

    const size_t actualDepth = std::min(depth, midPriceHistory.size());
    const double avrTicks = sumTicks / (double)actualDepth;
    if (avrTicks <= 0.0) return;

    const double currentTicks = (double)midPriceHistory.back();
    const double diffTicks = currentTicks - avrTicks;

    const double thresholdTicks = avrTicks * 0.001;

    const bool buyingTheDip = (trader.getFunds() >= toPriceTicks(10000.0));
    const bool cashOut = (trader.getStocks() >= 500);

    auto makeVol = [&](long capacity, double perc) -> long {
        if (capacity <= 0) return 0;

        Quantity minVol = (Quantity)std::floor((double)capacity * perc);
        Quantity maxVol = (Quantity)std::floor((double)capacity * 0.3);

        minVol = std::max(1L, minVol);
        maxVol = std::max(1L, maxVol);

        if (minVol >= maxVol) minVol = std::max(1L, maxVol / 2);

        std::uniform_int_distribution<Quantity> dist(minVol, maxVol);
        return std::clamp(dist(rng), 1L, capacity);
        };

    if (diffTicks > thresholdTicks && !cashOut)
    {
        auto bestAskTicks = LOB.bestAsk();

        if (!bestAskTicks)
            return;

        const PriceTicks executionTicks =
            (PriceTicks)std::llround((double)*bestAskTicks * 1.05);

        const double funds = (double)trader.getFunds();

        const Quantity canBuy = (Quantity)std::floor(funds / (double)executionTicks);
        if (canBuy <= 0) return;

        const double perc = diffTicks / avrTicks;
        const Quantity willBuy = makeVol(canBuy, perc);
        if (willBuy <= 0) return;

        auto res = LOB.processOrder(trader.getId(), executionTicks, willBuy, Side::BUY, clock);
        if (res.reason == RejectReason::None) {
            trader.addActiveOrderId(res.orderId, executionTicks);
        }
    }
    else if (diffTicks < -thresholdTicks && !buyingTheDip)
    {
        auto bestBidTicks = LOB.bestBid();

        if (!bestBidTicks)
            return;

        const PriceTicks executionTicks =
            (PriceTicks)std::llround((double)*bestBidTicks * 0.95);

        const Quantity canSell = trader.getStocks();
        if (canSell <= 0) return;

        const double perc = std::abs(diffTicks) / avrTicks;
        const Quantity willSell = makeVol(canSell, perc);
        if (willSell <= 0) return;

        auto res = LOB.processOrder(trader.getId(), executionTicks, willSell, Side::BUY, clock);
        if (res.reason == RejectReason::None) {
            trader.addActiveOrderId(res.orderId, executionTicks);
        }
    }
}