// written by shinji ogaki

#pragma once
#include <array>
#include <vector>

#include "..\glm\glm\glm.hpp"

#include "image.h"

// Voronoi Cell
struct Site
{
	glm::vec2 Position;
	float_t   Capacity;
};

// Weighted Linde-Buzo-Gray Stippling
struct LindeBuzoGray
{
	static const auto Channel = 6;

	int32_t N; // Target Number of Sites
	int32_t W; // Image Size
	std::vector<Site> Sites[Channel];
	void Initialize();
	void Relax(const std::vector<int32_t> &channels);
	void Run(const uint32_t frame);

	Image Density;
	std::array<float, Channel> Energy; // Multi Class
	std::array<int32_t, Channel> Counts; // Multi Class
};
