#pragma once

#include <map>
#include <list>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>

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