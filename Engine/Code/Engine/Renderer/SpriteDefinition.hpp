#pragma once
#include "Engine/Math/Vec2.hpp"

class SpriteSheet;
class Texture;
struct AABB2;

class SpriteDefinition
{
public:
	explicit SpriteDefinition(SpriteSheet const& spriteSheet, int spriteIndex, Vec2 const& uvAtMins, Vec2 const& uvAtMaxs);
	void					GetUVS(Vec2& out_uvAtMins, Vec2& out_uvAtMaxs) const;
	AABB2					GetUVS() const;
	SpriteSheet const&		GetSpriteSheet() const;
	Texture&				GetTexture() const;
	float					GetAspect() const;

protected:
	SpriteSheet const& m_spriteSheet;
	int m_spriteIndex = -1;
	Vec2 m_uvAtMins = Vec2::ZERO;
	Vec2 m_uvAtMaxs = Vec2::ONE;

};

