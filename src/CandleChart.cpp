#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <vector>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "CandleChart.h"
#include "UIHelpers.h"

CandleChart::CandleChart(long binSize, int candlesVisible)
	: binSize(binSize),
	nextBinTick(binSize),
	candlesVisible(candlesVisible)
{ }

void CandleChart::update(LimitOrderBook& LOB, long currentTick)
{
	auto tradeHistory = LOB.flushTrades();
	if (tradeHistory.empty()) return;

	if (candles.empty() || currentTick >= nextBinTick) {
		nextBinTick = (currentTick / binSize + 1) * binSize;

		Candle newCandle = {};
		newCandle.open = tradeHistory.front().price;
		newCandle.high = tradeHistory.front().price;
		newCandle.low = tradeHistory.front().price;
		newCandle.close = tradeHistory.back().price;

		for (const auto& trade : tradeHistory) {
			if (trade.price > newCandle.high) newCandle.high = trade.price;
			if (trade.price < newCandle.low)  newCandle.low = trade.price;
		}

		candles.push_back(newCandle);

		if (candles.size() > candlesVisible)
			candles.pop_front();
	}
	else {
		Candle& liveCandle = candles.back();
		for (const auto& t : tradeHistory) {
			if (t.price > liveCandle.high) liveCandle.high = t.price;
			if (t.price < liveCandle.low)  liveCandle.low = t.price;
		}
		liveCandle.close = tradeHistory.back().price;
	}
}

void CandleChart::draw(sf::RenderTarget& target, sf::RenderStates states) const {
	float chartWidth = static_cast<float>(target.getView().getSize().x * 0.5f);
	float chartHeight = static_cast<float>(target.getView().getSize().y);

	float lobWidth = static_cast<float>(target.getView().getSize().x * 0.25f);

	double minP = 1e18, maxP = -1e18;
	for (const auto& c : candles) {
		if (c.high > maxP) maxP = c.high;
		if (c.low < minP)  minP = c.low;
	}

	double range = maxP - minP;
	maxP += range * 0.05;
	minP -= range * 0.05;

	float emptySpace = 160;

	float candleWidth = (chartWidth - emptySpace) / candlesVisible;
	float wickWidth = 2;

	int i = 0;

	for (size_t i = 0; i < candles.size(); i++)
	{
		const auto& candle = candles[i];

		float yOpen = chartHeight - ((candle.open - minP) / (maxP - minP) * chartHeight);
		float yHigh = chartHeight - ((candle.high - minP) / (maxP - minP) * chartHeight);
		float yLow = chartHeight - ((candle.low - minP) / (maxP - minP) * chartHeight);
		float yClose = chartHeight - ((candle.close - minP) / (maxP - minP) * chartHeight);

		float x = lobWidth + (i * candleWidth) + (candleWidth / 2.0f);	

		float wickCenterY = (yHigh + yLow) / 2.0f;
		float totalWickHeight = std::abs(yLow - yHigh);

		if (totalWickHeight < 1.0f) totalWickHeight = 1.0f;

		//Draw wick
		UIHelper::drawColoredRect(target, x, wickCenterY, wickWidth, totalWickHeight, UISnap::Center, 0, Theme::TextDim);

		float candleTop = fmin(yOpen, yClose);
		float candleBottom = fmax(yOpen, yClose);
		float bodyHeight = fmax(candleBottom - candleTop, 1.0f);
		float bodyCenterY = (candleTop + candleBottom) / 2.0f;

		sf::Color candleColor;
		if (candle.close > candle.open) {
			candleColor = Theme::Bid;
		}
		else if (candle.close < candle.open) {
			candleColor = Theme::Ask;
		}
		else {
			candleColor = Theme::TextDim;
		}

		//Draw body over wick
		UIHelper::drawColoredRect(target, x, bodyCenterY, candleWidth * 0.8f, bodyHeight, UISnap::Center, 0, candleColor);
	}
}