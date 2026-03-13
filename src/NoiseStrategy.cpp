#include <algorithm>
#include <cstdlib>
#include <random>

#include "datatypes.h"
#include "NoiseStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"
#include "rng.h"

namespace
{
    template <typename T>
    T clampMin(T value, T minValue)
    {
        return (value < minValue) ? minValue : value;
    }
}

void NoiseStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    std::bernoulli_distribution tradeChance(0.5);
    if (!tradeChance(rng))
        return;

    const Tick now = clock.now();

    if (now != 0 && (now + trader.getId()) % 10 == 0 && trader.getOrderCount() > 3)
    {
        trader.clearOrdersPerc(LOB, 0.3f);
    }

    std::uniform_int_distribution<int> sideDist(0, 1);
    const Side side = (sideDist(rng) == 0) ? Side::BUY : Side::SELL;

    std::uniform_int_distribution<Quantity> qtyDist(4, 10);
    Quantity qty = qtyDist(rng);

    if (side == Side::SELL)
    {
        qty = std::min(qty, trader.getStocks());
        if (qty <= 0)
            return;
    }

    std::uniform_int_distribution<int> modeDist(1, 100);
    const bool aggressive = (modeDist(rng) <= 50);

    if (aggressive)
    {
        if (side == Side::BUY)
        {
            const auto bestAsk = LOB.bestAsk();
            if (!bestAsk)
                return;

            const PriceTicks orderPrice = *bestAsk;

            Quantity maxAffordable = trader.getFunds() / orderPrice;
            qty = std::clamp(qty, static_cast<Quantity>(0), maxAffordable);

            if (qty == 0)
                return;

            auto res = LOB.registerOrder(trader.getId(), orderPrice, qty, Side::BUY, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
            {
                trader.addActiveOrderId(res.orderId, orderPrice);
            }
        }
        else
        {
            const auto bestBid = LOB.bestBid();
            if (!bestBid)
                return;

            const PriceTicks orderPrice = *bestBid;

            auto res = LOB.registerOrder(trader.getId(), orderPrice, qty, Side::SELL, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
            {
                trader.addActiveOrderId(res.orderId, orderPrice);
            }
        }
    }
    else
    {
        PriceTicks mid = LOB.midPrice();
        if (mid <= 0)
            return;

        std::uniform_int_distribution<int> offsetDist(1, 5);
        int offset = offsetDist(rng);

        if (side == Side::BUY)
        {
            const PriceTicks orderPrice =
                static_cast<PriceTicks>(clampMin<long long>(
                    static_cast<long long>(mid) - static_cast<long long>(offset), 1LL));

            auto res = LOB.registerOrder(trader.getId(), orderPrice, qty, Side::BUY, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
            {
                trader.addActiveOrderId(res.orderId, orderPrice);
            }
        }
        else
        {
            const PriceTicks orderPrice =
                static_cast<PriceTicks>(clampMin<long long>(
                    static_cast<long long>(mid) + static_cast<long long>(offset), 1LL));

            auto res = LOB.registerOrder(trader.getId(), orderPrice, qty, Side::SELL, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
            {
                trader.addActiveOrderId(res.orderId, orderPrice);
            }
        }
    }
}