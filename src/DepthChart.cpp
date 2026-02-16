#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

#include "datatypes.h"
#include "LimitOrderBook.h"
#include "DepthChart.h"
#include "UIHelpers.h"

DepthChart::DepthChart(float binSize) {
    totalVolume = 0;
    this->binSize = binSize;
    bidTriangles.setPrimitiveType(sf::PrimitiveType::TriangleStrip);
    askTriangles.setPrimitiveType(sf::PrimitiveType::TriangleStrip);
}

void DepthChart::updateDepthPoints(const LimitOrderBook& LOB)
{
    depthPoints.clear();

    const auto& bids = LOB.getBids();
    const auto& asks = LOB.getAsks();

    if (bids.empty() || asks.empty()) {
        return;
    }

    depthPoints.reserve(bids.size() + asks.size() + 1);

    long runningBidVol = 0;
    std::vector<DepthPoint> tempBids;

    //Adding points in reverse then reversing to avoid adding to front
    for (auto it = bids.begin(); it != bids.end(); ++it) {
        long levelVol = 0;
        for (const auto& entry : it->second.orderEntries)
        {
            const Order* order = LOB.getOrder(entry.id);
            if (order == NULL)
                continue;

            levelVol += order->volume;
        }

        runningBidVol += levelVol;
        float priceLevel = std::floor(it->first / binSize) * binSize;

        tempBids.push_back({ priceLevel, runningBidVol });
    }

    for (auto it = tempBids.rbegin(); it != tempBids.rend(); ++it) {
        depthPoints.push_back(*it);
    }

    //Midpoint
    float midPrice = (bids.begin()->first + asks.begin()->first) / 2.0f;
    depthPoints.push_back({ std::floor(midPrice / binSize) * binSize, 0 });

    long runningAskVol = 0;
    for (auto it = asks.begin(); it != asks.end(); ++it) {
        long levelVol = 0;
        for (const auto& entry : it->second.orderEntries)
        {
            const Order* order = LOB.getOrder(entry.id);
            if (order == NULL)
                continue;

            levelVol += order->volume;
        }

        runningAskVol += levelVol;
        float priceLevel = std::floor(it->first / binSize) * binSize;

        depthPoints.push_back({ priceLevel, runningAskVol });
    }

    totalVolume = std::max(runningBidVol, runningAskVol);
}

void DepthChart::update(const LimitOrderBook& LOB, float chartWidth, float chartHeight, sf::Vector2u winSize) {
    updateDepthPoints(LOB);
    
    if (depthPoints.empty()) {
        bidTriangles.clear();
        askTriangles.clear();
        return;
    }

    int offset = 0;

    float binWidth = chartWidth / static_cast<float>(depthPoints.size() - 1);
    float bottomOfChart = (float)winSize.y;
    float startX = winSize.x - chartWidth + offset;

    size_t splitIndex = 0;
    for (size_t i = 0; i < depthPoints.size(); i++) {
        if (depthPoints[i].totalVolume == 0) {
            splitIndex = i;
            break;
        }
    }

    bidTriangles.resize((splitIndex + 1) * 2);
    size_t askCount = depthPoints.size() - splitIndex;
    askTriangles.resize(askCount * 2);

    for (size_t i = 0; i <= splitIndex; i++)
    {
        auto& curPoint = depthPoints[i];

        float hPerc = (totalVolume > 0) ? (float)curPoint.totalVolume / totalVolume : 0.f;
        float xPos = startX + (i * binWidth);
        float yPeak = bottomOfChart - (hPerc * chartHeight);

        bidTriangles[2 * i].position = { xPos, yPeak };
        bidTriangles[2 * i + 1].position = { xPos, bottomOfChart };

        sf::Color topCol = (curPoint.totalVolume == 0) ? Theme::TextDim : Theme::Bid;
        sf::Color botCol = (curPoint.totalVolume == 0) ? Theme::TextDim : Theme::BidBG;

        bidTriangles[2 * i].color = topCol;
        bidTriangles[2 * i + 1].color = botCol;
    }

    for (size_t i = 0; i < askCount; i++)
    {
        size_t dataIdx = splitIndex + i; //Offset to other side
        auto& curPoint = depthPoints[dataIdx];

        float hPerc = (totalVolume > 0) ? (float)curPoint.totalVolume / totalVolume : 0.f;
        float xPos = startX + (dataIdx * binWidth);
        float yPeak = bottomOfChart - (hPerc * chartHeight);

        askTriangles[2 * i].position = { xPos, yPeak };
        askTriangles[2 * i + 1].position = { xPos, bottomOfChart };

        sf::Color topCol = (curPoint.totalVolume == 0) ? Theme::TextDim : Theme::Ask;
        sf::Color botCol = (curPoint.totalVolume == 0) ? Theme::TextDim : Theme::AskBG;

        askTriangles[2 * i].color = topCol;
        askTriangles[2 * i + 1].color = botCol;
    }
}

void DepthChart::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    target.draw(bidTriangles, states);
    target.draw(askTriangles, states);
}