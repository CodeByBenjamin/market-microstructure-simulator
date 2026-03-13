#include <algorithm>
#include <cstdlib>

#include "datatypes.h"
#include "FundamentalStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"
#include "rng.h"

void FundamentalStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    std::bernoulli_distribution tradeChance(0.25);
    if (!tradeChance(rng))
        return;

    const Tick now = clock.now();

    if (now < 15)
        return;

    if (now != 0 && (now + trader.getId()) % 10 == 0 && trader.getOrderCount() > 3)
        trader.clearOrdersPerc(LOB, 0.4f);

    const Quantity maxInv = 40;
    const Quantity baseSize = 3;

    const PriceTicks fairValue = 2000 + ((trader.getId() * 13121) % 140 - 70);

    std::uniform_int_distribution<PriceTicks> jitter(-4, 4);
    PriceTicks ref = std::max<PriceTicks>(1, fairValue + jitter(rng));

    auto bestBid = LOB.bestBid();
    auto bestAsk = LOB.bestAsk();

    PriceTicks bidPrice = std::max<PriceTicks>(1, ref - 2);
    PriceTicks askPrice = ref + 2;

    if (bestBid)
        bidPrice = std::min<PriceTicks>(ref - 1, *bestBid + 1);

    if (bestAsk)
        askPrice = std::max<PriceTicks>(ref + 1, *bestAsk - 1);

    askPrice = std::max<PriceTicks>(askPrice, bidPrice + 1);

    long long invDiff =
        static_cast<long long>(trader.getStocks()) - static_cast<long long>(maxInv / 2);

    Quantity buyCap = static_cast<Quantity>(trader.getFunds() / bidPrice);
    Quantity sellCap = trader.getStocks();

    Quantity bidSize = std::min(baseSize, buyCap);
    Quantity askSize = std::min(baseSize, sellCap);

    if (invDiff > 10)
        bidSize = std::min<Quantity>(1, bidSize);

    if (invDiff < -10)
        askSize = std::min<Quantity>(1, askSize);

    // If trader thinks asset is cheap, bias toward buying
    if (bestAsk && ref > *bestAsk + 2)
    {
        if (bidSize > 0)
        {
            auto res = LOB.registerOrder(trader.getId(), *bestAsk, bidSize, Side::BUY, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
                trader.addActiveOrderId(res.orderId, *bestAsk);
        }
        return;
    }

    // If trader thinks asset is rich, bias toward selling
    if (bestBid && ref + 2 < *bestBid)
    {
        if (askSize > 0)
        {
            auto res = LOB.registerOrder(trader.getId(), *bestBid, askSize, Side::SELL, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
                trader.addActiveOrderId(res.orderId, *bestBid);
        }
        return;
    }

    // Otherwise post passive value-based quote
    std::bernoulli_distribution sideBias(0.5);

    if (sideBias(rng))
    {
        if (bidSize > 0 && trader.getStocks() < maxInv)
        {
            auto res = LOB.registerOrder(trader.getId(), bidPrice, bidSize, Side::BUY, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
                trader.addActiveOrderId(res.orderId, bidPrice);
        }
    }
    else
    {
        if (askSize > 0)
        {
            auto res = LOB.registerOrder(trader.getId(), askPrice, askSize, Side::SELL, clock);
            if (res.reason == RejectReason::None && res.orderId != 0)
                trader.addActiveOrderId(res.orderId, askPrice);
        }
    }
}