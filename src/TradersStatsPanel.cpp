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
#include <iostream>

#include "datatypes.h"
#include "LOBPanel.h"
#include "UIHelpers.h"
#include "TradersStatsPanel.h"

const char* TraderTypeNames[] =
{
    "Market Maker",
    "Trend Strategy"
};

struct Totals {
    int traders = 0;

    PriceTicks pnl = 0;

    int wins = 0;
    int sells = 0;

    PriceTicks entrySum = 0;
    Quantity positionSum = 0;
};

TradersStatsPanel::TradersStatsPanel(sf::Vector2u winSize, const sf::Font& f)
    : font(f),
    panelWidth(static_cast<float>(winSize.x * 0.25f)),
    panelHeight(static_cast<float>(winSize.y * 0.75f))
{
    labels.reserve(static_cast<std::size_t>(maxCount) * 8);
    
    boxCount = 0;

    for (int i = 0; i < 8 * maxCount; ++i) {
        labels.emplace_back(font);
    }

    panel.setPosition({ static_cast<float>(winSize.x) - panelWidth, 0.f});
    panel.setSize({ panelWidth, panelHeight });
    panel.setFillColor(Theme::Surface);

    statBoxes.setPrimitiveType(sf::PrimitiveType::Triangles);
}

void TradersStatsPanel::setupLabel(int index, const sf::Font& font, const std::string& str,
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

void TradersStatsPanel::update(const LimitOrderBook& LOB)
{
    labelCount = 0;

    const auto& traders = LOB.getTraders();
   
    std::vector<Totals> totals(2);

    boxCount = totals.size();

    for (const auto& [id, trader] : LOB.getTraders())
    {
        auto stats = trader->getStats();
        auto type = trader->getType();

        PriceTicks result;

        switch (type)
        {
        case Maker:
            totals[static_cast<int>(Maker)].pnl += stats.pnl;
            totals[static_cast<int>(Maker)].wins += stats.wins;
            totals[static_cast<int>(Maker)].sells += stats.sellFills;
            totals[static_cast<int>(Maker)].traders++;
            if (mul_overflow_i64(stats.avgEntry, stats.position, result)) {
                std::cout << "FATAL: overflow in recordTrade\n";
                return;
            }
            totals[static_cast<int>(Maker)].entrySum += result;
            totals[static_cast<int>(Maker)].positionSum += stats.position;
            break;
        case Trend:
            totals[static_cast<int>(Trend)].pnl += stats.pnl;
            totals[static_cast<int>(Trend)].wins += stats.wins;
            totals[static_cast<int>(Trend)].sells += stats.sellFills;
            totals[static_cast<int>(Trend)].traders++;
            if (mul_overflow_i64(stats.avgEntry, stats.position, result)) {
                std::cout << "FATAL: overflow in recordTrade\n";
                return;
            }
            totals[static_cast<int>(Trend)].entrySum += result;
            totals[static_cast<int>(Trend)].positionSum += stats.position;
            break;
        default:
            break;
        }
    }

    float startXPos = panel.getPosition().x + padding;
    float startYPos = panel.getPosition().y + padding;

    float xPos = startXPos;
    float yPos;

    float boxWidth = panel.getSize().x - 2 * padding;
    float boxHeight = (panel.getSize().y - 2 * padding) / (totals.size());

    if (statBoxes.getVertexCount() != totals.size() * 6) {
        statBoxes.resize(totals.size() * 6);
    }

    for (int i = 0; i < totals.size(); i++)
    {
        yPos = startYPos + (boxHeight + padding) * i;

        statBoxes[static_cast<std::size_t>(i) * 6].position = { xPos, yPos};
        statBoxes[static_cast<std::size_t>(i) * 6 + 1].position = { xPos + boxWidth, yPos };
        statBoxes[static_cast<std::size_t>(i) * 6 + 2].position = { xPos, yPos + boxHeight };
        statBoxes[static_cast<std::size_t>(i) * 6 + 3].position = { xPos + boxWidth, yPos };
        statBoxes[static_cast<std::size_t>(i) * 6 + 4].position = { xPos, yPos + boxHeight };
        statBoxes[static_cast<std::size_t>(i) * 6 + 5].position = { xPos + boxWidth, yPos + boxHeight };

        for (int k = 0; k < 6; k++) statBoxes[static_cast<std::size_t>(i) * 6 + k].color = Theme::BoxSurface;

        int winrate = 0;
        if (totals[i].sells != 0)
            winrate = std::round((static_cast<float>(totals[i].wins) / totals[i].sells) * 100);

        PriceTicks avgEntry = 0;
        if (totals[i].positionSum != 0)
            avgEntry = totals[i].entrySum / totals[i].positionSum;

        // Labels
        setupLabel(labelCount++, font, std::string(TraderTypeNames[i]), 26, Theme::TextMain, xPos, yPos, UISnap::Left, 10.f);

        const char* space = " ";
        sf::Color pnlColor = Theme::Bid;
        if (totals[i].pnl < 0)
        {
            space = "";
            pnlColor = Theme::Ask;
        }

        sf::Color winrateColor;
        if (winrate < 40)
        {
            winrateColor = Theme::Ask;
        }
        else if (winrate < 60)
        {
            winrateColor = Theme::TextMain;
        }
        else
        {
            winrateColor = Theme::Bid;
        }

        setupLabel(labelCount++, font, "Traders", 22, Theme::TextMain, xPos, yPos + 50.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(totals[i].traders), 22, Theme::TextMain, xPos + 200.f, yPos + 50.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "PnL", 22, Theme::TextMain, xPos, yPos + 80.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, space + UIHelper::formatPrice(totals[i].pnl), 22, pnlColor, xPos + 200.f, yPos + 80.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Winrate", 22, Theme::TextMain, xPos, yPos + 110.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(winrate) + "%", 22, winrateColor, xPos + 200.f, yPos + 110.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Avg. Entry", 22, Theme::TextMain, xPos, yPos + 140.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + UIHelper::formatPrice(avgEntry), 22, Theme::TextMain, xPos + 200.f, yPos + 140.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Inventory", 22, Theme::TextMain, xPos, yPos + 170.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(totals[i].positionSum), 22, Theme::TextMain, xPos + 200.f, yPos + 170.f, UISnap::Left, 0.f);
    }
}

void TradersStatsPanel::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(panel, states);
    target.draw(&statBoxes[0], static_cast<std::size_t>(boxCount) * 6, sf::PrimitiveType::Triangles, states);

    for (int i = 0; i < labelCount; i++) {
        target.draw(labels[i], states);
    }
}