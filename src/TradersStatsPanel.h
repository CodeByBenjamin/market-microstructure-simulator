#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <vector>

#include "LimitOrderBook.h"
#include "UIHelpers.h"

class TradersStatsPanel : public sf::Drawable
{
private:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    const sf::Font& font;

    float panelWidth;
    float panelHeight;
    int maxCount = 25;
    int labelCount = 0;

    float rowHeight = 30.f;
    float padding = 5.f;
    float currentY = 130.f;

    int boxCount;

    sf::RectangleShape panel;

    sf::VertexArray statBoxes;

    std::vector<sf::Text> labels;

    void setupLabel(int index, const sf::Font& font, const std::string& str,
        unsigned int size, sf::Color color,
        float x, float y, UISnap snap, float offset);

public:
    TradersStatsPanel(sf::Vector2u winSize, const sf::Font& font);

    void update(const LimitOrderBook& LOB);
};