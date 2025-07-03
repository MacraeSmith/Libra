#pragma once
#include "Game/Entity.hpp"
#include "Game/Tile.hpp"
#include <map>

class Map;
struct TileDefinition;
struct Tile;
class Aquarius : public Entity
{
	friend class Map;
protected:
	explicit Aquarius(Map* const& mapOwner, EntityFaction faction);
	virtual ~Aquarius() {}

	virtual void Update(float deltaSeconds) override;
	virtual Vec2 const UpdateEntityPathFinding(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void UpdateGameConfigXmlData() override;

private:
	void CreateTexture() override;

private:
	float m_trailAge;
};

