#include <iostream>
#include <vector>
#include <chrono>
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
#include "TrendStrategy.h"
#include "MarketMaker.h"
#include "CandleChart.h"
#include "priceutils.h"
#include "TradersStatsPanel.h"

int main()
{
    auto window = sf::RenderWindow(sf::VideoMode({1920u, 1080u}), "Market simulator", sf::State::Fullscreen);
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if (!font.openFromFile("fonts/RobotoMono-Regular.ttf"))
    {
        std::cout << "Error loading font!" << std::endl;
    }

    Clock clock;

    double dt = 1.0; //Ticks per update

    Tick ticksPerSec = 15;
    double realDt = 1.0 / ticksPerSec;
    auto lastTime = std::chrono::high_resolution_clock::now();

    LimitOrderBook LOB;

    LOBPanel lobPanel(window.getSize(), font);
    DepthChart depthChart(window.getSize(), 50);
    Tick binSize = ticksPerSec * 0.5;
    int candlesVisible = 60;
    CandleChart candleChart(font, binSize, candlesVisible);
    TradersStatsPanel tradersStatsPanel(window.getSize(), font);

    bool lobDirty = true;

    //Traders
    TrendStrategy* trendStrat = new TrendStrategy();
    std::vector<Trader*> trendTraders;
    trendTraders.reserve(5);
    for (int i = 0; i < 5; i++) {
        trendTraders.push_back(new Trader(trendStrat, TraderType::Trend, i, toPriceTicks(2000), 40L));
    }
    for (auto& t : trendTraders) LOB.registerTrader(t);

    MarketMaker* maker = new MarketMaker();
    std::vector<Trader*> marketMakers;
    marketMakers.reserve(15);
    for (int i = 0; i < 15; i++) {
        marketMakers.push_back(new Trader(maker, TraderType::Maker, i + 5, toPriceTicks(2000), 100L));
    }
    for (auto& t : marketMakers) LOB.registerTrader(t);

    bool pauseSim = false;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Space) pauseSim = !pauseSim;
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastTime;

        while (elapsed.count() >= realDt && !pauseSim)
        {
            clock.advance(dt);
            lastTime += std::chrono::duration_cast<
                std::chrono::high_resolution_clock::duration>(
                    std::chrono::duration<double>(realDt)
                );
            elapsed = now - lastTime;
        
            LOB.update();

            for (size_t i = 0; i < 5; i++)
            {
                trendTraders[i]->update(LOB, clock);
            }
            
            for (size_t i = 0; i < 15; i++)
            {
                marketMakers[i]->update(LOB, clock);
            }

            LOB.processOrders(clock);   

            lobDirty = true;
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
}