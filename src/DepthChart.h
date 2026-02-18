#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>

#include "datatypes.h"

class LimitOrderBook;

class DepthChart : public sf::Drawable {
private:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    sf::VertexArray bidTriangles;
    sf::VertexArray askTriangles;

    long totalVolume = 0;
    float binSize;
    std::vector<DepthPoint> depthPoints;
public:
    DepthChart(float binSize);

    void updateDepthPoints(const LimitOrderBook& LOB);
    void update(const LimitOrderBook& LOB, sf::Vector2u winSize);
};