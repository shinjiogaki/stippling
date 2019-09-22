// written by shinji ogaki

#pragma once
#include <string>
#include <valarray>

#include "..\glm\glm\glm.hpp"

// Minimum image class
struct Image
{
	int32_t BitsPerSample;
	int32_t Channel;
	int32_t Width;
	int32_t Height;

	// Raw Data
	std::valarray<uint8_t> Data08;
	std::valarray<float_t> Data32;

	std::string Name;

	Image();
	~Image();
	Image(const int32_t& width, const int32_t& height, const int32_t& channel, const int32_t& bit_per_sample);

	// Basic Interface
	void SetDefault();
	void SayGoodBye();

	bool Load();
	bool Save();

	void Create(const int32_t& width, const int32_t& height, const int32_t& channel, const int32_t& bit_per_sample, bool pack = false);
	bool DoesExist(const std::string& full_path);

	// Addressing
	static int32_t Wrap(int32_t u, const int32_t size);

	glm::vec3 GetColor(const int32_t u, const int32_t v) const;
	glm::vec3 GetColor(const glm::vec2& position) const;

	void SetColor(const glm::vec3& color, const int32_t u, const int32_t v);
	void AddColor(const glm::vec3& color, const int32_t u, const int32_t v);
	void AddColor(const glm::vec3& color, const int32_t id);

	// Utility
	void DrawCircle(const glm::vec2& p, const glm::vec3& color, const float radius);
};
