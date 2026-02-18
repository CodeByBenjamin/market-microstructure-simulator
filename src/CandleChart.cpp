#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <vector>
#include <cstdlib>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "CandleChart.h"
#include "UIHelpers.h"

CandleChart::CandleChart(long binSize, int candlesVisible)
	: binSize(binSize),
	nextBinTick(binSize),
	candlesVisible(candlesVisible)
{
	candleQuads.setPrimitiveType(sf::PrimitiveType::Triangles);
	wickQuads.setPrimitiveType(sf::PrimitiveType::Triangles);
}

void CandleChart::update(LimitOrderBook& LOB, sf::Vector2u winSize, long currentTick)
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

	//Getting Quads

	float chartWidth = static_cast<float>(winSize.x * 0.5f);
	float chartHeight = static_cast<float>(winSize.y);

	float lobWidth = static_cast<float>(winSize.x * 0.25f);

	double minP = 1e18, maxP = -1e18;
	for (const auto& c : candles) {
		if (c.high > maxP) maxP = c.high;
		if (c.low < minP)  minP = c.low;
	}

	double range = maxP - minP;
	maxP += range * 0.05;
	minP -= range * 0.05;

	float emptySpace = 160.f;

	float candleWidth = (chartWidth - emptySpace) / candlesVisible;
	float wickWidth = 2.f;

	wickQuads.resize(6 * candles.size());
	candleQuads.resize(6 * candles.size());

	int i = 0;

	for (size_t i = 0; i < candles.size(); i++)
	{
		const auto& candle = candles[i];

		float yOpen = chartHeight - ((candle.open - minP) / (maxP - minP) * chartHeight);
		float yHigh = chartHeight - ((candle.high - minP) / (maxP - minP) * chartHeight);
		float yLow = chartHeight - ((candle.low - minP) / (maxP - minP) * chartHeight);
		float yClose = chartHeight - ((candle.close - minP) / (maxP - minP) * chartHeight);

		float xCenter = lobWidth + (i * candleWidth) + (candleWidth / 2.0f);

		float xWickLeft = xCenter - wickWidth / 2.f;
		float xWickRight = xCenter + wickWidth / 2.f;

		wickQuads[i * 6].position = { xWickLeft, yHigh };
		wickQuads[i * 6 + 1].position = { xWickRight, yHigh };
		wickQuads[i * 6 + 2].position = { xWickLeft, yLow };
		wickQuads[i * 6 + 3].position = { xWickLeft, yLow };
		wickQuads[i * 6 + 4].position = { xWickRight, yLow };
		wickQuads[i * 6 + 5].position = { xWickRight, yHigh };

		//Color
		wickQuads[i * 6].color = Theme::TextDim;
		wickQuads[i * 6 + 1].color = Theme::TextDim;
		wickQuads[i * 6 + 2].color = Theme::TextDim;
		wickQuads[i * 6 + 3].color = Theme::TextDim;
		wickQuads[i * 6 + 4].color = Theme::TextDim;
		wickQuads[i * 6 + 5].color = Theme::TextDim;

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

		float xCandleLeft = xCenter - candleWidth * 0.8f / 2.f;
		float xCandleRight = xCenter + candleWidth * 0.8f / 2.f;

		float yTop = std::min(yOpen, yClose);
		float yBottom = std::max(yOpen, yClose);

		if (std::abs(yTop - yBottom) < 1.0f) {
			yTop = yOpen - 0.5f;
			yBottom = yOpen + 0.5f;
		}

		candleQuads[i * 6].position = { xCandleLeft, yTop };
		candleQuads[i * 6 + 1].position = { xCandleRight, yTop };
		candleQuads[i * 6 + 2].position = { xCandleLeft, yBottom };
		candleQuads[i * 6 + 3].position = { xCandleLeft, yBottom };
		candleQuads[i * 6 + 4].position = { xCandleRight, yBottom };
		candleQuads[i * 6 + 5].position = { xCandleRight, yTop };

		//Color
		candleQuads[i * 6].color = candleColor;
		candleQuads[i * 6 + 1].color = candleColor;
		candleQuads[i * 6 + 2].color = candleColor;
		candleQuads[i * 6 + 3].color = candleColor;
		candleQuads[i * 6 + 4].color = candleColor;
		candleQuads[i * 6 + 5].color = candleColor;
	}
}

void CandleChart::draw(sf::RenderTarget& target, sf::RenderStates states) const {
	target.draw(wickQuads, states);
	target.draw(candleQuads, states);
}