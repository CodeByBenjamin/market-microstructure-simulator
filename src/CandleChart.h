#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <deque>

#include "datatypes.h"

class LimitOrderBook;

class CandleChart : public sf::Drawable
{
private:
	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

	long binSize;
	long nextBinTick;

	int candlesVisible;
	std::deque<Candle> candles;

	sf::VertexArray candleQuads;
	sf::VertexArray wickQuads;
public:
	CandleChart(long binSize, int candlesVisible);

	void update(LimitOrderBook& LOB, sf::Vector2u winSize, long currentTick);
};