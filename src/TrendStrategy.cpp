#include <cmath>
#include <random>
#include <algorithm>
#include <cstdlib>

#include "datatypes.h"
#include "TrendStrategy.h"
#include "Trader.h"
#include "LimitOrderBook.h"
#include "Clock.h"

void TrendStrategy::decide(Trader& trader, LimitOrderBook& LOB, Clock& clock)
{
	static std::mt19937 rng(std::random_device{}());
	size_t depth = 100; //How many last trades to look at

	auto const& midPriceHistory = LOB.getMidPriceHistory();

	if (midPriceHistory.empty())
		return;

	double sum = 0;
	size_t count = 0;
	for (auto it = midPriceHistory.rbegin();
		it != midPriceHistory.rend() && count < depth;
		++it, ++count)
	{
		sum += *it;
	}

	size_t actualDepth = std::min(depth, midPriceHistory.size());
	double avr = sum / actualDepth;

	if (avr <= 0.0) return;

	double currentPrice = midPriceHistory.back();
	double diff = currentPrice - avr;

	double threshold = avr * 0.001;

	bool buyingTheDip = false;
	if (trader.getFunds() >= 10000) {
		buyingTheDip = true;
	}

	bool cashOut = false;
	if (trader.getStocks() >= 500) {
		cashOut = true;
	}

	if (diff > threshold && !cashOut)
	{
		auto const& asks = LOB.getAsks();
		if (asks.empty()) return;

		auto bestAskIt = asks.begin();

		double executionPrice = bestAskIt->first * 1.05;

		double funds = trader.getFunds();
		long canBuy = static_cast<long>(std::floor(funds / executionPrice));

		if (canBuy <= 0) return;

		double perc = diff / avr;

		long minVol = static_cast<long>(canBuy * perc);
		long maxVol = static_cast<long>(canBuy * 0.3);

		if (minVol >= maxVol) minVol = maxVol / 2;
		std::uniform_int_distribution<long> dist(std::max(1L, minVol), std::max(1L, maxVol));

		long willBuy = std::clamp(
			dist(rng), 
			1L,
			canBuy
		);

		LOB.processOrder(trader.getId(), executionPrice, willBuy, Side::BUY, clock);
	}
	else if (diff < -threshold && !buyingTheDip)
	{
		auto const& bids = LOB.getBids();
		if (bids.empty()) return;

		auto bestBidIt = bids.begin();

		double executionPrice = bestBidIt->first * 0.95;

		long canSell = trader.getStocks();

		if (canSell <= 0) return;

		double perc = std::abs(diff) / avr;

		long minVol = static_cast<long>(canSell * perc);
		long maxVol = static_cast<long>(canSell * 0.3);

		if (minVol >= maxVol) minVol = maxVol / 2;
		std::uniform_int_distribution<long> dist(std::max(1L, minVol), std::max(1L, maxVol));

		long willSell = std::clamp(
			dist(rng),
			1L,
			canSell
		);

		LOB.processOrder(trader.getId(), executionPrice, willSell, Side::SELL, clock);
	}
}