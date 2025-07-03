#pragma once
#include "Game/Entity.hpp"
#include "Engine/Math/MathUtils.hpp"

class Map;
class Scorpio : public Entity
{
	friend class Map;
protected:
	explicit Scorpio(Map* const& mapOwner, EntityFaction faction);
	virtual ~Scorpio() {}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void Die() override;
	virtual void UpdateGameConfigXmlData() override;

private:
	void CreateTexture() override;

private:
	AABB2 m_turretBounds;
	Texture* m_turretTexture = nullptr;
	Vec2 m_laserHitPos;
	Ray2 m_laserRay;
	float m_laserMaxLength;
	float m_laserLengthFraction = 1.f;
	float m_turretTurnSpeed;
};

