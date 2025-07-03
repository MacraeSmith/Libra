#pragma once
#include "Game/Entity.hpp"
#include "Engine/Math/MathUtils.hpp"

class Map;
class Gemini : public Entity
{
	friend class Map;

protected:
	explicit Gemini(Map* const& mapOwner, EntityType type, EntityFaction faction);
	virtual ~Gemini() {}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void Die() override;
	virtual void UpdateGameConfigXmlData() override;

	virtual Vec2 const UpdateEntityPathFinding(float deltaSeconds) override;
	

private: 
	void CreateTexture() override;

protected:
	Vec2 m_laserStartPos;
	Gemini* m_twinEntity = nullptr;

private:
	AABB2 m_turretBounds;
	Texture* m_turretTexture = nullptr;
	float m_turretOrientation;

	RaycastResult2D m_raycastResult;
};

