﻿#include <array>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

#include "linde_buzo_gray.h"

const auto invalid = std::numeric_limits<uint32_t>::max();

void LindeBuzoGray::Initialize()
{
	// The current size is limited to 2^x
	// TODO: support arbitrary size
	Density.Load();
	assert(W == Density.Width);
	assert(W == Density.Height);

	std::cout << "loaded" << std::endl;
	N = 2000;
	W = 512;

	// Generate initial points
	for (auto ch = 0; ch < Channel; ++ch)
	{
		const auto S = 32;
		Sites[ch].resize(S * S);
		auto id = 0;
		for (auto v = 0; v < S; ++v)
		{
			for (auto u = 0; u < S; ++u)
			{
				Sites[ch][id].Position = glm::vec2((u + rand() / (float)RAND_MAX) / S, (v + rand() / (float)RAND_MAX) / S);
				++id;
			}
		}
	}

	// Set energy
	Energy.fill(0);
	for (auto v = 0; v < W; ++v)
	{
		for (auto u = 0; u < W; ++u)
		{
			const auto col = Density.GetColor(u, v);
			//Energy[0] += col[0];
			//Energy[1] += col[1];
			//Energy[2] += col[2];
			for (auto c = 0; c < Channel; ++c)
				Energy[c] += col[c];
		}
	}

	for (auto c = 0; c < Channel; ++c)
	{
		Counts[c] = (int32_t)(Energy[c] / (W * W) * N);
	}
}

float_t distance(const glm::vec2 &a, const glm::vec2 &b)
{
	const auto dx = 0.5f > std::abs(a.x - b.x) ? std::abs(a.x - b.x) : 1 - std::abs(a.x - b.x);
	const auto dy = 0.5f > std::abs(a.y - b.y) ? std::abs(a.y - b.y) : 1 - std::abs(a.y - b.y);
	return dx * dx + dy * dy;
}

void LindeBuzoGray::Relax(const std::vector<int32_t> &channels)
{
	std::vector<std::pair<uint32_t, uint8_t>> next_ids(W * W, std::make_pair(invalid, 0));
	std::vector<std::pair<uint32_t, uint8_t>> site_ids(W * W, std::make_pair(invalid, 0));

	// Prepare sites
	for (const auto ch : channels)
	{
		for (size_t i = 0; i < Sites[ch].size(); ++i)
		{
			const auto u = (int)(W + Sites[ch][i].Position.x * W) % W;
			const auto v = (int)(W + Sites[ch][i].Position.y * W) % W;
			site_ids[v * W + u] = std::make_pair((uint32_t)i, (uint8_t)ch);
		}
	}

	// Jump flooding algorithm (https://www.comp.nus.edu.sg/~tants/jfa.html)
	// 2^x size limitation is due to this algorithm
	static const int32_t dx[9] = { -1, +0, +1, -1, 0, +1, -1, +0, +1 };
	static const int32_t dy[9] = { -1, -1, -1, +0, 0, +0, +1, +1, +1 };
	int32_t pass = 0;
	for (auto step = W / 2; step > 0; step = step >> 1, ++pass)
	{
		// Gather
		// TODO: parallelize
		for (auto v = 0; v < W; ++v)
		{
			for (auto u = 0; u < W; ++u)
			{
				const auto center_id = v * W + u;
				const glm::vec2 position((u + 0.5f) / W, (v + 0.5f) / W);
				for (auto n = 0; n < 9; ++n)
				{
					const auto nx = (u + step * dx[n] + W) % W;
					const auto ny = (v + step * dy[n] + W) % W;

					const auto neighbour_id = ny * W + nx;
					const auto neighbour = site_ids[neighbour_id];
					if (neighbour.first == invalid)
						continue;

					const auto self = next_ids[center_id];

					// The first one
					if (self.first == invalid)
						next_ids[center_id] = neighbour;

					// Choose the closest one
					else if (distance(position, Sites[neighbour.second][neighbour.first].Position) < distance(position, Sites[self.second][self.first].Position))
						next_ids[center_id] = neighbour;
				}
			}
		}

		// not swap, copy
		std::copy(next_ids.begin(), next_ids.end(), site_ids.begin());
	}

	// Move centers
	for(const auto ch : channels)
	{
		std::vector<glm::vec2> new_positions(Sites[ch].size(), glm::vec2(0));

		for (auto &cell : Sites[ch])
			cell.Capacity = 0;

		for (auto v = 0; v < W; ++v)
		{
			for (auto u = 0; u < W; ++u)
			{
				const auto id = site_ids[v * W + u];
				if (id.second == ch)
				{
					const glm::vec2 self((u + 0.5f) / W, (v + 0.5f) / W);
					const auto c = Sites[ch][id.first].Position;
					const auto l = std::abs(c.x - self.x);
					const auto m = std::abs(c.x - self.x + 1);
					const auto n = std::abs(c.x - self.x - 1);
					const auto o = std::abs(c.y - self.y);
					const auto p = std::abs(c.y - self.y + 1);
					const auto q = std::abs(c.y - self.y - 1);
					glm::vec2 d =
					{
						(l < m && l < n) ? self.x : ((m < n) ? self.x - 1 : self.x + 1),
						(o < p && o < q) ? self.y : ((p < q) ? self.y - 1 : self.y + 1)
					};
					const auto energy = Density.GetColor(d)[ch];
					const auto center = Density.GetColor(c)[ch];
					new_positions[id.first]          += d * energy;
					Sites    [ch][id.first].Capacity +=     energy;
				}
			}
		}

		// Normalize
		for (size_t j = 0; j < Sites[ch].size(); ++j) if (0 < Sites[ch][j].Capacity)
		{
			auto &p = Sites[ch][j].Position;
			p = new_positions[j] / Sites[ch][j].Capacity;
			if (0 > p.x) p.x += 1; if (0 > p.y) p.y += 1;
			if (1 < p.x) p.x -= 1; if (1 < p.y) p.y -= 1;
			assert(0 <= p.x && p.x <= 1);
			assert(0 <= p.y && p.y <= 1);
		}
	}
}

#define MULTI_CLASS
void LindeBuzoGray::Run(const uint32_t frame)
{
	std::stringstream ss;
	ss << "stippling" << std::setw(4) << std::setfill('0') << frame << ".png";
	std::cout << ss.str() << std::endl;

	Image out;
	out.Name = ss.str();
	out.Create(W, W, 3, 8);

	std::array<glm::vec3, Channel> colors;
	colors[0] = { 1, 0, 0 };
	colors[1] = { 0, 1, 0 };
	colors[2] = { 0, 0, 1 };
	colors[3] = { 1, 1, 0 };
	colors[4] = { 0, 1, 1 };
	colors[5] = { 1, 0, 1 };

	for (auto ch = 0; ch < Channel; ++ch)
	{
		// Draw dots
		for (size_t j = 0; j < Sites[ch].size(); ++j)
			out.DrawCircle(Sites[ch][j].Position, colors[ch], 2.0f);

		// Relaxation
		std::vector<int32_t> channels = { ch };
		Relax(channels);

		std::vector<Site> split;
		for (size_t j = 0; j < Sites[ch].size(); ++j)
		{
			auto c = Sites[ch][j];

			// TODO: use proper thresholds
			
			// Remove
			if (0.5f * (Energy[ch] / Counts[ch]) > c.Capacity)
				continue;

			// Split
			if (1.5f * (Energy[ch] / Counts[ch]) < c.Capacity)
			{
				auto cell0 = c;
				auto cell1 = c;

				// TODO: compute proper points e.g. using eigen value decomp
				cell0.Position.x += 0.005f * (rand() / (float_t)RAND_MAX - 0.5f);
				cell0.Position.y += 0.005f * (rand() / (float_t)RAND_MAX - 0.5f);
				cell1.Position.x += 0.005f * (rand() / (float_t)RAND_MAX - 0.5f);
				cell1.Position.y += 0.005f * (rand() / (float_t)RAND_MAX - 0.5f);

				split.push_back(cell0);
				split.push_back(cell1);
				continue;
			}

			// Keep
			split.push_back(c);
		}
		Sites[ch].swap(split);
	}

	// Multi class extension
#ifdef MULTI_CLASS
	std::vector<int32_t> channels = { 0, 1, 2, 3, 4, 5 };
	Relax(channels);
#endif

	out.Save();
}
