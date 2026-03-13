#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "datatypes.h"
#include "MomentumStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"
#include "rng.h"

void MomentumStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    std::bernoulli_distribution tradeChance(0.25);
    if (!tradeChance(rng))
        return;

    const Tick now = clock.now();

    if (now != 0 && (now + trader.getId()) % 10 == 0 && trader.getOrderCount() > 3)
    {
        trader.clearOrdersPerc(LOB, 0.4f);
    }

    const size_t depth = 8;
    const Quantity targetInv = 30;
    const Quantity maxInv = 60;

    auto const& history = LOB.getMidPriceHistory();
    if (history.empty())
        return;

    PriceTicks avgAbsMove = 1;
    if (history.size() >= 2)
    {
        PriceTicks moveSum = 0;
        size_t moveCount = 0;

        for (size_t i = history.size() - 1; i > 0 && moveCount < depth; --i, ++moveCount)
            moveSum += std::abs(history[i] - history[i - 1]);

        if (moveCount > 0)
            avgAbsMove = std::max<PriceTicks>(1, moveSum / static_cast<PriceTicks>(moveCount));
    }

    PriceTicks threshold = std::max<PriceTicks>(1, avgAbsMove * 2);

    PriceTicks sum = 0;
    size_t count = 0;

    for (auto it = history.rbegin(); it != history.rend() && count < depth; ++it, ++count)
        sum += *it;

    if (count == 0)
        return;

    PriceTicks avg = sum / static_cast<PriceTicks>(count);
    if (avg <= 0)
        return;

    long long invDiff =
        static_cast<long long>(trader.getStocks()) - static_cast<long long>(targetInv);

    double buyScale = 1.0;
    double sellScale = 1.0;

    if (invDiff > 0)
        buyScale = std::max(0.25, 1.0 - static_cast<double>(invDiff) / maxInv);

    if (invDiff < 0)
        sellScale = std::max(0.25, 1.0 - static_cast<double>(-invDiff) / maxInv);

    PriceTicks current = history.back();
    PriceTicks diff = current - avg;
    PriceTicks absDiff = std::abs(diff);

    if (history.size() < 3)
        return;

    PriceTicks recentSlope = history.back() - history[history.size() - 3];

    auto makeVol = [&](Quantity capacity, PriceTicks move, PriceTicks base) -> Quantity
        {
            if (capacity <= 0 || base <= 0)
                return 0;

            Quantity percBps =
                static_cast<Quantity>((static_cast<long long>(move) * 10000LL) / base);

            Quantity minVol =
                static_cast<Quantity>((static_cast<long long>(capacity) * percBps) / 10000LL);

            Quantity maxVol =
                static_cast<Quantity>((static_cast<long long>(capacity) * 15LL) / 100LL);

            minVol = std::max<Quantity>(1, minVol);
            maxVol = std::max<Quantity>(1, maxVol);

            if (minVol > maxVol)
                minVol = maxVol;

            std::uniform_int_distribution<Quantity> dist(minVol, maxVol);
            return std::clamp(dist(rng), static_cast<Quantity>(1), capacity);
        };

    if (diff > threshold && recentSlope > 0)
    {
        auto bestAsk = LOB.bestAsk();
        if (!bestAsk)
            return;

        PriceTicks price = *bestAsk;

        PriceTicks funds = trader.getFunds();
        Quantity room = 0;
        if (trader.getStocks() < maxInv)
            room = maxInv - trader.getStocks();
        Quantity capacity = static_cast<Quantity>(funds / price);
        capacity = std::min(capacity, room);
        capacity = static_cast<Quantity>(capacity * buyScale);

        if (capacity <= 0)
            return;

        Quantity willBuy = makeVol(capacity, absDiff, avg);
        if (willBuy <= 0)
            return;

        auto res = LOB.registerOrder(trader.getId(), price, willBuy, Side::BUY, clock);
        if (res.reason == RejectReason::None && res.orderId != 0)
            trader.addActiveOrderId(res.orderId, price);
    }
    else if (diff < -threshold && recentSlope < 0)
    {
        auto bestBid = LOB.bestBid();
        if (!bestBid)
            return;

        PriceTicks price = *bestBid;

        Quantity capacity = static_cast<Quantity>(trader.getStocks() * sellScale);

        if (capacity <= 0)
            return;

        Quantity willSell = makeVol(capacity, absDiff, avg);
        if (willSell <= 0)
            return;

        auto res = LOB.registerOrder(trader.getId(), price, willSell, Side::SELL, clock);
        if (res.reason == RejectReason::None && res.orderId != 0)
            trader.addActiveOrderId(res.orderId, price);
    }
}