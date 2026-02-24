#include "Trader.h"
#include "LimitOrderBook.h"

Trader::Trader(TradeStrategy* strategy, TraderId id, PriceTicks funds, Quantity stocks)
    : strategy(strategy),
    id(id),
    funds(funds),
    stocks(stocks)
{}

TraderId Trader::getId() const
{
	return id;
}

PriceTicks Trader::getFunds() const
{
	return funds;
}

Quantity Trader::getStocks() const
{
	return stocks;
}

const std::map<PriceTicks, std::vector<OrderId>>& Trader::getActiveOrderIds() const
{
	return ordersByPrice;
}

size_t Trader::getOrderCount() const 
{ 
    return idToPrice.size(); 
}

void Trader::changeFunds(PriceTicks funds)
{
	this->funds += funds;
}

void Trader::changeStocks(Quantity stocks)
{
	this->stocks += stocks;
}

void Trader::update(LimitOrderBook& LOB, Clock& clock)
{
	strategy->decide(*this, LOB, clock);
}

void Trader::addActiveOrderId(OrderId id, PriceTicks price) {
    idToPrice[id] = price;

    ordersByPrice[price].push_back(id);
}

void Trader::removeActiveOrderId(OrderId id) 
{
    auto itID = idToPrice.find(id);
    if (itID == idToPrice.end()) return;

    PriceTicks price = itID->second;

    auto itPrice = ordersByPrice.find(price);
    if (itPrice != ordersByPrice.end()) {
        std::erase(itPrice->second, id);

        if (itPrice->second.empty()) {
            ordersByPrice.erase(itPrice);
        }
    }

    idToPrice.erase(itID);
}

void Trader::onOrderFinished(OrderId id) 
{
	removeActiveOrderId(id);
}

void Trader::clearHalfOrders(LimitOrderBook& LOB) {
    if (idToPrice.empty()) return;

    int numToCancel = std::round(idToPrice.size() * 0.5f);

    for (int i = 0; i < numToCancel; i++) {
        auto it = idToPrice.begin();
        if (it == idToPrice.end()) break;

        OrderId id = it->first;

        LOB.cancelOrder(id);
    }
}