#pragma once
#include "Engine/Math/Vec2.hpp"
struct Disc2
{
public:
	Vec2 m_center;
	float m_radius = 1.f;

public:
	Disc2() {}
	~Disc2() {}
	explicit Disc2(Vec2 const& center, float const& radius);

	//Accessors

	//Mutators
	void Translate(Vec2 const& translation);
};

