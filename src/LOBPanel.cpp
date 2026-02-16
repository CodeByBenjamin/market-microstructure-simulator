#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <string>
#include <algorithm>

#include "datatypes.h"
#include "LOBPanel.h"
#include "UIHelpers.h"
#include "LimitOrderBook.h"

void LOBPanel::draw(sf::RenderWindow& window, const sf::Font& font, const LimitOrderBook& LOB, float lobWidth)
{
	//Draw LOBPanel

	float winHeight = static_cast<float>(window.getSize().y);
	UIHelper::drawColoredRect(window, 0.f, 0.f, lobWidth, winHeight, UISnap::Left, 0.f, Theme::Surface);

	//Draw Label Text

	UIHelper::drawLabel(window, font, "BIDS ($)", 28, lobWidth / 4.f, 20.f, UISnap::Center, 0.f, Theme::Bid);
	UIHelper::drawLabel(window, font, "ASKS ($)", 28, 3.f * lobWidth / 4.f, 20.f, UISnap::Center, 0.f, Theme::Ask);

	//Draw LOBPanel

	const auto& bids = LOB.getBids();
	const auto& asks = LOB.getAsks();

	float rowHeight = 30.f;
	float padding = 5.f;
	float currentY = 130.f;

	if (!bids.empty() && !asks.empty())
	{
		double bestBid = bids.begin()->first;
		double bestAsk = asks.begin()->first;
		double mid = (bestBid + bestAsk) / 2.0;
		double spread = bestAsk - bestBid;

		float centerX = lobWidth / 2.f;

		std::string fullPrice = UIHelper::formatPrice(mid);
		size_t dotPos = fullPrice.find('.');

		std::string leftSide = fullPrice.substr(0, dotPos);
		std::string rightSide = fullPrice.substr(dotPos);
		UIHelper::drawLabel(window, font, leftSide, 30, centerX, 3.f * currentY / 4.f, UISnap::Right, -9.f, Theme::TextDim);
		UIHelper::drawLabel(window, font, rightSide, 30, centerX, 3.f * currentY / 4.f, UISnap::Left, -9.f, Theme::TextDim);

		fullPrice = UIHelper::formatPrice(spread);
		dotPos = fullPrice.find('.');

		leftSide = fullPrice.substr(0, dotPos);
		rightSide = fullPrice.substr(dotPos);
		UIHelper::drawLabel(window, font, leftSide, 22, centerX, currentY / 2.f, UISnap::Right, -7.f, Theme::Accent);
		UIHelper::drawLabel(window, font, rightSide, 22, centerX, currentY / 2.f, UISnap::Left, -7.f, Theme::Accent);

		int count = 0;
		int maxCount = 25; //Price levels to show
		long maxVol = std::max(LOB.getHighestVolume(BUY, maxCount), LOB.getHighestVolume(SELL, maxCount));

		if (maxVol == 0)
			return;

		count = 0;
		for (auto it = bids.begin(); it != bids.end() && count < maxCount; ++it) {
			if (it->second.orderEntries.empty())
				continue;

			long onePriceVol = 0;
			for (const auto& entry : it->second.orderEntries)
			{
				const Order* order = LOB.getOrder(entry.id);
				if (order == NULL)
					continue;

				onePriceVol += order->volume;
			}

			float fullPerc = static_cast<float>(onePriceVol) / maxVol;
			float yPos = currentY + (rowHeight + padding) * count;
			float centerX = lobWidth / 2.f;

			UIHelper::drawColoredRect(window, centerX, yPos, centerX * fullPerc, rowHeight, UISnap::Right, 0.f, Theme::BidBG);

			UIHelper::drawLabel(window, font, UIHelper::formatPrice(it->first), 24, centerX, yPos, UISnap::Right, -10.f, Theme::Bid);
			UIHelper::drawLabel(window, font, std::to_string(onePriceVol), 22, 0.f, yPos, UISnap::Left, 10.f, Theme::TextDim);

			++count;
		}

		count = 0;
		for (auto it = asks.begin(); it != asks.end() && count < maxCount; ++it) {
			if (it->second.orderEntries.empty())
				continue;

			long onePriceVol = 0;
			for (const auto& entry : it->second.orderEntries)
			{
				const Order* order = LOB.getOrder(entry.id);
				if (order == NULL)
					continue;

				onePriceVol += order->volume;
			}

			float fullPerc = static_cast<float>(onePriceVol) / maxVol;
			float yPos = currentY + (rowHeight + padding) * count;
			float centerX = lobWidth / 2.f;

			UIHelper::drawColoredRect(window, centerX, yPos, centerX * fullPerc, rowHeight, UISnap::Left, 0.f, Theme::AskBG);

			UIHelper::drawLabel(window, font, UIHelper::formatPrice(it->first), 24, centerX, yPos, UISnap::Left, 10.f, Theme::Ask);
			UIHelper::drawLabel(window, font, std::to_string(onePriceVol), 22, lobWidth, yPos, UISnap::Right, -10.f, Theme::TextDim);

			++count;
		}
	}

	UIHelper::drawColoredRect(window, lobWidth, 0.f, 2.f, window.getSize().y, UISnap::Left, 0.f, Theme::Border);
	UIHelper::drawColoredRect(window, lobWidth / 2.f, window.getSize().y / 2.f + currentY, 2.f, window.getSize().y, UISnap::Center, 0.f, Theme::Border);
}