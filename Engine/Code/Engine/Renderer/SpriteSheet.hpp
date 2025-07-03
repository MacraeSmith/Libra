#pragma once
#include "Engine/Renderer/SpriteDefinition.hpp"
#include <vector>

struct IntVec2;
struct AABB2;
class Texture;
class SpriteDefinition;

class SpriteSheet
{
public:
	explicit SpriteSheet(Texture& texture, IntVec2 const& simpleGridLayout);
	~SpriteSheet() {}

	Texture&					GetTexture() const;
	int							GetNumSprites() const;
	SpriteDefinition const&		GetSpriteDef(int spriteIndex) const;
	void						GetSpriteUVs(Vec2& out_uvAtMins, Vec2& out_uvAtMax, int spriteIndex) const;
	AABB2						GetSpriteUVs(int spriteIndex) const;

protected:
	Texture& m_texture;
	std::vector<SpriteDefinition> m_spriteDefs;
};

