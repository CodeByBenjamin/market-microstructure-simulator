#include <random>
#include <algorithm>
#include <cmath>

#include "datatypes.h"
#include "RandomStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"

void RandomStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) {
    static std::mt19937 rng(std::random_device{}());

    const PriceTicks perceivedValueTicks = toPriceTicks(20);

    const PriceTicks marketPriceTicks = LOB.midPrice();

    const PriceTicks midTicks = (PriceTicks)std::llround(
        (double)marketPriceTicks * 0.7 + (double)perceivedValueTicks * 0.3
    );

    if (clock.now() != 0 && clock.now() % 4 == 0 && trader.getOrderCount() > 5) {
        trader.clearHalfOrders(LOB);
    }

    std::uniform_real_distribution<double> offsetPct(0.001, 0.01);   // 0.1% to 1%
    std::uniform_real_distribution<double> jitterPct(-0.005, 0.005); // -0.5% to +0.5%
    std::uniform_int_distribution<long> volDist(5, 20);

    PriceTicks refTicks = (PriceTicks)std::llround((double)midTicks * (1.0 + jitterPct(rng)));

    PriceTicks offTicks = (PriceTicks)std::llround((double)midTicks * offsetPct(rng));
    offTicks = std::max<PriceTicks>(1, offTicks);

    {
        PriceTicks buyPriceTicks = refTicks - offTicks;
        OrderResult orderResult = LOB.processOrder(trader.getId(), buyPriceTicks, volDist(rng), Side::BUY, clock);
        if (orderResult.orderId != 0) {
            trader.addActiveOrderId(orderResult.orderId, buyPriceTicks);
        }
    }

    {
        PriceTicks sellPriceTicks = refTicks + offTicks;
        OrderResult orderResult = LOB.processOrder(trader.getId(), sellPriceTicks, volDist(rng), Side::SELL, clock);
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
                auto res = LOB.processOrder(trader.getId(), *bestAskTicks, 5, Side::BUY, clock);
                if (res.orderId != 0) {
                    trader.addActiveOrderId(res.orderId, *bestAskTicks);
                }
            }
        }
        else {
            auto bestBidTicks = LOB.bestBid();
            if (bestBidTicks) {
                auto res = LOB.processOrder(trader.getId(), *bestBidTicks, 5, Side::SELL, clock);
                if (res.orderId != 0) {
                    trader.addActiveOrderId(res.orderId, *bestBidTicks);
                }
            }
        }
    }
}