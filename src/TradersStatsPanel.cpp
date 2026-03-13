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
    "Contrarian Strategy",
    "Noise Strategy",
    "Fundamental Strategy",
    "Momentum Strategy"
};

struct Totals {
    int traders = 0;

    PriceTicks allFunds = 0;
    Quantity allStocks = 0;
    PriceTicks startEquity = 0;

    PriceTicks winValue = 0;
    PriceTicks sellValue = 0;

    PriceTicks entrySum = 0;
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
   
    std::vector<Totals> totals(5);

    boxCount = totals.size();

    auto storeInfo = [&](int index, const TraderStats& stats, PriceTicks allFunds) {
            PriceTicks result;
        
            totals[index].startEquity += stats.startEquity;
            totals[index].allFunds += allFunds;
            totals[index].allStocks += stats.oldPosition;
            totals[index].winValue += stats.winSellValue;
            totals[index].sellValue += stats.totalSellValue;
            totals[index].traders++;
            if (mul_overflow_i64(stats.avgEntry, stats.oldPosition, result)) {
                std::cout << "FATAL: overflow in storeInfo\n";
                return;
            }
            totals[index].entrySum += result;
        };

    for (const auto& [id, trader] : LOB.getTraders())
    {
        auto stats = trader->getStats();
        auto type = trader->getType();

        PriceTicks funds = trader->getFunds();
        PriceTicks lockedFunds = trader->getLockedFunds();

        Quantity stocks = trader->getStocks();
        Quantity lockedStocks = trader->getLockedStocks();

        switch (type)
        {
        case Maker:
            storeInfo(static_cast<int>(Maker), stats, funds + lockedFunds);
            break;
        case Contrarian:
            storeInfo(static_cast<int>(Contrarian), stats, funds + lockedFunds);
            break;
        case Noise:
            storeInfo(static_cast<int>(Noise), stats, funds + lockedFunds);
            break;
        case Fundamental:
            storeInfo(static_cast<int>(Fundamental), stats, funds + lockedFunds);
            break;
        case Momentum:
            storeInfo(static_cast<int>(Momentum), stats, funds + lockedFunds);
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
    float boxHeight = (panel.getSize().y - 2 * padding - 15.f) / (totals.size());

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

        int profSellPct = 0;
        if (totals[i].sellValue != 0)
            profSellPct = std::round((static_cast<double>(totals[i].winValue) / totals[i].sellValue) * 100);

        profSellPct = std::clamp(profSellPct, 0, 100);

        PriceTicks avgEntry = 0;
        if (totals[i].allStocks != 0)
            avgEntry = totals[i].entrySum / totals[i].allStocks;

        // Labels
        setupLabel(labelCount++, font, std::string(TraderTypeNames[i]), 22, Theme::TextMain, xPos, yPos, UISnap::Left, 10.f);
         
        PriceTicks equity = 0;
        if (mul_overflow_i64(LOB.midPrice(), totals[i].allStocks, equity)) {
            std::cout << "FATAL: overflow in equityCal\n";
            return;
        }
        equity += totals[i].allFunds;

        PriceTicks pnl = equity - totals[i].startEquity;

        const char* space = " ";
        sf::Color pnlColor = Theme::Bid;
        if (pnl < 0)
        {
            space = "";
            pnlColor = Theme::Ask;
        }

        sf::Color profSellPctColor;
        if (profSellPct < 40)
        {
            profSellPctColor = Theme::Ask;
        }
        else if (profSellPct < 60)
        {
            profSellPctColor = Theme::TextMain;
        }
        else
        {
            profSellPctColor = Theme::Bid;
        }

        setupLabel(labelCount++, font, "Traders", 18, Theme::TextMain, xPos, yPos + 50.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(totals[i].traders), 18, Theme::TextMain, xPos + 200.f, yPos + 50.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "PnL $", 18, Theme::TextMain, xPos, yPos + 70.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, space + UIHelper::formatPrice(pnl) + "$", 18, pnlColor, xPos + 200.f, yPos + 70.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Prof. Sell %", 18, Theme::TextMain, xPos, yPos + 90.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(profSellPct) + "%", 18, profSellPctColor, xPos + 200.f, yPos + 90.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Avg. Entry $", 18, Theme::TextMain, xPos, yPos + 110.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + UIHelper::formatPrice(avgEntry) + "$", 18, Theme::TextMain, xPos + 200.f, yPos + 110.f, UISnap::Left, 0.f);

        setupLabel(labelCount++, font, "Inventory", 18, Theme::TextMain, xPos, yPos + 130.f, UISnap::Left, 10.f);
        setupLabel(labelCount++, font, " " + std::to_string(totals[i].allStocks), 18, Theme::TextMain, xPos + 200.f, yPos + 130.f, UISnap::Left, 0.f);
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