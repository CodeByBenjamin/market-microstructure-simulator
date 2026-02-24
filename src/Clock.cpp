#include "Clock.h"

Tick Clock::now() const
{
	return curTime;
}

void Clock::advance(Tick dt)
{
	curTime += dt;
}