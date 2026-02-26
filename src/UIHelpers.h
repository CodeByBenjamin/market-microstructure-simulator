#pragma once

#include <string>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Text.hpp>

#include "datatypes.h"

namespace Theme {
    inline const sf::Color Background = sf::Color(23, 27, 36);   // Background color
    inline const sf::Color Surface = sf::Color(43, 43, 64);    // Panel backgrounds
    inline const sf::Color Border = sf::Color(47, 51, 61);    // Dividers/Grid lines

    inline const sf::Color TextMain = sf::Color(242, 242, 242); // Primary numbers
    inline const sf::Color TextDim = sf::Color(139, 148, 158); // Timestamps/Labels

    inline const sf::Color Bid = sf::Color(72, 168, 100);  // Green
    inline const sf::Color Ask = sf::Color(188, 72, 72);   // Red
    inline const sf::Color BidBG = sf::Color(72, 168, 100, 40);  // BG Green
    inline const sf::Color AskBG = sf::Color(188, 72, 72, 40);   // BG Red
    inline const sf::Color Accent = sf::Color(210, 153, 34);  // "Special" info
    inline const sf::Color AccentBG = sf::Color(210, 153, 34, 40);  // "Special" info BG

    inline const sf::Color Wick = sf::Color(110, 118, 128);  //Wick color
}

enum class UISnap { Left, Center, Right };

class UIHelper
{
public:
    static std::string formatPrice(PriceTicks priceTicks);

    static void drawLabel(sf::RenderTarget& target, sf::Text text, float x, float y, UISnap snap, float offset);
    static void drawColoredRect(sf::RenderTarget& target, float x, float y, float width, float height, UISnap snap, float offset, sf::Color color);
};