#include <iomanip>
#include <sstream>
#include <string>
#include <ios>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector2.hpp>

#include "UIHelpers.h"

std::string UIHelper::formatPrice(double price)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << price;
    return ss.str();
}

void UIHelper::drawLabel(sf::RenderTarget& target, const sf::Font& font, const std::string& label, int fontSize, float x, float y, UISnap snap, float offset, sf::Color color)
{
    sf::Text text(font);
    text.setCharacterSize(fontSize);
    text.setFillColor(color);
    text.setString(label);

    sf::FloatRect bounds = text.getLocalBounds();

    switch (snap)
    {
    case UISnap::Left:
        text.setOrigin({ 0.f, 0.f });
        break;
    case UISnap::Center:
        text.setOrigin({ std::round(bounds.size.x / 2.f), 0.f });
        break;
    case UISnap::Right:
        text.setOrigin({ std::round(bounds.size.x), 0.f });        
        break;
    }

    text.setPosition({ std::round(x + offset), std::round(y) });

    target.draw(text);
}

void UIHelper::drawColoredRect(sf::RenderTarget& target, float x, float y, float width, float height, UISnap snap, float offset, sf::Color color)
{
    sf::RectangleShape rect;

    rect.setSize(sf::Vector2f(width, height));
    rect.setFillColor(color);

    switch (snap)
    {
    case UISnap::Left:
        break;
    case UISnap::Center:
        rect.setOrigin({ width / 2.f, height / 2.f });  
        break;
    case UISnap::Right:
        rect.setOrigin({ width, 0.f });
        break;
    }

    rect.setPosition({ x + offset, y });

    target.draw(rect);
}