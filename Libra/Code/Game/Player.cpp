#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"

#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/LineSegment2.hpp"


Player::Player(Map* const& mapOwner, EntityFaction faction, Vec2 const& startingPos, float const& startingOrientationDegrees)
	:Entity(mapOwner, ENTITY_TYPE_GOOD_PLAYER, faction, startingPos, startingOrientationDegrees)
{
	UpdateGameConfigXmlData();
	CreateTexture();
}

void Player::Update(float deltaSeconds)
{
	UpdateTimers(deltaSeconds);
	m_isPushedByWalls = !g_noClipMode;

	CheckInput(deltaSeconds);

	if (IsOnTargetTile(m_position, m_map->m_endCoord))
	{
		g_game->GoToNextMap();
		return;
	}

	if (DidChangeTile())
	{
		IntVec2 tileCoords = m_map->GetTileCoordsFromPosition(m_position);
		m_map->PopulateDistanceMapWithStationaryEntities(*m_map->m_distanceMapToPlayer, tileCoords, DEFAULT_HEAT_MAP_SOLID_VALUE);
		m_map->PopulateDistanceMapWithStationaryEntities(*m_map->m_amphibianDistanceMapToPlayer, tileCoords, DEFAULT_HEAT_MAP_SOLID_VALUE, false);
	}

	m_positionLastFrame = m_position;
}

void Player::Render() const
{

	Vec2 fwrdNormal = GetForwardNormal();
	std::vector<Vertex_PCU> worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);
	
	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);

	fwrdNormal.RotateDegrees(m_turretRelativeOffset);
	worldVerts.clear();
	AddVertsForAABB2D(worldVerts, m_turretBounds, Rgba8::WHITE);
	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_turretTexture);
	g_renderer->DrawVertexArray(worldVerts);
	
	std::vector<Vertex_PCU> shapeVerts;

	if (g_noClipMode)
	{
		AddVertsForRing2D(shapeVerts, m_position, 0.4f, m_debugLineThickness, Rgba8::BLACK);
	}

	if (m_isInvincible)
	{
		AddVertsForRing2D(shapeVerts, m_position, 0.45f, m_debugLineThickness, Rgba8(255, 255, 255, 150));
	}

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(shapeVerts);
}

void Player::DebugRender() const
{
	Vec2 vecFwrd = GetForwardNormal() * m_cosmeticRadius;
	//Turret forward Vector
	Vec2 turretFwrd = vecFwrd.GetRotatedDegrees(m_turretRelativeOffset);
	DebugDrawLine2D(m_position, m_position + turretFwrd, m_debugLineThickness * 2.5f, Rgba8::BLUE);

	//Physics Ring
	DebugDrawRing(m_position, m_physicsRadius, m_debugLineThickness, Rgba8(0, 255, 255));

	//Forward Vector
	DebugDrawLine2D(m_position, m_position + vecFwrd, m_debugLineThickness, Rgba8::RED);

	//Left Vector
	Vec2 vecLeft = vecFwrd.GetRotated90Degrees();
	DebugDrawLine2D(m_position, m_position + vecLeft, m_debugLineThickness, Rgba8::GREEN);

	//Velocity Line
	DebugDrawLine2D(m_position, m_position + m_velocity, m_debugLineThickness, Rgba8(255, 255, 0));

	//Turret Goal Direction
	Vec2 turretGoalDirection = vecFwrd.GetRotatedDegrees(m_turretGoalOrientationDegrees);
	DebugDrawLine2D(m_position + turretGoalDirection, m_position + turretGoalDirection * 1.25f, m_debugLineThickness * 2.5f, Rgba8::BLUE);

	//Goal Direction
	Vec2 goalDirection = Vec2::MakeFromPolarDegrees(m_goalOrientationDegrees);
	DebugDrawLine2D(m_position + goalDirection * 0.5f, m_position + goalDirection * 0.75f, m_debugLineThickness, Rgba8::RED);
}

void Player::LoseHealth(float amount)
{
	if (m_isDead)
		return;

	if (!m_isInvincible)
	{
		m_health -= amount;

		if (m_damageSFXAge >= SFX_PLAY_RATE)
		{
			g_game->PlayGameSFX(PLAYER_DAMAGED, m_position);
			m_damageSFXAge = 0.f;
		}
	}

	if (m_health <= 0)
	{
		Die();
	}
}

void Player::Die()
{
	m_isDead = true;
	g_game->m_inGameOverCountdownMode = true;
	m_map->SpawnExplosion(m_position, DEATH_EXPLOSION_SIZE, DEATH_EXPLOSION_DURATION);
	g_game->PlayGameSFX(PLAYER_KILLED, m_position);
}

void Player::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("playerPhysicsRadius", 0.3f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("playerStartingHealth", 10.f);
	m_health = m_maxHealth;
	m_moveSpeed = g_gameConfigBlackboard.GetValue("playerDriveSpeed", 1.5f);
	m_turnSpeed = g_gameConfigBlackboard.GetValue("playerTurnRate", 180.f);
	m_turretTurnSpeed = g_gameConfigBlackboard.GetValue("playerTurretTurnRate", 360.f);
	m_fireRate = g_gameConfigBlackboard.GetValue("playerFireRate", 0.1f);
	m_bulletSpawnOffset = g_gameConfigBlackboard.GetValue("playerBulletSpawnOffset", 0.4f);
	m_flameSpread = g_gameConfigBlackboard.GetValue("playerFlameSpread", 15.f);
}

void Player::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Tank/PlayerTankBase.png");
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);

	m_turretBounds = m_entityBounds;
	m_turretTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Tank/PlayerTankTop.png");
}

void Player::CheckInput(float deltaSeconds)
{
	CheckControllerInput(deltaSeconds);
	CheckKeyboardInput(deltaSeconds);
}

void Player::CheckKeyboardInput(float deltaSeconds)
{

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F2))
	{
		m_isInvincible = !m_isInvincible;
	}

	if (m_isDead)
		return;

	Vec2 moveIntentions = Vec2::ZERO;
	
	if (g_inputSystem->IsKeyDown('W'))
	{
		moveIntentions += Vec2::NORTH;
	}

	if (g_inputSystem->IsKeyDown('A'))
	{
		moveIntentions += Vec2::WEST;
	}

	if (g_inputSystem->IsKeyDown('S'))
	{
		moveIntentions += Vec2::SOUTH;
	}

	if (g_inputSystem->IsKeyDown('D'))
	{
		moveIntentions += Vec2::EAST;
	}

	Vec2 fwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees);
	if (moveIntentions != Vec2::ZERO)
	{
		m_goalOrientationDegrees = moveIntentions.GetOrientationDegrees();
		m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, m_goalOrientationDegrees, m_turnSpeed * deltaSeconds);

		m_velocity = fwrdNormal * m_moveSpeed;
		m_position += m_velocity * deltaSeconds;
	}
	
	Vec2 turretGoalDirection = Vec2::ZERO;
	if (g_inputSystem->IsKeyDown('I'))
	{
		turretGoalDirection += Vec2::NORTH;
	}

	if (g_inputSystem->IsKeyDown('J'))
	{
		turretGoalDirection += Vec2::WEST;
	}

	if (g_inputSystem->IsKeyDown('K'))
	{
		turretGoalDirection += Vec2::SOUTH;
	}

	if (g_inputSystem->IsKeyDown('L'))
	{
		turretGoalDirection += Vec2::EAST;
	}

	if (turretGoalDirection != Vec2::ZERO)
	{
		m_turretGoalOrientationDegrees = turretGoalDirection.GetOrientationDegrees() - m_orientationDegrees;
		m_turretRelativeOffset = GetTurnedTowardDegrees(m_turretRelativeOffset, m_turretGoalOrientationDegrees, m_turretTurnSpeed * deltaSeconds);
	}

	if (g_inputSystem->WasKeyJustPressed('R'))
	{
		ToggleWeaponTypes();
	}

	if (g_inputSystem->IsKeyDown(' '))
	{
		if (m_weaponType == FLAMETHROWER && !m_isPlayingLoopingFireSFX)
		{
			g_game->PlayLoopingGameSFX(LOOPING_FIRE);
			m_isPlayingLoopingFireSFX = true;
		}

		if (m_fireCountdownTime <= 0)
		{
			Vec2 bulletFwrdNormal;

			EntityType entityType;
			switch (m_weaponType)
			{
			case BOLT:
				entityType = ENTITY_TYPE_GOOD_BOLT;
				bulletFwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees + m_turretRelativeOffset);
				break;
			case FLAMETHROWER:
				entityType = ENTITY_TYPE_GOOD_FLAME_BULLET;
				bulletFwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees + g_rng->RollRandomFloatInRange(-m_flameSpread, m_flameSpread) + m_turretRelativeOffset);
				break;
			default:
				entityType = ENTITY_TYPE_GOOD_BOLT;
				break;
			}

			TryShootBullet(entityType, FACTION_GOOD, bulletFwrdNormal, true);
		}
	}

	if (g_inputSystem->WasKeyJustReleased(' ') && m_isPlayingLoopingFireSFX)
	{
		g_game->StopLoopingGameSFX(LOOPING_FIRE);
		m_isPlayingLoopingFireSFX = false;
	}
}

void Player::CheckControllerInput(float deltaSeconds)
{
	XboxController controller = g_inputSystem->GetController(0);
	AnalogJoystick const& leftStick = controller.GetLeftStick();
	AnalogJoystick const& rightStick = controller.GetRightStick();

	if (g_game->m_inGameOverMode || controller.WasButtonJustPressed(XboxButtonID::BUTTON_START))
	{
		RespawnPlayer();
	}

	if (m_isDead)
		return;

	//Rotation and movement
	float magnitude = leftStick.GetMagnitude();
	if (magnitude > 0.f)
	{
		float rotationIntentions = leftStick.GetOrientationDegrees();

		m_goalOrientationDegrees = rotationIntentions;
		m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, m_goalOrientationDegrees, magnitude * m_turnSpeed * deltaSeconds);
	}

	Vec2 fwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees);
	m_velocity = fwrdNormal * magnitude * m_moveSpeed;
	m_position += m_velocity * deltaSeconds;
	
	magnitude = rightStick.GetMagnitude();

	if (magnitude > 0.f)
	{
		float turretRotateIntentions = rightStick.GetOrientationDegrees() - m_orientationDegrees;
		m_turretGoalOrientationDegrees = turretRotateIntentions;
		m_turretRelativeOffset = GetTurnedTowardDegrees(m_turretRelativeOffset, m_turretGoalOrientationDegrees, magnitude * m_turretTurnSpeed * deltaSeconds);
	}

	if (controller.WasButtonJustPressed(XboxButtonID::BUTTON_RSHOULDER))
	{
		ToggleWeaponTypes();
	}

	if (controller.GetRightTrigger() > 0.f)
	{
		if (m_weaponType == FLAMETHROWER && !m_isPlayingLoopingFireSFX)
		{
			g_game->PlayLoopingGameSFX(LOOPING_FIRE);
			m_isPlayingLoopingFireSFX = true;
		}

		if (m_fireCountdownTime <= 0)
		{
			if (m_fireCountdownTime <= 0)
			{
				Vec2 bulletFwrdNormal;

				EntityType entityType;
				switch (m_weaponType)
				{
				case BOLT:
					entityType = ENTITY_TYPE_GOOD_BOLT;
					bulletFwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees + m_turretRelativeOffset);
					break;
				case FLAMETHROWER:
					entityType = ENTITY_TYPE_GOOD_FLAME_BULLET;
					bulletFwrdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees + g_rng->RollRandomFloatInRange(-m_flameSpread, m_flameSpread) + m_turretRelativeOffset);
					break;
				default:
					entityType = ENTITY_TYPE_GOOD_BOLT;
					break;
				}

				TryShootBullet(entityType, FACTION_GOOD, bulletFwrdNormal, true);
			}
		}
	}

	if(controller.WasButtonJustReleased(XboxButtonID::BUTTON_VIRTUAL_RIGHT_TRIGGER_BUTTON) && m_isPlayingLoopingFireSFX)
	{
		g_game->StopLoopingGameSFX(LOOPING_FIRE);
		m_isPlayingLoopingFireSFX = false;
	}

	if (m_isPlayingLoopingFireSFX && controller.GetRightTrigger() <= 0.f && !g_inputSystem->IsKeyDown(' '))
	{
		g_game->StopLoopingGameSFX(LOOPING_FIRE);
		m_isPlayingLoopingFireSFX = false;
	}
	
}

void Player::ToggleWeaponTypes()
{
	m_fireCountdownTime = 0.f;

	if (m_weaponType == BOLT)
	{
		m_weaponType = FLAMETHROWER;
		m_fireRate = g_gameConfigBlackboard.GetValue("playerFlameSprayRate", 0.05f);
		return;
	}

	m_weaponType = BOLT;
	m_fireRate = g_gameConfigBlackboard.GetValue("playerFireRate", 0.1f);

	if (m_isPlayingLoopingFireSFX)
	{
		g_game->StopLoopingGameSFX(LOOPING_FIRE);
	}
}

void Player::RespawnPlayer()
{
	m_isDead = false;
	m_health = g_gameConfigBlackboard.GetValue("playerStartingHealth", 10.f);
}
