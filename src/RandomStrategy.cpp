#include <random>

#include "datatypes.h"
#include "RandomStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"

void RandomStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock) {
    static std::mt19937 rng(std::random_device{}());

    double growthRate = 1000.0;
    if (clock.now() > 70) growthRate = 200.0;

    double perceivedValue = 20.0 + static_cast<double>(clock.now()) / growthRate;

    //double perceivedValue = 20.0 + static_cast<double>(clock.now()) / 1000; // The "True" value
    double marketPrice = LOB.getMidPriceHistory().back();

    double mid = (marketPrice * 0.7) + (perceivedValue * 0.3);

    for (long id : trader.getActiveOrderIds()) {
        LOB.cancelOrder(id);
    }
    trader.clearActiveOrderIds();

    std::uniform_real_distribution<double> distDist(0.001, 0.01); // 0.01% to 1%
    std::uniform_int_distribution<long> volDist(5, 20);

    double myOffset = mid * distDist(rng);

    std::uniform_real_distribution<double> jitter(-0.005, 0.005);
    double myRefPrice = mid * (1.0 + jitter(rng));

    double price = myRefPrice - myOffset;

    if (price < 0.01) price = 0.01;
    trader.addActiveOrderId(LOB.processOrder(trader.getId(), price, volDist(rng), Side::BUY, clock));

    price = myRefPrice + myOffset;

    if (price < 0.01) price = 0.01;
    trader.addActiveOrderId(LOB.processOrder(trader.getId(), price, volDist(rng), Side::SELL, clock));

    //10% chance to make an aggresive trade
    std::uniform_int_distribution<int> chance(1, 100);
    if (chance(rng) > 90) {
        if (chance(rng) > 50) {
            if (!LOB.getAsks().empty()) {
                double bestAsk = LOB.getAsks().begin()->first;
                LOB.processOrder(trader.getId(), bestAsk, 5, Side::BUY, clock);
            }
        }
        else {
            if (!LOB.getBids().empty()) {
                double bestBid = LOB.getBids().begin()->first;
                LOB.processOrder(trader.getId(), bestBid, 5, Side::SELL, clock);
            }
        }
    }
}