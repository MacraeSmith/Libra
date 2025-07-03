#pragma once
#include "Game/Entity.hpp"

class Game;
class Map;

enum WeaponType : int 
{
	BOLT,
	FLAMETHROWER,
	NUM_WEAPON_TYPES,
};

class Player : public Entity
{
	friend class Map;
protected:
	explicit Player(Map* const& mapOwner, EntityFaction faction, Vec2 const& startingPos, float const& startingOrientationDegrees = 0.f);
	virtual ~Player() {}

	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;
	virtual void DebugRender() const override;
	virtual void LoseHealth(float amount) override;
	virtual void Die() override;
	virtual void UpdateGameConfigXmlData() override;
	void RespawnPlayer();


private:
	void CreateTexture() override;

	//Input
	void CheckInput(float deltaSeconds);
	void CheckKeyboardInput(float deltaSeconds);
	void CheckControllerInput(float deltaSeconds);

	void ToggleWeaponTypes();

private:
	AABB2 m_turretBounds;
	Texture* m_turretTexture = nullptr;
	float m_turretTurnSpeed;
	float m_goalOrientationDegrees;
	float m_turretGoalOrientationDegrees;
	float m_turretRelativeOffset;
	bool m_isInvincible = false;
	WeaponType m_weaponType = BOLT;
	float m_flameSpread;
	bool m_isPlayingLoopingFireSFX = false;
};

