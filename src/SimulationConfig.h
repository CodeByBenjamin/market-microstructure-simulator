#pragma once

#include "datatypes.h"

namespace Config
{
    namespace Simulation
    {
        constexpr bool deterministicSeed = false;
        constexpr uint32_t fixedSeed = 42;

        constexpr double tickDelta = 1.0;
        constexpr Tick ticksPerSecond = 15;
        constexpr int maxUpdatesPerFrame = 10;
    }

    namespace Display
    {
        constexpr unsigned int windowWidth = 1920;
        constexpr unsigned int windowHeight = 1080;

        constexpr int depthLevels = 50;
        constexpr int visibleCandles = 60;
        constexpr Tick candleBinMultiplier = 2;
    }

    namespace Population
    {
        constexpr int marketMakers = 12;
        constexpr int momentumTraders = 10;
        constexpr int contrarianTraders = 10;
        constexpr int fundamentalTraders = 12;
        constexpr int noiseTraders = 20;

        constexpr int totalTraders =
            marketMakers +
            momentumTraders +
            contrarianTraders +
            fundamentalTraders +
            noiseTraders;
    }

    namespace InitialState
    {
        constexpr Quantity marketMakerInventory = 35;
        constexpr Quantity momentumInventory = 25;
        constexpr Quantity contrarianInventory = 25;
        constexpr Quantity fundamentalInventory = 25;
        constexpr Quantity noiseInventory = 15;

        constexpr PriceTicks marketMakerFunds = 2000 * marketMakerInventory;
        constexpr PriceTicks momentumFunds = 2000 * momentumInventory;
        constexpr PriceTicks contrarianFunds = 2000 * contrarianInventory;
        constexpr PriceTicks fundamentalFunds = 2000 * fundamentalInventory;
        constexpr PriceTicks noiseFunds = 2000 * noiseInventory;

        constexpr Quantity totalStartingInventory =
            Population::marketMakers * marketMakerInventory +
            Population::momentumTraders * momentumInventory +
            Population::contrarianTraders * contrarianInventory +
            Population::fundamentalTraders * fundamentalInventory +
            Population::noiseTraders * noiseInventory;
    }
}