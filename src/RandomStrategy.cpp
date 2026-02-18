#include <random>

#include "datatypes.h"
#include "RandomStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"

void RandomStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) {
    static std::mt19937 rng(std::random_device{}());

    double perceivedValue = 20.f;

    double marketPrice = LOB.getMidPriceHistory().empty() ? perceivedValue : LOB.getMidPriceHistory().back();

    double mid = (marketPrice * 0.7) + (perceivedValue * 0.3);

    if (clock.now() % 4 == 0 && trader.getOrderCount() > 5)
    {
        trader.clearHalfOrders(LOB);
    }

    std::uniform_real_distribution<double> distDist(0.001, 0.01); // 0.01% to 1%
    std::uniform_int_distribution<long> volDist(5, 20);

    double myOffset = mid * distDist(rng);

    std::uniform_real_distribution<double> jitter(-0.005, 0.005);
    double myRefPrice = mid * (1.0 + jitter(rng));

    double price = myRefPrice - myOffset;

    if (price < 0.01) price = 0.01;

    long orderId;

    if (LOB.processOrder(&orderId, trader.getId(), price, volDist(rng), Side::BUY, clock))
    {
        trader.addActiveOrderId(orderId, price);
    }

    price = myRefPrice + myOffset;

    if (price < 0.01) price = 0.01;

    if (LOB.processOrder(&orderId, trader.getId(), price, volDist(rng), Side::SELL, clock))
    {
        trader.addActiveOrderId(orderId, price);
    }

    //10% chance to make an aggresive trade
    std::uniform_int_distribution<int> chance(1, 100);
    if (chance(rng) > 90) {
        if (chance(rng) > 50) {
            if (!LOB.getAsks().empty()) {
                double bestAsk = LOB.getAsks().begin()->first;

                if (LOB.processOrder(&orderId, trader.getId(), bestAsk, 5, Side::BUY, clock))
                {
                    trader.addActiveOrderId(orderId, price);
                }     
            }
        }
        else {
            if (!LOB.getBids().empty()) {
                double bestBid = LOB.getBids().begin()->first;

                if (LOB.processOrder(&orderId, trader.getId(), bestBid, 5, Side::SELL, clock))
                {
                    trader.addActiveOrderId(orderId, price);
                }
            }
        }
    }
}