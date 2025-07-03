#pragma once
#include "Game/Entity.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
class SpriteSheet;

class Explosion : public Entity
{
	friend class Map;
protected:
	explicit Explosion(Map* const& mapOwner, Vec2 const& centerPos, AABB2 const& bounds, SpriteSheet const& spriteSheet, Rgba8 const& tint, SpriteAnimPlaybackType playbackType, int startIndex, int endIndex, float duration);
	virtual ~Explosion(){}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void Die() override;
	virtual void UpdateGameConfigXmlData() override;

private:
	void CreateTexture() override;
	
protected:
	SpriteAnimDefinition m_spriteAnimDef;
	float m_duration;
	float m_age;
	Rgba8 m_tint;
};

