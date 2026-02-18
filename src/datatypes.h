#pragma once

#include <SFML/Graphics/Text.hpp>
#include <vector>
#include <string>

#include "UIHelpers.h"

enum Side
{
	BUY,
	SELL
};

struct Order
{
	long id;
	long traderId;
	double price;
	long volume;
	Side side;
	long long timeStamp;
};

struct TradeRecord
{
	long tradeId;
	double price;
	long volume;
	long buyerOrderId;
	long sellerOrderId;
	long long timeStamp;
};

struct DepthPoint {
	float price;
	long totalVolume;
};

struct Candle {
	double open, high, low, close;
	long startTime;
};

struct OrderEntry {
	long id;
	size_t index;
};

struct PriceLevel {
	std::string priceLabel;
	std::vector<OrderEntry> orderEntries;
	size_t nextToMatch = 0;
	long levelVolume = 0;
};