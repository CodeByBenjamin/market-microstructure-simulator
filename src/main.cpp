#include <iostream>
#include <vector>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <SFML/Window/Event.hpp>

#include "Clock.h"
#include "datatypes.h"
#include "UIHelpers.h"
#include "LimitOrderBook.h"
#include "LOBPanel.h"
#include "DepthChart.h"
#include "Trader.h"
#include "TrendStrategy.h"
#include "RandomStrategy.h"
#include "CandleChart.h"

int main()
{
    auto window = sf::RenderWindow(sf::VideoMode({1920u, 1080u}), "Market simulator", sf::State::Fullscreen);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("fonts/RobotoMono-Regular.ttf"))
    {
        std::cout << "Error loading font!" << std::endl;
    }

    Clock clock;

    double dt = 1.0; //Ticks per update

    double ticksPerSec = 10.0;
    double realDt = 1.0 / ticksPerSec;
    auto lastTime = std::chrono::high_resolution_clock::now();

    LOBPanel lobPanel;

    LimitOrderBook LOB;
    DepthChart depthChart(0.5);
    long binSize = ticksPerSec * 0.1;
    int candlesVisible = 100;
    CandleChart candleChart(binSize, candlesVisible);

    float lobWidth = static_cast<float>(window.getSize().x * 0.25f);
    float depthChartWidth = static_cast<float>(window.getSize().x * 0.25f);
    float depthChartHeight = static_cast<float>(window.getSize().y * 0.25f);

    bool lobDirty = true;

    TrendStrategy* trendStrat = new TrendStrategy();
    std::vector<Trader> trendTraders;
    trendTraders.reserve(10);
    for (int i = 0; i < 5; i++) {
        trendTraders.emplace_back(trendStrat, i, 2000.0, 100L);
    }
    for (auto& t : trendTraders) LOB.registerTrader(&t);

    RandomStrategy* randomStrat = new RandomStrategy();
    std::vector<Trader> randomTraders;
    randomTraders.reserve(20);
    for (int i = 0; i < 10; i++) {
        randomTraders.emplace_back(randomStrat, i + 5, 2000.0, 100L);
    }
    for (auto& t : randomTraders) LOB.registerTrader(&t);

    LOB.registerTrader(new Trader{randomStrat, 999, 100000.0, 20000L});

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
            if (clock.now() == 30) {
                Order whalePanic = { 999, 999, 10.0, 2000, Side::SELL, clock.now() };
                LOB.processOrder(whalePanic, clock);
                }

        
            for (size_t i = 0; i < 10; i++)
            {
                randomTraders[i].update(LOB, clock);
            }
            
            for (size_t i = 0; i < 5; i++)
            {
                trendTraders[i].update(LOB, clock);
            }
        
            lobDirty = true;
        }

        window.clear(Theme::Background);

        if (lobDirty)
        {
            depthChart.update(LOB, depthChartWidth, depthChartHeight, window.getSize());
            candleChart.update(LOB, clock.now());
            lobDirty = false;
        }

        lobPanel.draw(window, font, LOB, lobWidth);
        window.draw(depthChart);
        window.draw(candleChart);
        window.display();
    }
}