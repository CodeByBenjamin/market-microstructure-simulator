#pragma once

#include "datatypes.h"

class Clock
{
private:
	Tick curTime = 0;
public:
	Tick now() const;

	void advance(Tick dt);
};