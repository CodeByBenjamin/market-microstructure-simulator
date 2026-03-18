#include <string>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <cmath>
#include <format>

#include "UIHelpers.h"
#include "priceutils.h"

std::string UIHelper::formatPrice(PriceTicks priceTicks)
{
    return std::format("{:.2f}", toPrice(priceTicks));
}