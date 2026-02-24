#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <vector>

#include "datatypes.h"

class LimitOrderBook;

class DepthChart : public sf::Drawable {
private:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    float chartWidth;
    float chartHeight;
    float bottomOfChart;
    float startX;

    float padX = 5.f;
    float padY = 5.f;

    sf::VertexArray bidTriangles;
    sf::VertexArray askTriangles;

    sf::RectangleShape panel;

    Quantity totalVolume = 0;
    PriceTicks binSize;
    std::vector<DepthPoint> depthPoints;
public:
    DepthChart(sf::Vector2u winSize, PriceTicks binSize);

    void updateDepthPoints(const LimitOrderBook& LOB);
    void update(const LimitOrderBook& LOB);
};