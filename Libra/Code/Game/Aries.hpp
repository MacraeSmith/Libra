#pragma once
#include "Game/Entity.hpp"
class Map;
class Aries : public Entity
{
	friend class Map;
protected:
	explicit Aries(Map* const& mapOwner, EntityFaction faction);
	virtual ~Aries() {}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void UpdateGameConfigXmlData() override;

	bool DidBulletHitShield(Vec2 const& bulletPos) const;

private:
	void CreateTexture() override;

private:
	float m_shieldAperture;
};

