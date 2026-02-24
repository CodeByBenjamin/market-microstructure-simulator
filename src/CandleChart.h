#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Text.hpp>
#include <deque>

#include "datatypes.h"

class LimitOrderBook;

class CandleChart : public sf::Drawable
{
private:
	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

	PriceTicks scaleMinP = 0;
	PriceTicks scaleMaxP = 0;
	bool scaleInit = false;

	Tick binSize;
	Tick nextBinTick;

	int candlesVisible;
	std::deque<Candle> candles;

	sf::VertexArray candleQuads;
	sf::VertexArray wickQuads;

	sf::VertexArray gridLines;

	const sf::Font& font;
	unsigned int labelSize = 12;
	std::vector<sf::Text> yLabels;
public:
	CandleChart(const sf::Font& font, Tick binSize, int candlesVisible);

	void update(LimitOrderBook& LOB, sf::Vector2u winSize, Tick currentTick);
};