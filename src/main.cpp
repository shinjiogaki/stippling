#include "linde_buzo_gray.h"
#include <iostream>
int main()
{
	LindeBuzoGray lbg;

	// Set image name
	lbg.Density.Name = "density.png";

	// Init
	lbg.Initialize();

	// Iterate
	for (auto i = 0; i < 32; ++i)
	{
		lbg.Run(i);
		std::cout<< i << std::endl;
	}

	return 0;
}