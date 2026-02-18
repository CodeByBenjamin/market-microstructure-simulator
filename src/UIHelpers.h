#pragma once

#include <string>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

namespace Theme {
    inline const sf::Color Background = sf::Color(23, 27, 36);   // Background color
    inline const sf::Color Surface = sf::Color(43, 43, 64);    // Panel backgrounds
    inline const sf::Color Border = sf::Color(47, 51, 61);    // Dividers

    inline const sf::Color TextMain = sf::Color(242, 242, 242); // Primary numbers
    inline const sf::Color TextDim = sf::Color(139, 148, 158); // Timestamps/Labels

    inline const sf::Color Bid = sf::Color(72, 168, 100);  // Green
    inline const sf::Color Ask = sf::Color(188, 72, 72);   // Red
    inline const sf::Color BidBG = sf::Color(72, 168, 100, 80);  // BG Green
    inline const sf::Color AskBG = sf::Color(188, 72, 72, 80);   // Bg Red
    inline const sf::Color Accent = sf::Color(210, 153, 34);  // "Special" info
}

enum class UISnap { Left, Center, Right };

class UIHelper
{
public:
    static std::string formatPrice(double price);

    static void drawLabel(sf::RenderTarget& target, sf::Text text, float x, float y, UISnap snap, float offset);
    static void drawColoredRect(sf::RenderTarget& target, float x, float y, float width, float height, UISnap snap, float offset, sf::Color color);
};