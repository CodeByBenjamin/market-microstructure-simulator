#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <algorithm>
#include <cmath>

#include "datatypes.h"
#include "LOBPanel.h"
#include "UIHelpers.h"
#include "LimitOrderBook.h"

LOBPanel::LOBPanel(sf::Vector2u winSize, const sf::Font& f)
    : font(f),
    lobWidth(static_cast<float>(winSize.x * 0.25f)),
    activeBids(0),
    activeAsks(0)
{
    labels.reserve(static_cast<std::size_t>(maxCount) * 8);

    for (int i = 0; i < 8 * maxCount; ++i) {
        labels.emplace_back(font);
    }

    panel.setPosition({ 0.f, 0.f });
    panel.setSize({ lobWidth, static_cast<float>(winSize.y) });
    panel.setFillColor(Theme::Surface);

    sideSeperator.setPosition({ lobWidth / 2.f - 1, 130.0f });
    sideSeperator.setSize({ 2.f, static_cast<float>(winSize.y) });
    sideSeperator.setFillColor(Theme::Border);

    panelSeperator.setPosition({ lobWidth, 0.f });
    panelSeperator.setSize({ 2.f, static_cast<float>(winSize.y) });
    panelSeperator.setFillColor(Theme::Border);

    float yPos = currentY + (rowHeight + padding / 2);
    bestPricesSeperator.setPosition({ 0.f, yPos });
    bestPricesSeperator.setSize({ lobWidth, 2.f });
    bestPricesSeperator.setFillColor(Theme::AccentBG);

    bidBars.setPrimitiveType(sf::PrimitiveType::Triangles);
    askBars.setPrimitiveType(sf::PrimitiveType::Triangles);
}

void LOBPanel::setupLabel(int index, const sf::Font& font, const std::string& str,
    unsigned int size, sf::Color color,
    float x, float y, UISnap snap, float offset)
{
    if (index >= labels.size()) return;

    sf::Text& text = labels[index];

    if (text.getString() == str) {
        text.setPosition({ std::round(x + offset), std::round(y) });
        return;
    }

    text.setString(str);
    text.setCharacterSize(size);
    text.setFillColor(color);

    sf::FloatRect bounds = text.getLocalBounds();

    text.setOrigin({ 0.f, 0.f });

    if (snap == UISnap::Right) {
        text.setOrigin({ std::round(bounds.size.x), 0.f });
    }
    else if (snap == UISnap::Center) {
        text.setOrigin({ std::round(bounds.size.x / 2.f), 0.f });
    }

    text.setPosition({ std::round(x + offset), std::round(y) });
}

void LOBPanel::update(const LimitOrderBook& LOB)
{
    labelCount = 0;
    activeBids = 0;
    activeAsks = 0;

    setupLabel(labelCount++, font, "BIDS ($)", 28, Theme::Bid,
        lobWidth * 0.25f, 20.f, UISnap::Center, 0.f);

    setupLabel(labelCount++, font, "ASKS ($)", 28, Theme::Ask,
        lobWidth * 0.75f, 20.f, UISnap::Center, 0.f);

    const auto& bids = LOB.getBids();
    const auto& asks = LOB.getAsks();

    // Main Prices

    auto bestBid = LOB.bestBid();
    if (!bestBid)
        bestBid = 0;
    auto bestAsk = LOB.bestAsk();
    if (!bestAsk)
        bestAsk = 0;
    PriceTicks midPrice = LOB.midPrice();
    PriceTicks spread = *bestAsk - *bestBid;

    float centerX = lobWidth / 2.f;

    std::string fullPrice = UIHelper::formatPrice(midPrice);
    size_t dotPos = fullPrice.find('.');
    std::string leftSide = fullPrice.substr(0, dotPos);
    std::string rightSide = fullPrice.substr(dotPos);

    setupLabel(labelCount++, font, leftSide, 30, Theme::TextDim,
        centerX, 3.f * currentY / 4.f, UISnap::Right, -9.f);

    setupLabel(labelCount++, font, rightSide, 30, Theme::TextDim,
        centerX, 3.f * currentY / 4.f, UISnap::Left, -9.f);

    fullPrice = UIHelper::formatPrice(spread);
    dotPos = fullPrice.find('.');
    leftSide = fullPrice.substr(0, dotPos);
    rightSide = fullPrice.substr(dotPos);

    setupLabel(labelCount++, font, leftSide, 22, Theme::Accent,
        centerX, currentY / 2.f, UISnap::Right, -7.f);

    setupLabel(labelCount++, font, rightSide, 22, Theme::Accent,
        centerX, currentY / 2.f, UISnap::Left, -7.f);

    Quantity maxVol = std::max(LOB.getHighestVolume(BUY, maxCount), LOB.getHighestVolume(SELL, maxCount));

    if (maxVol == 0) return;

    int count = 0;

    if (!bids.empty())
    {
        bidBars.resize(static_cast<std::size_t>(maxCount) * 6);
        for (auto it = bids.begin(); it != bids.end() && count < maxCount; ++it) {
            if (it->second.orderEntries.empty()) continue;
            Quantity levelVol = it->second.levelVolume;
            if (levelVol == 0) continue;

            float fullPerc = static_cast<float>(levelVol) / maxVol;
            float yPos = currentY + (rowHeight + padding) * count;

            bidBars[static_cast<std::size_t>(count) * 6].position = { centerX, yPos };
            bidBars[static_cast<std::size_t>(count) * 6 + 1].position = { centerX - centerX * fullPerc, yPos };
            bidBars[static_cast<std::size_t>(count) * 6 + 2].position = { centerX, yPos + rowHeight };
            bidBars[static_cast<std::size_t>(count) * 6 + 3].position = { centerX, yPos + rowHeight };
            bidBars[static_cast<std::size_t>(count) * 6 + 4].position = { centerX - centerX * fullPerc, yPos + rowHeight };
            bidBars[static_cast<std::size_t>(count) * 6 + 5].position = { centerX - centerX * fullPerc, yPos };

            for (int k = 0; k < 6; k++) bidBars[static_cast<std::size_t>(count) * 6 + k].color = Theme::BidBG;

            // Labels
            setupLabel(labelCount++, font, it->second.priceLabel, 24, Theme::Bid,
                centerX, yPos, UISnap::Right, -10.f);

            setupLabel(labelCount++, font, std::to_string(levelVol), 22, Theme::TextDim,
                0.f, yPos, UISnap::Left, 10.f);
            count++;
        }
        activeBids = count;
    }

    if (!asks.empty())
    {
        askBars.resize(static_cast<std::size_t>(maxCount) * 6);
        count = 0;
        for (auto it = asks.begin(); it != asks.end() && count < maxCount; ++it) {
            if (it->second.orderEntries.empty()) continue;
            Quantity levelVol = it->second.levelVolume;
            if (levelVol == 0) continue;

            float fullPerc = static_cast<float>(levelVol) / maxVol;
            float yPos = currentY + (rowHeight + padding) * count;

            askBars[static_cast<std::size_t>(count) * 6].position = { centerX, yPos };
            askBars[static_cast<std::size_t>(count) * 6 + 1].position = { centerX + centerX * fullPerc, yPos };
            askBars[static_cast<std::size_t>(count) * 6 + 2].position = { centerX, yPos + rowHeight };
            askBars[static_cast<std::size_t>(count) * 6 + 3].position = { centerX, yPos + rowHeight };
            askBars[static_cast<std::size_t>(count) * 6 + 4].position = { centerX + centerX * fullPerc, yPos + rowHeight };
            askBars[static_cast<std::size_t>(count) * 6 + 5].position = { centerX + centerX * fullPerc, yPos };

            for (int k = 0; k < 6; k++) askBars[static_cast<std::size_t>(count) * 6 + k].color = Theme::AskBG;

            // Labels
            setupLabel(labelCount++, font, it->second.priceLabel, 24, Theme::Ask,
                centerX, yPos, UISnap::Left, 10.f);

            setupLabel(labelCount++, font, std::to_string(levelVol), 22, Theme::TextDim,
                lobWidth, yPos, UISnap::Right, -10.f);
            count++;
        }
        activeAsks = count;
    }
}

void LOBPanel::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(panel, states);
    target.draw(sideSeperator, states);
    target.draw(panelSeperator, states);
    target.draw(bestPricesSeperator, states);

    if (activeBids > 0) {
        target.draw(&bidBars[0], static_cast<std::size_t>(activeBids) * 6, sf::PrimitiveType::Triangles, states);
    }

    if (activeAsks > 0) {
        target.draw(&askBars[0], static_cast<std::size_t>(activeAsks) * 6, sf::PrimitiveType::Triangles, states);
    }

    for (int i = 0; i < labelCount; i++) {
        target.draw(labels[i], states);
    }
}