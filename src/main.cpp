#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <optional>
#include <algorithm>
#include <format>

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
#include "LobPanel.h"
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
#include "rng.h"
#include "SimulationConfig.h"

void printSimulationSummary(
    const LimitOrderBook& lob,
    const Clock& clock,
    uint32_t seed,
    Quantity startingInventory,
    const std::vector<Trader*>& marketMakers,
    const std::vector<Trader*>& momentumTraders,
    const std::vector<Trader*>& contrarianTraders,
    const std::vector<Trader*>& fundamentalTraders,
    const std::vector<Trader*>& noiseTraders
);

uint32_t makeSimulationSeed();

void addTraders(
    std::vector<Trader*>& target,
    std::vector<std::unique_ptr<Trader>>& storage,
    LimitOrderBook& lob,
    TradeStrategy* strategy,
    TraderType type,
    int traderCount,
    PriceTicks funds,
    Quantity inventory,
    TraderId& nextTraderId
);

PriceTicks tradePriceSum = 0;
Quantity tradeCount = 0;

int main()
{
    // Window and UI setup
    auto window = sf::RenderWindow(
        sf::VideoMode({ Config::Display::windowWidth, Config::Display::windowHeight }),
        "Market simulator",
        sf::State::Fullscreen
    );
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if (!font.openFromFile("../../../fonts/RobotoMono-Regular.ttf"))
    {
        std::cerr << "Error loading font!" << std::endl;
        return 1;
    }

    // RNG initialization
    const uint32_t seed = makeSimulationSeed();

    rng.seed(seed);

    std::cout << "\n===== SIMULATION START =====\n";
    std::cout << "Seed: " << seed << "\n";
    std::cout << "RNG: Mersenne Twister (mt19937)\n";
    std::cout << "============================\n";

    // Simulation state
    Clock clock;
    constexpr double realDt = 1.0 / Config::Simulation::ticksPerSecond;

    auto lastTime = std::chrono::high_resolution_clock::now();

    LimitOrderBook lob;

    LobPanel lobPanel(window.getSize(), font);
    DepthChart depthChart(window.getSize(), Config::Display::depthLevels);
    Tick binSize = static_cast<Tick>(
        Config::Simulation::ticksPerSecond * Config::Display::candleBinMultiplier
        );
    CandleChart candleChart(font, binSize, Config::Display::visibleCandles);
    TradersStatsPanel tradersStatsPanel(window.getSize(), font);

    bool lobDirty = true;
    bool pauseSim = false;

    // Strategy and trader 
    auto makerStrategy = std::make_unique<MarketMaker>();
    auto momentumStrategy = std::make_unique<MomentumStrategy>();
    auto contrarianStrategy = std::make_unique<ContrarianStrategy>();
    auto fundamentalStrategy = std::make_unique<FundamentalStrategy>();
    auto noiseStrategy = std::make_unique<NoiseStrategy>();

    std::vector<std::unique_ptr<Trader>> traderStorage;
    traderStorage.reserve(Config::Population::totalTraders);

    std::vector<Trader*> marketMakers;
    std::vector<Trader*> momentumTraders;
    std::vector<Trader*> contrarianTraders;
    std::vector<Trader*> fundamentalTraders;
    std::vector<Trader*> noiseTraders;

    marketMakers.reserve(Config::Population::marketMakers);
    momentumTraders.reserve(Config::Population::momentumTraders);
    contrarianTraders.reserve(Config::Population::contrarianTraders);
    fundamentalTraders.reserve(Config::Population::fundamentalTraders);
    noiseTraders.reserve(Config::Population::noiseTraders);

    TraderId nextTraderId = 1;

    addTraders(
        marketMakers,
        traderStorage,
        lob,
        makerStrategy.get(),
        Maker,
        Config::Population::marketMakers,
        Config::InitialState::marketMakerFunds,
        Config::InitialState::marketMakerInventory,
        nextTraderId
    );

    addTraders(
        momentumTraders,
        traderStorage,
        lob,
        momentumStrategy.get(),
        Momentum,
        Config::Population::momentumTraders,
        Config::InitialState::momentumFunds,
        Config::InitialState::momentumInventory,
        nextTraderId
    );

    addTraders(
        contrarianTraders,
        traderStorage,
        lob,
        contrarianStrategy.get(),
        Contrarian,
        Config::Population::contrarianTraders,
        Config::InitialState::contrarianFunds,
        Config::InitialState::contrarianInventory,
        nextTraderId
    );

    addTraders(
        fundamentalTraders,
        traderStorage,
        lob,
        fundamentalStrategy.get(),
        Fundamental,
        Config::Population::fundamentalTraders,
        Config::InitialState::fundamentalFunds,
        Config::InitialState::fundamentalInventory,
        nextTraderId
    );

    addTraders(
        noiseTraders,
        traderStorage,
        lob,
        noiseStrategy.get(),
        Noise,
        Config::Population::noiseTraders,
        Config::InitialState::noiseFunds,
        Config::InitialState::noiseInventory,
        nextTraderId
    );

    // Main event/render loop
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

                    printSimulationSummary(
                        lob,
                        clock,
                        seed,
                        Config::InitialState::totalStartingInventory,
                        marketMakers,
                        momentumTraders,
                        contrarianTraders,
                        fundamentalTraders,
                        noiseTraders
                    );

                    lastTime = std::chrono::high_resolution_clock::now();
                }
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastTime;

        int updatesThisFrame = 0;

        while (!pauseSim &&
            elapsed.count() >= realDt &&
            updatesThisFrame < Config::Simulation::maxUpdatesPerFrame)
        {
            clock.advance(Config::Simulation::tickDelta);

            lastTime += std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
                std::chrono::duration<double>(realDt)
            );
            elapsed = now - lastTime;

            lob.update();

            for (Trader* trader : marketMakers)
            {
                trader->update(lob, clock);
            }

            for (Trader* trader : momentumTraders)
            {
                trader->update(lob, clock);
            }

            for (Trader* trader : contrarianTraders)
            {
                trader->update(lob, clock);
            }

            for (Trader* trader : fundamentalTraders)
            {
                trader->update(lob, clock);
            }

            for (Trader* trader : noiseTraders)
            {
                trader->update(lob, clock);
            }

            lob.processOrders(clock);
            lobDirty = true;
            ++updatesThisFrame;
        }

        if (!pauseSim && updatesThisFrame == Config::Simulation::maxUpdatesPerFrame)
        {
            lastTime = now;
        }

        window.clear(Theme::Background);

        if (lobDirty)
        {
            lobPanel.update(lob);
            depthChart.update(lob);
            candleChart.update(lob, window.getSize(), clock.now());
            tradersStatsPanel.update(lob);
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

void printSimulationSummary(
    const LimitOrderBook& lob,
    const Clock& clock,
    uint32_t seed,
    Quantity startingInventory,
    const std::vector<Trader*>& marketMakers,
    const std::vector<Trader*>& momentumTraders,
    const std::vector<Trader*>& contrarianTraders,
    const std::vector<Trader*>& fundamentalTraders,
    const std::vector<Trader*>& noiseTraders
)
{
    std::cout << "\n========== SIMULATION SUMMARY ==========\n";

    std::cout << "Seed: " << seed << "\n\n";
    std::cout << "Ticks: " << clock.now() << "\n";

    const double seconds =
        static_cast<double>(clock.now()) / Config::Simulation::ticksPerSecond;

    std::cout << "Simulated time (seconds): "
        << std::format("{:.2f}", seconds) << "\n";
    std::cout << "Ticks per second: "
        << Config::Simulation::ticksPerSecond << "\n";

    std::cout << "\nTotal traders: " << Config::Population::totalTraders << "\n";

    double tradesPerSecond = 0.0;
    if (seconds > 0.0)
        tradesPerSecond = static_cast<double>(tradeCount) / seconds;

    PriceTicks averageTradePrice = 0;
    if (tradeCount > 0)
        averageTradePrice = tradePriceSum / tradeCount;

    std::cout << "\n[Market]\n";
    std::cout << "Trades: " << tradeCount << "\n";
    std::cout << "Trades per second: " << std::format("{:.2f}", tradesPerSecond) << "\n";
    std::cout << "Average trade price: " << UIHelper::formatPrice(averageTradePrice) << "\n";

    const auto& midHistory = lob.getMidPriceHistory();

    if (!midHistory.empty())
    {
        PriceTicks minMid = midHistory.front();
        PriceTicks maxMid = midHistory.front();
        PriceTicks sumMid = 0;

        for (PriceTicks p : midHistory)
        {
            if (p < minMid) minMid = p;
            if (p > maxMid) maxMid = p;
            sumMid += p;
        }

        const PriceTicks avgMid =
            static_cast<PriceTicks>(sumMid / static_cast<long long>(midHistory.size()));
        const PriceTicks rangeMid = maxMid - minMid;
        const PriceTicks currentMid = midHistory.back();

        std::cout << "Current midprice: " << UIHelper::formatPrice(currentMid) << "\n";
        std::cout << "Average midprice: " << UIHelper::formatPrice(avgMid) << "\n";
        std::cout << "Min midprice: " << UIHelper::formatPrice(minMid) << "\n";
        std::cout << "Max midprice: " << UIHelper::formatPrice(maxMid) << "\n";
        std::cout << "Midprice range: " << UIHelper::formatPrice(rangeMid) << "\n";
    }

    auto printGroup = [](const std::string& name,
        const auto& traders,
        const LimitOrderBook& lob)
        {
            Quantity totalInv = 0;
            PriceTicks totalPnl = 0;
            int totalCount = 0;

            Quantity maxInv = 0;
            Quantity minInv = 0;
            bool first = true;

            const PriceTicks mid = lob.midPrice();

            for (const auto& t : traders)
            {
                const Quantity inv = t->getStocks() + t->getLockedStocks();
                totalInv += inv;

                const auto& stats = t->getStats();

                PriceTicks inventoryValue = 0;
                if (mul_overflow_i64(mid, inv, inventoryValue))
                {
                    std::cerr << "Simulation summary error: overflow computing equity\n";
                    return;
                }

                PriceTicks equity = t->getFunds() + t->getLockedFunds();
                equity += inventoryValue;

                const PriceTicks pnl = equity - stats.startEquity;
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
            std::cout << "Average PnL: " << UIHelper::formatPrice(avgPnl) << "\n";
            std::cout << "Average inventory: " << avgInv << "\n";
            std::cout << "Min inventory: " << minInv << "\n";
            std::cout << "Max inventory: " << maxInv << "\n";
        };

    printGroup("Market Makers", marketMakers, lob);
    printGroup("Momentum Traders", momentumTraders, lob);
    printGroup("Contrarian Traders", contrarianTraders, lob);
    printGroup("Fundamental Traders", fundamentalTraders, lob);
    printGroup("Noise Traders", noiseTraders, lob);

    Quantity totalInventory = 0;

    auto sumInv = [&](const auto& traders)
        {
            for (const auto& t : traders)
                totalInventory += t->getStocks() + t->getLockedStocks();
        };

    sumInv(marketMakers);
    sumInv(momentumTraders);
    sumInv(contrarianTraders);
    sumInv(fundamentalTraders);
    sumInv(noiseTraders);

    std::cout << "\n[Consistency Checks]\n";
    if (totalInventory == startingInventory)
        std::cout << "Inventory conservation: "
        << totalInventory << " / " << startingInventory << " (PASS)\n";
    else
        std::cout << "Inventory conservation: "
        << totalInventory << " / " << startingInventory << " (FAIL)\n";

    std::cout << "========================================\n";
}

uint32_t makeSimulationSeed()
{
    if (Config::Simulation::deterministicSeed)
        return Config::Simulation::fixedSeed;

    return static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
        );
}

void addTraders(
    std::vector<Trader*>& target,
    std::vector<std::unique_ptr<Trader>>& storage,
    LimitOrderBook& lob,
    TradeStrategy* strategy,
    TraderType type,
    int traderCount,
    PriceTicks funds,
    Quantity inventory,
    TraderId& nextTraderId
)
{
    for (int i = 0; i < traderCount; ++i)
    {
        auto trader = std::make_unique<Trader>(
            strategy,
            type,
            nextTraderId++,
            funds,
            inventory
        );

        Trader* ptr = trader.get();

        storage.push_back(std::move(trader));
        lob.registerTrader(ptr);
        target.push_back(ptr);
    }
}