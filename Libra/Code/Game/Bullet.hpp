#pragma once
#include "Game/Entity.hpp"
class Map;
class SpriteAnimDefinition;
class Bullet : public Entity
{
	friend class Map;
protected:
	explicit Bullet(Map* const& mapOwner, EntityType entityType, EntityFaction faction);
	virtual ~Bullet();

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void LoseHealth(float amount) override;
	virtual void Die() override;
	virtual void UpdateGameConfigXmlData() override;
	void BounceOffSurfaceNormal(Vec2 const& surfaceNormal);

private:
	void CreateTexture() override;
	void TurnTowardsPlayer(float maxTurnDegrees);

private:

	int m_numBounces = 0;
	bool m_isTrackingBullet = false;
	bool m_isFlameBullet = false;
	float m_bulletLength;
	float m_decayRate = 1.f;
	float m_flameOrientation = 0.f;
	float m_flameRotationSpeed;
	SpriteAnimDefinition* m_spriteAnimDef = nullptr;
};

