#include <array>
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
	N = 20000;
	W = 1024;

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
				Sites[ch][id].Position = glm::vec2((u + 0.5f) / S, (v + 0.5f) / S);
				++id;
			}
		}
	}

	// Set energy
	Density.Name = "image.png";
	Density.Load();

	// The current size is limited to 2^x
	// TODO: support arbitrary size
	assert(W == Density.Width);
	assert(W == Density.Height);

	Energy.fill(0.0f);
	for (auto v = 0; v < W; ++v)
	{
		for (auto u = 0; u < W; ++u)
		{
			const auto c = Density.GetColor(u, v);
			Energy[0] += c[0];
			Energy[1] += c[1];
			Energy[2] += c[2];
		}
	}

	Counts[0] = (int32_t)(Energy[0] / (W * W) * N);
	Counts[1] = (int32_t)(Energy[1] / (W * W) * N);
	Counts[2] = (int32_t)(Energy[2] / (W * W) * N);
}

float distance(const glm::vec2 &a, const glm::vec2 &b)
{
	const auto dx = 0.5f > std::abs(a.x - b.x) ? std::abs(a.x - b.x) : 1.0f - std::abs(a.x - b.x);
	const auto dy = 0.5f > std::abs(a.y - b.y) ? std::abs(a.y - b.y) : 1.0f - std::abs(a.y - b.y);
	return dx * dx + dy * dy;
}

void LindeBuzoGray::Relax(const std::vector<int32_t> &channels)
{
	std::vector<std::pair<int32_t, uint8_t>> NextIDs(W*W, std::make_pair(invalid, 0));
	std::vector<std::pair<int32_t, uint8_t>> SiteIDs(W*W, std::make_pair(invalid, 0));

	// Prepare sites
	for (const auto ch : channels)
	{
		for (size_t i = 0; i < Sites[ch].size(); ++i)
		{
			const auto u = (int)(W + Sites[ch][i].Position.x * W) % W;
			const auto v = (int)(W + Sites[ch][i].Position.y * W) % W;
			SiteIDs[v * W + u] = std::make_pair((int32_t)i, (uint8_t)ch);
		}
	}

	// Jump flooding algorithm (https://www.comp.nus.edu.sg/~tants/jfa.html)
	int dx[9] = { -1, +0, +1, -1, 0, +1, -1, +0, +1 };
	int dy[9] = { -1, -1, -1, +0, 0, +0, +1, +1, +1 };
	int pass = 0;
	for (int step = W / 2; step > 0; step = step >> 1, ++pass)
	{
		// Gather
		// TODO: parallelize using std::thread
		for (auto v = 0; v < W; ++v)
		{
			for (auto u = 0; u < W; ++u)
			{
				const auto center_id = v * W + u;
				const glm::vec2 position((u + 0.5f) / W, (v + 0.5f) / W);
				for (int n = 0; n < 9; ++n)
				{
					const auto nx = (u + step * dx[n] + W) % W;
					const auto ny = (v + step * dy[n] + W) % W;

					const auto neighbour_id = ny * W + nx;
					const auto neighbour = SiteIDs[neighbour_id];
					if (neighbour.first == invalid)
					{
						continue;
					}
					const auto self = NextIDs[center_id];
					if (self.first == invalid)
					{
						// The first one
						NextIDs[center_id] = neighbour;
					}
					else
					{
						// Choose the closest one
						if (distance(position, Sites[neighbour.second][neighbour.first].Position) < distance(position, Sites[self.second][self.first].Position))
						{
							NextIDs[center_id] = neighbour;
						}
					}
				}
			}
		}

		// not swap, copy
		std::copy(NextIDs.begin(), NextIDs.end(), SiteIDs.begin());
	}

	// Move centers
	for(const auto ch : channels)
	{
		std::vector<glm::vec2> New(Sites[ch].size(), glm::vec2(0));

		for (auto &cell : Sites[ch])
		{
			cell.Capacity = 0;
		}

		for (auto v = 0; v < W; ++v)
		{
			for (auto u = 0; u < W; ++u)
			{
				const auto id = SiteIDs[v*W + u];
				if (id.second == ch)
				{
					const glm::vec2 self((u + 0.5f) / W, (v + 0.5f) / W);
					const auto c = Sites[ch][id.first].Position;
					const auto l = std::abs(c.x - self.x);
					const auto m = std::abs(c.x - self.x + 1.0f);
					const auto n = std::abs(c.x - self.x - 1.0f);
					const auto o = std::abs(c.y - self.y);
					const auto p = std::abs(c.y - self.y + 1.0f);
					const auto q = std::abs(c.y - self.y - 1.0f);
					glm::vec2 d =
					{
						(l < m && l < n) ? self.x : ((m < n) ? self.x - 1 : self.x + 1),
						(o < p && o < q) ? self.y : ((p < q) ? self.y - 1 : self.y + 1)
					};
					const auto energy = Density.GetColor(d)[ch];
					const auto center = Density.GetColor(c)[ch];
					const auto sigma = (size_t)(N / 4) < Sites[ch].size() ? 0.2f : 0.25f;
					const auto weight = std::exp(-(energy - center) * (energy - center) / (2.0f * sigma * sigma));
					New[id.first] += d * energy * weight;
					Sites[ch][id.first].Capacity += energy * weight;
				}
			}
		}

		// Normalize
		for (size_t j = 0; j < Sites[ch].size(); ++j) if (0 < Sites[ch][j].Capacity)
		{
			auto &p = Sites[ch][j].Position;
			p = New[j] / Sites[ch][j].Capacity;
			if (0 > p.x) p.x += 1; if (0 > p.y) p.y += 1;
			if (1 < p.x) p.x -= 1; if (1 < p.y) p.y -= 1;
			assert(0 <= p.x &&p.x <= 1);
			assert(0 <= p.y &&p.y <= 1);
		}
	}
}

void LindeBuzoGray::Run(const uint32_t frame)
{
	Image out;
	std::stringstream ss;
	ss << "stippling" << std::setw(4) << std::setfill('0') << frame << ".png";
	out.Name = ss.str();
	std::cout << out.Name << std::endl;
	out.Create(W, W, 3, 8);

	std::array<glm::vec3, 3> colors;
	colors[0] = { 1,0,0 };
	colors[1] = { 0,1,0 };
	colors[2] = { 0,0,1 };

	for (auto ch = 0; ch < Channel; ++ch)
	{
		// Draw dots
		for (size_t j = 0; j < Sites[ch].size(); ++j)
		{
			out.DrawCircle(Sites[ch][j].Position, colors[ch], 2.5f);
		}

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
			{
				continue;
			}

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
		std::cout << Sites[ch].size() << " " << split.size() << std::endl;
		Sites[ch].swap(split);
	}

	// Multi class extension
#ifdef MULTI_CLASS
	std::vector<int32_t> channels = { 0,1,2 };
	Relax(channels);
#endif

	std::cout << "r " << Sites[0].size() / (float)N << "\t" << Energy[0] << std::endl;
	std::cout << "g " << Sites[1].size() / (float)N << "\t" << Energy[1] << std::endl;
	std::cout << "b " << Sites[2].size() / (float)N << "\t" << Energy[2] << std::endl;
	out.Save();
}
