#include "Trader.h"
#include "LimitOrderBook.h"

Trader::Trader(TradeStrategy* strategy, long id, double funds, long stocks)
	: strategy(strategy),
	id(id),
	funds(funds),
	stocks(stocks)
{}

long Trader::getId() const
{
	return id;
}

double Trader::getFunds() const
{
	return funds;
}

double Trader::getStocks() const
{
	return stocks;
}

const std::map<double, std::vector<long>>& Trader::getActiveOrderIds() const
{
	return ordersByPrice;
}

size_t Trader::getOrderCount() const 
{ 
    return idToPrice.size(); 
}

void Trader::changeFunds(double funds)
{
	this->funds += funds;
}

void Trader::changeStocks(long stocks)
{
	this->stocks += stocks;
}

void Trader::update(LimitOrderBook& LOB, Clock& clock)
{
	strategy->decide(*this, LOB, clock);
}

void Trader::addActiveOrderId(long id, double price) {
    idToPrice[id] = price;

    ordersByPrice[price].push_back(id);
}

void Trader::removeActiveOrderId(long id) 
{
    auto itID = idToPrice.find(id);
    if (itID == idToPrice.end()) return;

    double price = itID->second;

    auto itPrice = ordersByPrice.find(price);
    if (itPrice != ordersByPrice.end()) {
        std::erase(itPrice->second, id);

        if (itPrice->second.empty()) {
            ordersByPrice.erase(itPrice);
        }
    }

    idToPrice.erase(itID);
}

void Trader::onOrderFinished(long id) 
{
	removeActiveOrderId(id);
}

void Trader::clearHalfOrders(LimitOrderBook& LOB) {
    if (idToPrice.empty()) return;

    int numToCancel = std::round(idToPrice.size() * 0.5f);

    for (int i = 0; i < numToCancel; i++) {
        auto it = idToPrice.begin();
        if (it == idToPrice.end()) break;

        long id = it->first;

        LOB.cancelOrder(id);
        this->removeActiveOrderId(id);
    }
}