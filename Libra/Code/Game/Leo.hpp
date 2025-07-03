#pragma once
#include "Game/Entity.hpp"
class Leo : public Entity
{
	friend class Map;
protected:
	explicit Leo(Map* const& mapOwner, EntityFaction faction);
	virtual ~Leo() {}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void UpdateGameConfigXmlData() override;

	void DebugRenderPathFindingInfo() const;

private:
	void CreateTexture() override;

};

