#include "Disc2.hpp"

Disc2::Disc2(Vec2 const& center, float const& radius)
	:m_center(center)
	,m_radius(radius)
{
}

void Disc2::Translate(Vec2 const& translation)
{
	m_center += translation;
}
