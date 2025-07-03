#pragma once
#include "Game/Entity.hpp"
class Capricorn : public Entity
{
	friend class Map;
protected:
	explicit Capricorn(Map* const& mapOwner, EntityFaction faction);
	virtual ~Capricorn(){}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void UpdateGameConfigXmlData() override;

private:
	void CreateTexture() override;


};

