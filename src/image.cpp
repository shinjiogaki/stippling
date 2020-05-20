// Redqueen Copyright (c) shinji All Rights Reserved.

#include "image.h"
#include <algorithm> // clamp
#include <fstream> // ifstream

#define STB_IMAGE_IMPLEMENTATION
#include "..\stb\stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "..\stb\stb_image_write.h"

Image::Image()
{
	SetDefault();
}

Image::Image(const int32_t &width, const int32_t &height, const int32_t &channel, const int32_t &bit_per_sample)
{
	Create(width, height, channel, bit_per_sample);
}

Image::~Image()
{
	SayGoodBye();
}

void Image::SetDefault()
{
	Width = 0;
	Height = 0;
	Channel = 3;
	BitsPerSample = 32;
	Name = "";
}

void Image::SayGoodBye()
{
	SetDefault();

	Data08.~valarray();
	Data32.~valarray();
}

void Image::Create(const int32_t& width, const int32_t& height, const int32_t& channel, const int32_t& bit_per_sample, bool pack)
{
	if (0 >= width || 0 >= height || 0 >= channel || 0 >= bit_per_sample)
	{
		return;
	}

	Width = width;
	Height = height;
	Channel = channel;
	BitsPerSample = bit_per_sample;

	const auto num = Width * Height * Channel;
	if  (8 == BitsPerSample) { Data08.resize(num); Data08 = 0;    }
	if (32 == BitsPerSample) { Data32.resize(num); Data32 = 0.0f; }
}

int32_t Image::Wrap(int32_t p, const int32_t size)
{
	return (p >= 0) ? (p % size) : (size - 1) - std::abs(p + 1) % size;
}

glm::vec3 Image::GetColor(const glm::vec2 &position) const
{
	const auto int_u = (int32_t)std::floor(Width  * position.x);
	const auto int_v = (int32_t)std::floor(Height * position.y);
	const auto new_u = Wrap(int_u, Width );
	const auto new_v = Wrap(int_v, Height);
	return GetColor(new_u, new_v);
}

// Fundamental
glm::vec3 Image::GetColor(const int32_t u, const int32_t v) const
{
	glm::vec3 color;
	const auto pixel_id = v * Width + u;

	if (3 <= Channel)
	{
		const auto index = pixel_id * Channel;
		switch (BitsPerSample)
		{
		case 8:
			color.x = (float_t)Data08[index + 0];
			color.y = (float_t)Data08[index + 1];
			color.z = (float_t)Data08[index + 2];
			color /= 255.0f;
			break;
		case 32:
			color.x = Data32[index + 0];
			color.y = Data32[index + 1];
			color.z = Data32[index + 2];
			break;
		}
	}
	else if (Channel == 1)
	{
		switch (BitsPerSample)
		{
		case  8: color = glm::vec3(Data08[pixel_id] / 255.0f); break;
		case 32: color = glm::vec3(Data32[pixel_id]); break;
		}
	}
	return color;
}

void Image::SetColor(const glm::vec3 &color, const int32_t u, const int32_t v)
{
	const auto index = (v * Width + u) * Channel;
	switch (BitsPerSample)
	{
	case 32:
		Data32[index + 0] = color.x;
		Data32[index + 1] = color.y;
		Data32[index + 2] = color.z;
		break;
	case 8:
		Data08[index + 0] = color.x >= 1 ? 255 : (color.x < 0 ? 0 : (uint8_t)(color.x * 255.0f));
		Data08[index + 1] = color.y >= 1 ? 255 : (color.y < 0 ? 0 : (uint8_t)(color.y * 255.0f));
		Data08[index + 2] = color.z >= 1 ? 255 : (color.z < 0 ? 0 : (uint8_t)(color.z * 255.0f));
		break;
	}
}

void Image::AddColor(const glm::vec3 &color, const int32_t u, const int32_t v)
{
	AddColor(color, v * Width + u);
}

void Image::AddColor(const glm::vec3& color, const int32_t id)
{
	const auto index = id * Channel;
	switch (BitsPerSample)
	{
	case 32:
		Data32[index + 0] += color.x;
		Data32[index + 1] += color.y;
		Data32[index + 2] += color.z;
		break;
	case 8:
		Data08[index + 0] = (uint8_t)(255.0f * std::clamp(Data08[index + 0] / 255.0f + color.x, 0.0f, 1.0f));
		Data08[index + 1] = (uint8_t)(255.0f * std::clamp(Data08[index + 1] / 255.0f + color.y, 0.0f, 1.0f));
		Data08[index + 2] = (uint8_t)(255.0f * std::clamp(Data08[index + 2] / 255.0f + color.z, 0.0f, 1.0f));
		break;
	}
}

bool Image::DoesExist(const std::string &full_path)
{
	std::ifstream stream(full_path.data(), std::ios::in | std::ios::binary);
	if (stream.is_open())
	{
		stream.close();
		return true;
	}
	return false;
}

bool Image::Load()
{
	if (Name.empty())
		return false;

	if (DoesExist(Name))
	{
		if (stbi_is_hdr(Name.data()))
		{
			auto pixels = stbi_loadf(Name.data(), &Width, &Height, &Channel, 0);
			Create(Width, Height, Channel, 32);
			for (auto id = 0; id < Width * Height * 3; ++id)
				Data32[id] = pixels[id];
			stbi_image_free(pixels);
		}
		else
		{
			auto pixels = stbi_load(Name.data(), &Width, &Height, &Channel, 0);
			Create(Width, Height, Channel, 8);
			for (auto v = 0; v < Height; ++v)
			{
				for (auto u = 0; u < Width; ++u)
				{
					const auto id0 = (v * Width + u) * Channel;
					const auto id1 = ((Height - 1 - v) * Width + u) * Channel;
					for (auto c = 0; c < Channel; ++c)
						Data08[id1 + c] = pixels[id0 + c];
				}
			}
			stbi_image_free(pixels);
		}
		return true;
	}

	return false;
}

bool Image::Save()
{
	if (0 < Data32.size())
	{
		stbi_write_hdr(Name.data(), Width, Height, Channel, &Data32[0]);
		return true;
	}
	if (0 < Data08.size())
	{
		stbi_write_png(Name.data(), Width, Height, Channel, &Data08[0], Width * Channel);
		return true;
	}
	return false;
}

int32_t wrap(const int32_t c, const int32_t S)
{
	if (c <  0) return c + S;
	if (c >= S) return c - S;
	return c;
}

void Image::DrawCircle(const glm::vec2 &given_p, const glm::vec3 &color, const float r)
{
	static const auto aa = 4;

	const auto p = glm::vec2(given_p.x, 1.0f - given_p.y) * glm::vec2((float_t)Width, (float_t)Height);
	const auto min_u = (int32_t)std::floor(p.x - r);
	const auto min_v = (int32_t)std::floor(p.y - r);
	const auto max_u = (int32_t)std::ceil(p.x + r);
	const auto max_v = (int32_t)std::ceil(p.y + r);
	for (auto v = min_v; v <= max_v; ++v)
	{
		for (auto u = min_u; u <= max_u; ++u)
		{
			for (auto y = 0; y < aa; ++y)
			{
				for (auto x = 0; x < aa; ++x)
				{
					glm::vec2 c(u + (x + 0.5f) / aa, v + (y + 0.5f) / aa);
					const auto nu = wrap(u, Width);
					const auto nv = wrap(v, Height);
					assert(0 <= nu && nu < Width);
					assert(0 <= nv && nv < Height);
					if (glm::dot(c - p, c - p) < r * r)
						AddColor(color / (float_t)(aa * aa), nu, nv);
				}
			}
		}
	}
}
