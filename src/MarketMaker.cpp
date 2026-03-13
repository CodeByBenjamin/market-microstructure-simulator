#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "datatypes.h"
#include "MarketMaker.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"
#include "priceutils.h"
#include "rng.h"

void MarketMaker::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
    trader.clearOrdersPerc(LOB, 0.3f);

    const PriceTicks fairValue = 2000;
    const PriceTicks spread = 4;
    const Quantity size = 3;
    const Quantity targetInv = 35;
    const size_t depth = 20;

    PriceTicks mid = LOB.midPrice();

    auto const& history = LOB.getMidPriceHistory(); 
    PriceTicks avg = mid; 

    if (!history.empty()) 
    { 
        PriceTicks sum = 0; 
        size_t count = 0; 

        for (auto it = history.rbegin(); it != history.rend() && count < depth; ++it, ++count) 
            sum += *it; 

        if (count > 0) 
            avg = sum / count; 
    }

    PriceTicks ref = (mid > 0) ? 
        static_cast<PriceTicks>(std::llround(0.3 * fairValue + 0.7 * avg)) : fairValue;

    long long invDiff =
        static_cast<long long>(trader.getStocks()) - static_cast<long long>(targetInv);

    PriceTicks skew = static_cast<PriceTicks>(invDiff / 6);
    skew = std::clamp<PriceTicks>(skew, -6, 6);

    PriceTicks bid = ref - spread - skew;
    PriceTicks ask = ref + spread - skew;

    bid = std::max<PriceTicks>(1, bid);
    ask = std::max<PriceTicks>(bid + 1, ask);

    Quantity buyCap =
        trader.getFunds() / bid;

    Quantity sellCap =
        trader.getStocks();

    Quantity bidSize = std::min(size, buyCap);
    Quantity askSize = std::min(size, sellCap);

    if (invDiff > 6) {
        bidSize = std::min<Quantity>(1, bidSize);
        askSize = std::min<Quantity>(size + 1, sellCap);
    }
    else if (invDiff < -6) {
        askSize = std::min<Quantity>(1, askSize);
        bidSize = std::min<Quantity>(size + 1, buyCap);
    }

    if (buyCap > 0)
    {
        auto r = LOB.registerOrder(
            trader.getId(),
            bid,
            std::min(bidSize, buyCap),
            Side::BUY,
            clock
        );

        if (r.orderId)
            trader.addActiveOrderId(r.orderId, bid);
    }

    if (sellCap > 0)
    {
        auto r = LOB.registerOrder(
            trader.getId(),
            ask,
            std::min(askSize, sellCap),
            Side::SELL,
            clock
        );

        if (r.orderId)
            trader.addActiveOrderId(r.orderId, ask);
    }
}