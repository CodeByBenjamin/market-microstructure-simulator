#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <vector>

#include "LimitOrderBook.h"
#include "UIHelpers.h"

class LOBPanel : public sf::Drawable
{
private:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    const sf::Font& font;

    float lobWidth;
    int maxCount = 25;
    int labelCount = 0;

    float rowHeight = 30.f;
    float padding = 5.f;
    float currentY = 130.f;

    int activeBids;
    int activeAsks;

    sf::RectangleShape panel;
    sf::RectangleShape sideSeperator;
    sf::RectangleShape panelSeperator;
    sf::RectangleShape bestPricesSeperator;

    sf::VertexArray bidBars;
    sf::VertexArray askBars;

    std::vector<sf::Text> labels;

    void setupLabel(int index, const sf::Font& font, const std::string& str,
        unsigned int size, sf::Color color,
        float x, float y, UISnap snap, float offset);

public:
    LOBPanel(sf::Vector2u winSize, const sf::Font& font);

    void update(const LimitOrderBook& LOB);
};