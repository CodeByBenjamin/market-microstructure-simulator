#pragma once

#include <vector>
#include <string>

using TraderId = long;
using TradeId = long;
using OrderId = long;
using Quantity = long;
using PriceTicks = int64_t;
using Tick = int64_t;
	
enum Side
{
	BUY,
	SELL
};

struct Order
{
	OrderId id;
	TraderId traderId;
	PriceTicks price;
	Quantity volume;
	Side side;
	Tick timeStamp;
};

struct TradeRecord
{
	TradeId tradeId;
	PriceTicks price;
	Quantity volume;
	long buyerOrderId;
	long sellerOrderId;
	Tick timeStamp;
};

struct DepthPoint {
	PriceTicks price;
	Quantity totalVolume;
};

struct Candle {
	PriceTicks open, high, low, close;
	Tick startTime;
};

struct OrderEntry {
	OrderId id;
	size_t index;
};

struct PriceLevel {
	std::string priceLabel;
	std::vector<OrderEntry> orderEntries;
	size_t nextToMatch = 0;
	Quantity levelVolume = 0;
};

enum class RejectReason {
	None,
	NoTrader,
	InsufficientFunds,
	InsufficientStocks,
	Overflow
};

struct OrderResult {
	OrderId orderId; // 0 if rejected
	RejectReason reason; // None if accepted
};