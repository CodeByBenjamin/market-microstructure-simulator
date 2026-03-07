#include <algorithm>
#include <cmath>

#include "datatypes.h"
#include "MarketMaker.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"
#include "rng.h"

void MarketMaker::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    const size_t depth = 30;

    if (clock.now() != 0 && clock.now() % 4 == 0 && trader.getOrderCount() > 5) {
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

    const PriceTicks marketPriceTicks = LOB.midPrice();

    const PriceTicks midTicks = (PriceTicks)std::llround(
        (double)marketPriceTicks * 0.7 + (double)avrTicks * 0.3
    );

    std::uniform_int_distribution<PriceTicks> offsetPct(2, 5);
    std::uniform_int_distribution<PriceTicks> jitterPct(1, 2);

    PriceTicks refTicks = midTicks + jitterPct(rng);

    PriceTicks offTicks = midTicks + offsetPct(rng);

    {
        PriceTicks buyPriceTicks = refTicks - offTicks;
        OrderResult orderResult = LOB.registerOrder(trader.getId(), buyPriceTicks, volDist(rng), Side::BUY, clock);
        if (orderResult.orderId != 0) {
            trader.addActiveOrderId(orderResult.orderId, buyPriceTicks);
        }
    }

    {
        PriceTicks sellPriceTicks = refTicks + offTicks;
        OrderResult orderResult = LOB.registerOrder(trader.getId(), sellPriceTicks, volDist(rng), Side::SELL, clock);
        if (orderResult.orderId != 0) {
            trader.addActiveOrderId(orderResult.orderId, sellPriceTicks);
        }
    }

    // 10% chance aggressive trade
    std::uniform_int_distribution<int> chance(1, 100);
    if (chance(rng) > 90) {
        const bool doBuy = (chance(rng) > 50);

        if (doBuy) {
            auto bestAskTicks = LOB.bestAsk();
            if (bestAskTicks) {
                auto res = LOB.registerOrder(trader.getId(), *bestAskTicks, 5, Side::BUY, clock);
                if (res.orderId != 0) {
                    trader.addActiveOrderId(res.orderId, *bestAskTicks);
                }
            }
        }
        else {
            auto bestBidTicks = LOB.bestBid();
            if (bestBidTicks) {
                auto res = LOB.registerOrder(trader.getId(), *bestBidTicks, 5, Side::SELL, clock);
                if (res.orderId != 0) {
                    trader.addActiveOrderId(res.orderId, *bestBidTicks);
                }
            }
        }
    }
}