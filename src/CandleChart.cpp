#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "CandleChart.h"
#include "UIHelpers.h"

CandleChart::CandleChart(const sf::Font& font, Tick binSize, int candlesVisible)
	: font(font),
	binSize(binSize),
	nextBinTick(binSize),
	candlesVisible(candlesVisible)
{
	candleQuads.setPrimitiveType(sf::PrimitiveType::Triangles);
	wickQuads.setPrimitiveType(sf::PrimitiveType::Triangles);
	gridLines.setPrimitiveType(sf::PrimitiveType::Lines);
}

void CandleChart::update(LimitOrderBook& LOB, sf::Vector2u winSize, Tick currentTick)
{
	if (candlesVisible <= 0) return;

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

	PriceTicks visibleMin = candles.front().low;
	PriceTicks visibleMax = candles.front().high;

	for (const auto& c : candles) {
		if (c.high > visibleMax) visibleMax = c.high;
		if (c.low < visibleMin) visibleMin = c.low;
	}
	PriceTicks visibleRange = std::max<PriceTicks>(1, visibleMax - visibleMin);

	PriceTicks pad = std::max<PriceTicks>(1, (PriceTicks)std::llround((double)visibleRange * 0.05));
	pad = std::max<PriceTicks>(1, pad);

	PriceTicks targetMin = visibleMin - pad;
	PriceTicks targetMax = visibleMax + pad;
	PriceTicks targetRange = std::max<PriceTicks>(1, targetMax - targetMin);

	if (!scaleInit) {
		scaleMinP = targetMin;
		scaleMaxP = targetMax;
		scaleInit = true;
	}
	else {
		PriceTicks curRange = std::max<PriceTicks>(1, scaleMaxP - scaleMinP);

		const double expandTrigger = 0.92;
		const double shrinkTrigger = 0.55;
		const double maxExpandRatio = 1.25;
		const double maxShrinkRatio = 0.85;

		double used = (double)targetRange / (double)curRange;

		bool needExpand = (targetMin < scaleMinP) || (targetMax > scaleMaxP) || (used > expandTrigger);
		bool canShrink = (used < shrinkTrigger);

		if (needExpand) {
			scaleMinP = (PriceTicks)std::llround(scaleMinP * 0.7 + targetMin * 0.3);
			scaleMaxP = (PriceTicks)std::llround(scaleMaxP * 0.7 + targetMax * 0.3);
		}
		else if (canShrink) {
			scaleMinP = (PriceTicks)std::llround(scaleMinP * 0.9 + targetMin * 0.1);
			scaleMaxP = (PriceTicks)std::llround(scaleMaxP * 0.9 + targetMax * 0.1);
		}

		if (scaleMinP > targetMin) scaleMinP = targetMin;
		if (scaleMaxP < targetMax) scaleMaxP = targetMax;
	}

	float emptySpace = 160.f;
	float candleWidth = (chartWidth - emptySpace) / (float)candlesVisible;
	candleWidth = std::max(1.0f, candleWidth);
	float wickWidth = 2.f;

	wickQuads.resize(6 * candles.size());
	candleQuads.resize(6 * candles.size());

	yLabels.clear();

	scaleMinP = std::max<PriceTicks>(0, scaleMinP);
	scaleMaxP = std::max<PriceTicks>(scaleMinP + 1, scaleMaxP);

	PriceTicks minP = scaleMinP;
	PriceTicks maxP = scaleMaxP;
	PriceTicks range = std::max<PriceTicks>(1, maxP - minP);
	double denom = (double)range;

	auto priceToY = [&](PriceTicks p) -> float {
		double t = (double)(p - minP) / denom;
		return chartHeight - (float)(t * chartHeight);
		};

	auto niceStepTicks = [](PriceTicks rawStep) -> PriceTicks {
		if (rawStep <= 1) return 1;

		PriceTicks p10 = 1;
		while (p10 * 10 <= rawStep) p10 *= 10;

		PriceTicks m = rawStep / p10;
		PriceTicks niceM;
		if (m <= 1) niceM = 1;
		else if (m <= 2) niceM = 2;
		else if (m <= 5) niceM = 5;
		else niceM = 10;

		return niceM * p10;
		};

	yLabels.clear();
	gridLines.clear();

	float topPad = labelSize * 0.9f;
	float botPad = labelSize * 0.9f;

	//How many labels can fit without overlap
	float desiredPx = labelSize * 1.6f;
	int maxLabels = (int)(chartHeight / desiredPx);
	if (maxLabels < 2) maxLabels = 2;
	if (maxLabels > 14) maxLabels = 14;

	//Pick a step in ticks that yields ~maxLabels labels
	PriceTicks rawStep = std::max<PriceTicks>(1, range / maxLabels);
	PriceTicks step = niceStepTicks(rawStep);

	//Align start/end to the step grid
	PriceTicks start = (minP / step) * step;
	PriceTicks end = ((maxP + step - 1) / step) * step;

	float lastY = -1e9f;

	for (PriceTicks p = start; p <= end; p += step) {
		float y = priceToY(p);

		if (y < topPad) y = topPad;
		if (y > chartHeight - botPad) y = chartHeight - botPad;

		if (std::abs(y - lastY) < labelSize * 1.2f) continue;
		lastY = y;

		gridLines.append({ {lobWidth + 2.f, y}, Theme::Border });
		gridLines.append({ {lobWidth + chartWidth - 100.f, y}, Theme::Border });

		sf::Text txt(font);
		txt.setCharacterSize(labelSize);
		txt.setFillColor(Theme::TextMain);
		txt.setString(UIHelper::formatPrice(p));

		float labelX = lobWidth + chartWidth - 80.f;

		auto b = txt.getLocalBounds();
		txt.setOrigin({ 0.f, b.position.y + b.size.y * 0.5f });
		txt.setPosition({ labelX, y });

		yLabels.push_back(txt);
	}

	for (size_t i = 0; i < candles.size(); i++)
	{
		const auto& candle = candles[i];

		float yOpen = priceToY(candle.open);
		float yHigh = priceToY(candle.high);
		float yLow = priceToY(candle.low);
		float yClose = priceToY(candle.close);

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
		wickQuads[i * 6].color = Theme::Wick;
		wickQuads[i * 6 + 1].color = Theme::Wick;
		wickQuads[i * 6 + 2].color = Theme::Wick;
		wickQuads[i * 6 + 3].color = Theme::Wick;
		wickQuads[i * 6 + 4].color = Theme::Wick;
		wickQuads[i * 6 + 5].color = Theme::Wick;

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
	target.draw(gridLines);
	
	target.draw(wickQuads, states);
	target.draw(candleQuads, states);

	for (const auto& t : yLabels)
		target.draw(t);
}