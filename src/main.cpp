#include "linde_buzo_gray.h"

int main()
{
	LindeBuzoGray lbg;
	lbg.Initialize();
	for(auto i = 0; i < 32; ++i)
		lbg.Run(i);
	return 0;
}