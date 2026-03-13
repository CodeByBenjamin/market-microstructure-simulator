#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <optional>
#include <algorithm>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "Clock.h"
#include "datatypes.h"
#include "UIHelpers.h"
#include "LimitOrderBook.h"
#include "LOBPanel.h"
#include "DepthChart.h"
#include "Trader.h"
#include "ContrarianStrategy.h"
#include "MarketMaker.h"
#include "CandleChart.h"
#include "priceutils.h"
#include "TradersStatsPanel.h"
#include "NoiseStrategy.h"
#include "FundamentalStrategy.h"
#include "MomentumStrategy.h"

PriceTicks tradePriceSum = 0;
Quantity tradeCount = 0;

int main()
{
    auto window = sf::RenderWindow(
        sf::VideoMode({ 1920u, 1080u }),
        "Market simulator",
        sf::State::Fullscreen
    );
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if (!font.openFromFile("fonts/RobotoMono-Regular.ttf"))
    {
        std::cerr << "Error loading font!" << std::endl;
        return 1;
    }

    Clock clock;

    constexpr double dt = 1.0;
    constexpr Tick ticksPerSec = 15;
    constexpr double realDt = 1.0 / static_cast<double>(ticksPerSec);

    auto lastTime = std::chrono::high_resolution_clock::now();

    LimitOrderBook LOB;

    PriceTicks startMid = 0;
    {
        auto bb0 = LOB.bestBid();
        auto ba0 = LOB.bestAsk();
        if (bb0 && ba0)
            startMid = (*bb0 + *ba0) / 2;
    }


    LOBPanel lobPanel(window.getSize(), font);
    DepthChart depthChart(window.getSize(), 50);
    Tick binSize = static_cast<Tick>(ticksPerSec * 2);
    int candlesVisible = 60;
    CandleChart candleChart(font, binSize, candlesVisible);
    TradersStatsPanel tradersStatsPanel(window.getSize(), font);

    bool lobDirty = true;
    bool pauseSim = false;

    // Traders
    constexpr int momentumCount = 10;
    constexpr int fundamentalCount = 12; 
    constexpr int noiseCount = 20;
    constexpr int contrarianCount = 10;
    constexpr int makerCount = 12; 

    auto momentumStrategy = std::make_unique<MomentumStrategy>();
    auto fundamentalStrategy = std::make_unique<FundamentalStrategy>();
    auto noiseStrategy = std::make_unique<NoiseStrategy>();
    auto contrarianStrategy = std::make_unique<ContrarianStrategy>();
    auto makerStrategy = std::make_unique<MarketMaker>();

    std::vector<std::unique_ptr<Trader>> traderStorage;
    traderStorage.reserve(momentumCount + fundamentalCount + noiseCount + contrarianCount + makerCount);

    std::vector<Trader*> momentumTraders;
    std::vector<Trader*> fundamentalTraders;
    std::vector<Trader*> noiseTraders;
    std::vector<Trader*> contrarianTraders;
    std::vector<Trader*> marketMakers;

    momentumTraders.reserve(momentumCount);
    fundamentalTraders.reserve(fundamentalCount);
    noiseTraders.reserve(noiseCount);
    contrarianTraders.reserve(contrarianCount);
    marketMakers.reserve(makerCount);

    TraderId nextTraderId = 1;

    auto makeTrader = [&](auto* strategy,
        TraderType type,
        PriceTicks funds,
        Quantity stocks) -> Trader*
        {
            traderStorage.push_back(
                std::make_unique<Trader>(strategy, type, nextTraderId++, funds, stocks)
            );
            Trader* ptr = traderStorage.back().get();
            LOB.registerTrader(ptr);
            return ptr;
        };

    for (int i = 0; i < momentumCount; ++i)
    {
        momentumTraders.push_back(
            makeTrader(momentumStrategy.get(), TraderType::Momentum, 2000 * 25, 25L)
        );
    }

    for (int i = 0; i < fundamentalCount; ++i)
    {
        fundamentalTraders.push_back(
            makeTrader(fundamentalStrategy.get(), TraderType::Fundamental, 2000 * 25, 25L)
        );
    }

    for (int i = 0; i < noiseCount; ++i)
    {
        noiseTraders.push_back(
            makeTrader(noiseStrategy.get(), TraderType::Noise, 2000 * 15, 15L)
        );
    }

    for (int i = 0; i < contrarianCount; ++i)
    {
        contrarianTraders.push_back(
            makeTrader(contrarianStrategy.get(), TraderType::Contrarian, 2000 * 25, 25L)
        );
    }

    for (int i = 0; i < makerCount; ++i)
    {
        marketMakers.push_back(
            makeTrader(makerStrategy.get(), TraderType::Maker, 2000 * 35, 35L)
        );
    }

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }

                if (keyPressed->code == sf::Keyboard::Key::Space)
                {
                    pauseSim = !pauseSim;

                    std::cout << "\n=========== SIM DIAGNOSTICS ===========\n";

                    std::cout << "Total ticks: " << clock.now() << "\n";
                    std::cout << "Runtime (s): "
                        << static_cast<double>(clock.now()) / ticksPerSec << "\n";

                    auto printGroup = [](const std::string& name, const auto& traders, LimitOrderBook& LOB)
                        {
                            Quantity totalInv = 0;
                            PriceTicks totalPnl = 0;
                            int totalCount = 0;

                            Quantity maxInv = 0;
                            Quantity minInv = 0;
                            bool first = true;

                            PriceTicks mid = LOB.midPrice();

                            for (const auto& t : traders)
                            {
                                Quantity inv = t->getStocks() + t->getLockedStocks();
                                totalInv += inv;

                                const auto stats = t->getStats();

                                PriceTicks inventoryValue = 0;
                                if (mul_overflow_i64(mid, inv, inventoryValue))
                                {
                                    std::cout << "FATAL: overflow in equity calc\n";
                                    return;
                                }

                                PriceTicks equity = t->getFunds() + t->getLockedFunds();
                                equity += inventoryValue;

                                PriceTicks pnl = equity - stats.startEquity;
                                totalPnl += pnl;
                                totalCount++;

                                if (first)
                                {
                                    maxInv = inv;
                                    minInv = inv;
                                    first = false;
                                }
                                else
                                {
                                    if (inv > maxInv) maxInv = inv;
                                    if (inv < minInv) minInv = inv;
                                }
                            }

                            Quantity avgInv = 0;
                            PriceTicks avgPnl = 0;

                            if (totalCount > 0)
                            {
                                avgInv = totalInv / totalCount;
                                avgPnl = totalPnl / totalCount;
                            }

                            std::cout << "\n[" << name << "]\n";
                            std::cout << "Avg inventory: " << avgInv << "\n";
                            std::cout << "Min inventory: " << minInv << "\n";
                            std::cout << "Max inventory: " << maxInv << "\n";
                            std::cout << "Avg PnL: " << UIHelper::formatPrice(avgPnl) << "\n";
                        };

                    printGroup("Market Maker", marketMakers, LOB);
                    printGroup("Noise", noiseTraders, LOB);
                    printGroup("Momentum", momentumTraders, LOB);
                    printGroup("Contrarian", contrarianTraders, LOB);
                    printGroup("Fundamental", fundamentalTraders, LOB);

                    Quantity totalInventory = 0;

                    auto sumInv = [&](const auto& traders)
                        {
                            for (const auto& t : traders)
                                totalInventory += t->getStocks() + t->getLockedStocks();
                        };

                    sumInv(marketMakers);
                    sumInv(noiseTraders);
                    sumInv(momentumTraders);
                    sumInv(contrarianTraders);
                    sumInv(fundamentalTraders);

                    std::cout << "Inventory conservation: " << totalInventory << "\n";
                    std::cout << "=======================================\n";

                    auto const& midHistory = LOB.getMidPriceHistory();

                    if (!midHistory.empty())
                    {
                        PriceTicks minMid = midHistory.front();
                        PriceTicks maxMid = midHistory.front();

                        long long sumMid = 0;
                        int above2003 = 0;
                        int below1993 = 0;

                        for (PriceTicks p : midHistory)
                        {
                            if (p < minMid) minMid = p;
                            if (p > maxMid) maxMid = p;

                            sumMid += p;

                            if (p > 2003) above2003++;
                            if (p < 1993) below1993++;
                        }

                        PriceTicks avgMid = static_cast<PriceTicks>(sumMid / static_cast<long long>(midHistory.size()));
                        PriceTicks rangeMid = maxMid - minMid;
                        PriceTicks currentMid = midHistory.back();

                        std::cout << "\n[Price Diagnostics]\n";
                        std::cout << "Current mid: " << UIHelper::formatPrice(currentMid) << "\n";
                        std::cout << "Average mid: " << UIHelper::formatPrice(avgMid) << "\n";
                        std::cout << "Min mid: " << UIHelper::formatPrice(minMid) << "\n";
                        std::cout << "Max mid: " << UIHelper::formatPrice(maxMid) << "\n";
                        std::cout << "Range: " << UIHelper::formatPrice(rangeMid) << "\n";
                        std::cout << "Samples above 20.03: " << above2003 << "\n";
                        std::cout << "Samples below 19.93: " << below1993 << "\n";
                    }

                    lastTime = std::chrono::high_resolution_clock::now();
                }
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastTime;

        // Prevent spiral-of-death if rendering stalls
        constexpr int maxUpdatesPerFrame = 10;
        int updatesThisFrame = 0;

        while (!pauseSim && elapsed.count() >= realDt && updatesThisFrame < maxUpdatesPerFrame)
        {
            clock.advance(dt);

            lastTime += std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
                std::chrono::duration<double>(realDt)
            );
            elapsed = now - lastTime;

            LOB.update();

            for (Trader* t : momentumTraders)
            {
                if (t) t->update(LOB, clock);
            }
            
            for (Trader* t : fundamentalTraders)
            {
                if (t) t->update(LOB, clock);
            }
            
            for (Trader* t : noiseTraders)
            {
                if (t) t->update(LOB, clock);
            }

            for (Trader* t : contrarianTraders)
            {
                if (t) t->update(LOB, clock);
            }

            for (Trader* t : marketMakers)
            {
                if (t) t->update(LOB, clock);
            } 

            LOB.processOrders(clock);
            lobDirty = true;
            ++updatesThisFrame;
        }

        // If frame fell badly behind, resync instead of trying to catch up forever
        if (!pauseSim && updatesThisFrame == maxUpdatesPerFrame)
        {
            lastTime = now;
        }

        window.clear(Theme::Background);

        if (lobDirty)
        {
            lobPanel.update(LOB);
            depthChart.update(LOB);
            candleChart.update(LOB, window.getSize(), clock.now());
            tradersStatsPanel.update(LOB);
            lobDirty = false;
        }

        window.draw(lobPanel);
        window.draw(depthChart);
        window.draw(candleChart);
        window.draw(tradersStatsPanel);
        window.display();
    }

    return 0;
}