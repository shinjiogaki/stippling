#include "linde_buzo_gray.h"

int main()
{
	LindeBuzoGray lbg;

	// Set image name
	lbg.Density.Name = "image.png";

	// Init
	lbg.Initialize();

	// Iterate
	for(auto i = 0; i < 32; ++i)
		lbg.Run(i);

	return 0;
}