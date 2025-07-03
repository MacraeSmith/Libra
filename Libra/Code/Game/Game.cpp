#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/MapDefinition.hpp"

#include <Engine/Core/ErrorWarningAssert.hpp>
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/Disc2.hpp"
#include"Engine/Math/OBB2.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/TileHeatMap.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/DevConsole.hpp"

#include "Engine/Renderer/SpriteAnimDefinition.hpp"

RandomNumberGenerator* g_rng;
SpriteSheet* g_terrainSprites = nullptr;
bool g_debugMode = false;
bool g_showEntireMap = false;


Game::Game()
{
 	m_worldCamera = new Camera();
	m_screenCamera = new Camera();
	g_rng = new RandomNumberGenerator();

	Texture* terrainTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Terrain/Terrain_8x8.png");
	g_terrainSprites = new SpriteSheet(*terrainTexture, IntVec2(8, 8));

	ParseGameConfigData();
	LoadAllAudioAssets();
	PlayGameMusic(ATTRACT_SCREEN);
	CreateAllMaps();
	SubscribeToEvents();
}

Game::~Game()
{
	StopGameMusic(m_gameMusic);

	for (int mapIndex = 0; mapIndex < static_cast<int>(m_allMaps.size()); ++mapIndex)
	{
		delete m_allMaps[mapIndex];
		m_allMaps[mapIndex] = nullptr;
	}
	m_currentMap = nullptr;

	delete m_worldCamera;
	m_worldCamera = nullptr;
	delete m_screenCamera;
	m_screenCamera = nullptr;
	delete g_rng;
	g_rng = nullptr;
	
}

//Frame Flow
//--------------------------------------------------------------------
void Game::BeginFrame()
{
	m_screenCamera->SetOrthoView(Vec2(0.f, 0.f), Vec2(m_screenDimensions.x, m_screenDimensions.y));
	if (!g_showEntireMap)
	{
		AABB3 cameraBounds(m_currentWorldCameraBounds, Vec2(0.f, 1.f));
		m_worldCamera->SetOrthoView(cameraBounds);
		return;
	}

	AABB3 fullMapCameraBounds(m_fullMapCameraBounds, Vec2(0.f, 1.f));
	m_worldCamera->SetOrthoView(fullMapCameraBounds);

}

void Game::Update(float deltaSeconds)
{
	CheckKeyboardInputs();
	CheckControllerInputs();

	AdjustTimeDistortion(deltaSeconds);
	
	if (!m_inAttractMode && !m_inGameWonMode)
	{
		UpdateGameplay(deltaSeconds);
	}

	UpdateAttractMode(deltaSeconds);
	
	if (m_shouldUpdateOneFrame)
	{
		m_shouldUpdateOneFrame = false;
		m_isPaused = true;
	}

	if (!m_isPaused)
	{
		UpdateCameras(deltaSeconds);
	}

	if (m_shouldRestart)
	{
		g_app->RestartGame();
	}
}

void Game::Render() const
{
	g_renderer->ClearScreen(Rgba8(0, 0, 0, 255));

	//World Camera
	g_renderer->BeginCamera(*m_worldCamera);

	RenderGameplay();

	g_renderer->EndCamera(*m_worldCamera);

	//Screen Camera
	g_renderer->BeginCamera(*m_screenCamera);

	RenderAttractMode();
	RenderGameOverMode();
	RenderGameWonMode();
	RenderDevConsole();

	g_renderer->EndCamera(*m_screenCamera);
}

void Game::EndFrame()
{
	m_currentMap->EndFrame();
}

//Construction functions
//--------------------------------------------------------------------
void Game::LoadAllAudioAssets()
{
	g_audioSystem->CreateOrGetSound("Data/Audio/Music/AttractMusic.mp3");
	g_audioSystem->CreateOrGetSound("Data/Audio/Music/GameplayMusic.mp3");

	m_sfxDataPathPairs.insert({ BUTTON_SELECT, "Data/Audio/SFX/Click.mp3" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/Click.mp3");

	m_sfxDataPathPairs.insert({ PAUSE, "Data/Audio/SFX/Pause.mp3" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/Pause.mp3");

	m_sfxDataPathPairs.insert({ UNPAUSE, "Data/Audio/SFX/Unpause.mp3" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/Unpause.mp3");

	m_sfxDataPathPairs.insert({ BOLT_FIRED, "Data/Audio/SFX/BoltFired.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/BoltFired.wav");

	m_sfxDataPathPairs.insert({ BULLET_BOUNCE, "Data/Audio/SFX/BulletBounce.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/BulletBounce.wav");

	m_sfxDataPathPairs.insert({ BULLET_FIRED, "Data/Audio/SFX/BulletFired.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/BulletFired.wav");

	m_sfxDataPathPairs.insert({ ENEMY_DAMAGED, "Data/Audio/SFX/EnemyDamaged.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/EnemyDamaged.wav");

	m_sfxDataPathPairs.insert({ ENEMY_KILLED, "Data/Audio/SFX/EnemyDied.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/EnemyDied.wav");

	m_sfxDataPathPairs.insert({ GAME_OVER, "Data/Audio/SFX/GameOver.mp3" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/GameOver.mp3");

	m_sfxDataPathPairs.insert({ GAME_VICTORY,"Data/Audio/SFX/GameVictory.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/GameVictory.wav");

	m_sfxDataPathPairs.insert({ NEW_LEVEL, "Data/Audio/SFX/NewLevel.mp3" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/NewLevel.mp3");

	m_sfxDataPathPairs.insert({ PLAYER_KILLED,"Data/Audio/SFX/EnemyDied.wav" });

	m_sfxDataPathPairs.insert({ PLAYER_DAMAGED, "Data/Audio/SFX/PlayerDamaged.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/PlayerDamaged.wav");

	m_sfxDataPathPairs.insert({ RESPAWN, "Data/Audio/SFX/Respawn.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/Respawn.wav");

	m_sfxDataPathPairs.insert({ LOOPING_FIRE, "Data/Audio/Music/LoopingFire.wav" });
	g_audioSystem->CreateOrGetSound("Data/Audio/Music/LoopingFire.wav");
}

void Game::ParseGameConfigData()
{
	m_screenDimensions.x = g_gameConfigBlackboard.GetValue("screenSizeX", 1600.f);
	m_screenDimensions.y = g_gameConfigBlackboard.GetValue("screenSizeY", 800.f);
	m_worldAspect = m_screenDimensions.x / m_screenDimensions.y;
	m_screenCenter = g_gameConfigBlackboard.GetValue("screenCenter", Vec2(999.f, 999.f));

	m_gameOverCountdown = g_gameConfigBlackboard.GetValue("gameOverCountdownTime", 3.f);
	m_maxScreenShakeTrauma = g_gameConfigBlackboard.GetValue("maxScreenShakeTrauma", 10.f);
}

void Game::CreateAllMaps()
{
	m_numTotalMaps = g_gameConfigBlackboard.GetValue("numMaps", 0);

	MapDefinition::InitMapDefinitions();
	TileDefinition::InitTileDefinitions();

	//Get list of map names in the order set in GameConfig.xml
	std::string unseparatedMapNames = g_gameConfigBlackboard.GetValue("maps", "MAP_NAMES_NOT_FOUND");
	Strings mapNames = SplitStringOnDelimiter(unseparatedMapNames, ',');
	m_allMaps.reserve(static_cast<int>(mapNames.size()));

	MapDefinition* currentMapDef = nullptr;
	for (int mapNum = 0; mapNum < m_numTotalMaps; ++mapNum)
	{
		currentMapDef = MapDefinition::GetMapDefinitionAdressFromName(mapNames[mapNum].c_str());
		m_allMaps.push_back(new Map(this, mapNum, currentMapDef));
	}

	m_currentMap = m_allMaps[0];
	m_currentMapIndex = 0;
	m_currentMapDimensions = m_currentMap->m_dimensions;
	ChangeFullMapCameraBounds(m_currentMapDimensions);

	//create player on first map
	Entity* player = m_currentMap->SpawnNewEntity(ENTITY_TYPE_GOOD_PLAYER, FACTION_GOOD);
	m_player = dynamic_cast<Player*>(player);

	//spawn all enemies on every map
	for (int mapNum = 0; mapNum < m_numTotalMaps; ++mapNum)
	{
		m_allMaps[mapNum]->SpawnInitialNpcs();
	}
}

void Game::SubscribeToEvents()
{
	SubscribeEventCallbackFunction("Controls", Game::Event_ShowGameControls);
	SubscribeEventCallbackFunction("HealPlayer", HealPlayerEvent);
	SubscribeEventCallbackFunction("ChangeTrackedLeo", ChangeTrackedLeoEvent);
	SubscribeEventCallbackFunction("RegenerateSolidMapsForMobileEntities", RegenerateSolidMapsForMobileEntitiesEvent);
}

//Change Map
//--------------------------------------------------------------------

void Game::GoToMap(int mapIndex)
{
	m_currentMapIndex = mapIndex;
	if (m_currentMapIndex > m_numTotalMaps - 1)
	{
		ToggleGameWonMode(true);
		return;
	}

	m_currentMap->RemoveEntityFromMap(m_player);
	m_currentMap->KillAllBulletsOnMap();
	m_currentMap = m_allMaps[m_currentMapIndex];
	m_currentMapDimensions = m_currentMap->m_dimensions;
	m_currentMap->AddEntityToMap(m_player);
	m_currentMap->ResetPlayer();
	ChangeFullMapCameraBounds(m_currentMapDimensions);
}

void Game::GoToNextMap()
{
	m_currentMapIndex++;
	if (m_currentMapIndex > m_numTotalMaps - 1)
	{
		ToggleGameWonMode(true);
		return;
	}

	m_currentMap->RemoveEntityFromMap(m_player);
	m_currentMap->KillAllBulletsOnMap();
	m_currentMap = m_allMaps[m_currentMapIndex];
	m_currentMapDimensions = m_currentMap->m_dimensions;
	m_currentMap->AddEntityToMap(m_player);
	m_currentMap->ResetPlayer();
	ChangeFullMapCameraBounds(m_currentMapDimensions);
	PlayGameSFX(NEW_LEVEL);
}

//Input
//--------------------------------------------------------------------
void Game::CheckKeyboardInputs()
{
	float musicSpeed = 1.f;
	if (m_isPaused)
	{
		musicSpeed = 0.f;
	}

	//SloMo
	m_isSlowMo = (g_inputSystem->IsKeyDown('T'));
	if (m_isSlowMo && !m_isPaused)
	{
		musicSpeed -= 0.5f;
	}

	//FastMo
	m_isFastMo = (g_inputSystem->IsKeyDown('Y'));
	if (m_isFastMo && !m_isPaused)
	{
		musicSpeed += 1.5f;
	}

	ChangeMusicSpeed(musicSpeed);
	//Escape
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_ESC))
	{
		if (m_inAttractMode)
		{
			g_app->HandleQuitRequested();
		}

		if (m_inGameOverMode || m_inGameWonMode)
		{
			ExitGame();
		}

		else
		{
			if (m_isPaused)
			{
				ExitGame();
			}

			else
			{
				ToggleGamePaused(true);
				PlayGameSFX(PAUSE);
			}
		}
	}

	//P
	if (g_inputSystem->WasKeyJustPressed('P'))
	{
		if (m_inAttractMode)
		{
			StartGame();
		}

		else if (m_inGameOverMode || m_inGameWonMode)
		{
			ExitGame();
		}

		else if (!m_inGameOverCountdownMode)
		{
			if (m_isPaused)
			{
				PlayGameSFX(UNPAUSE);
			}

			else 
			{
				PlayGameSFX(PAUSE);
			}

			ToggleGamePaused(!m_isPaused);
		}
	}

	if (g_inputSystem->WasKeyJustPressed('N'))
	{
		if (m_inGameOverMode)
		{
			ToggleGameOverMode(false);
		}
	}

	//Move one Frame
	if (g_inputSystem->WasKeyJustPressed('O') )
	{
		m_shouldUpdateOneFrame = true;
		m_shouldIgnorePauseOverlay = true;
	}

	//Toggle Debug
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F1) ) //F1 key
	{
		g_debugMode = !g_debugMode;
	}

	//Restart Game
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F8)) //F8 key
	{
		m_shouldRestart = true;
	}

	//Show Full Map
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F4))
	{
		g_showEntireMap = !g_showEntireMap;
	}

	// go to next map
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F9))
	{
		GoToNextMap();
		return;
	}

}

void Game::CheckControllerInputs()
{
	XboxController controller = g_inputSystem->GetController(0);

	if (controller.WasButtonJustPressed(XboxButtonID::BUTTON_START))
	{
		if (m_inAttractMode)
		{
			StartGame();
		}

		else if (m_inGameOverMode)
		{
			ToggleGameOverMode(false);
		}

		else if (m_inGameWonMode)
		{
			ExitGame();
		}
		
		else 
		{
			if (m_isPaused)
			{
				PlayGameSFX(UNPAUSE);
			}

			else 
			{
				PlayGameSFX(PAUSE);
			}

			ToggleGamePaused(!m_isPaused);
			
		}
	}

	if (controller.WasButtonJustPressed(XboxButtonID::BUTTON_B))
	{
		if (m_inAttractMode)
		{
			g_app->HandleQuitRequested();
		}

		else if (m_isPaused || m_inGameOverMode || m_inGameWonMode)
		{
			ExitGame();
		}

		else
		{
			PlayGameSFX(PAUSE);
			ToggleGamePaused(true);
		}
	}

	if (controller.IsButtonDown(XboxButtonID::BUTTON_RTHUMB))
	{
		m_isFastMo = !m_isFastMo;
	}
	
}

//Game Modes
//--------------------------------------------------------------------
void Game::ToggleGamePaused(bool isPaused)
{
	m_isPaused = isPaused;
	TogglePausedMusic(isPaused);

}

void Game::ExitGame()
{
	m_inGameOverMode = false;
	m_inGameWonMode = false;
	m_inAttractMode = true;
	ToggleGamePaused(false);
	PlayGameSFX(BUTTON_SELECT);
	PlayGameMusic(ATTRACT_SCREEN);
}

void Game::StartGame()
{
	m_inGameOverMode = false;
	m_inGameWonMode = false;
	m_inAttractMode = false;
	ToggleGamePaused(false);
	PlayGameSFX(BUTTON_SELECT);
	PlayGameMusic(GAMEPLAY);
	GoToMap(0);
}

void Game::TogglePausedMusic(bool isPaused)
{
	if (isPaused)
	{
		g_audioSystem->SetSoundPlaybackSpeed(m_gameMusic, 0.f);
		return;
	}
	g_audioSystem->SetSoundPlaybackSpeed(m_gameMusic, 1.f);
	m_shouldIgnorePauseOverlay = false;
}

void Game::ToggleGameOverMode(bool isInGameOver)
{
	m_gameOverCountdown = g_gameConfigBlackboard.GetValue("gameOverCountdownTime", 3.f);
	m_inGameOverMode = isInGameOver;
	m_inGameOverCountdownMode = false;
	m_inGameWonMode = false;
	ToggleGamePaused(isInGameOver);

	if (!isInGameOver)
	{
		m_currentMap->RespawnPlayer();
		PlayGameSFX(BUTTON_SELECT);
	}

	else
	{
		PlayGameSFX(GAME_OVER);
	}
}

void Game::ToggleGameWonMode(bool isInGameWon)
{
	m_inGameWonMode = isInGameWon;
	m_isPaused = isInGameWon;
	PlayGameSFX(GAME_VICTORY);
}

//Update functions
//--------------------------------------------------------------------
void Game::UpdateAttractMode(float deltaSeconds)
{
	m_attractModeCounterElapsedTime += deltaSeconds;

	if (m_attractModeCounterElapsedTime >= m_attractModeCounterTime)
	{
		m_attractModeCounterElapsedTime = 0.f;
	}

	m_attractModeDiscRadius = RangeMap(m_attractModeCounterElapsedTime, 0.f, m_attractModeCounterTime, 50.f, 200.f);
}

void Game::UpdateGameplay(float deltaSeconds)
{
	m_currentMap->Update(deltaSeconds);

	if (m_inGameOverCountdownMode)
	{
		m_gameOverCountdown -= deltaSeconds;
		if (m_gameOverCountdown <= 0.f)
		{
			ToggleGameOverMode(true);
		}
	}
}

void Game::UpdateCameras(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	
	if (g_showEntireMap)
	{
		AABB3 fullMapCameraBounds(m_fullMapCameraBounds, Vec2(0.f, 1.f));
		m_worldCamera->SetOrthoView(fullMapCameraBounds);
		return;
	}

	AABB3 cameraBounds(m_currentWorldCameraBounds, Vec2(0.f, 1.f));
	m_worldCamera->SetOrthoView(cameraBounds);
}

//Render functions
//--------------------------------------------------------------------
void Game::RenderAttractMode() const
{
	if (!m_inAttractMode)
		return;

	std::vector<Vertex_PCU> shapeVerts;
	AddVertsForAABB2D(shapeVerts, AABB2(0.f, 0.f, m_screenDimensions.x, m_screenDimensions.y), Rgba8::WHITE);

	Texture* attractTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/AttractScreen.png");

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(attractTexture);
	g_renderer->DrawVertexArray(shapeVerts);

	shapeVerts.clear();
	AddVertsForRing2D(shapeVerts, Vec2(m_screenCenter.x, m_screenCenter.y), m_attractModeDiscRadius, m_attractModeRingThickness, Rgba8(100, 250, 23, 255));

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(shapeVerts);
}

void Game::RenderGameplay() const
{
	if (m_inAttractMode)
		return;
	if (m_currentMap != nullptr)
	{
		m_currentMap->Render();
	}
}

void Game::RenderGameOverMode() const
{
	if (!m_inGameOverMode)
		return;

	std::vector<Vertex_PCU> gameOverVerts;
	AABB2 screenBounds(0.f, 0.f, m_screenDimensions.x, m_screenDimensions.y);
	AddVertsForAABB2D(gameOverVerts, screenBounds, Rgba8::WHITE);
	Texture* gameOverTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/YouDiedScreen.png");

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(gameOverTexture);
	g_renderer->DrawVertexArray(gameOverVerts);
}

void Game::RenderGameWonMode() const
{
	if (!m_inGameWonMode)
		return;

	std::vector<Vertex_PCU> gameWonVerts;
	AABB2 screenBounds(0.f, 0.f, m_screenDimensions.x, m_screenDimensions.y);
	AddVertsForAABB2D(gameWonVerts, screenBounds, Rgba8::WHITE);
	Texture* gameWonTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/VictoryScreen.jpg");

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(gameWonTexture);
	g_renderer->DrawVertexArray(gameWonVerts);

}

void Game::RenderDevConsole() const
{
	AABB2 screenBounds(m_screenCamera->GetOrthoBottomLeft(), m_screenCamera->GetOrthoTopRight());
	g_devConsole->Render(screenBounds);
}

//Helper Functions
//-----------------------------------------------------------------------------------------------

void Game::AdjustTimeDistortion(float& deltaSeconds)
{

	if (m_isSlowMo) //slows down time to 1/10th
	{
		deltaSeconds *= 0.1f;
	}

	if (m_isFastMo) //speeds up time by 4
	{
		deltaSeconds *= 8.f;
	}

	if (m_shouldUpdateOneFrame) //unpause to move ship forward one frame. simSpeed should still calculates if the game is in sloMo
	{
		m_isPaused = false;
	}

	else if (m_isPaused) //freeze sim speed if game is paused
	{
		deltaSeconds = 0.0f;
	}

}

void Game::ChangeFullMapCameraBounds(IntVec2 const& mapDimensions)
{
	float numTilesVf = static_cast<float>(mapDimensions.y);
	float numTilesHf = static_cast<float>(mapDimensions.x);

	float numTilesInViewHorizontally = m_numberTilesInViewVertically * m_worldAspect;
	Vec2 camMaxs = Vec2(numTilesInViewHorizontally, static_cast<float>(m_numberTilesInViewVertically));
	m_currentWorldCameraBounds = AABB2(Vec2(0.f, 0.f), camMaxs);

	if (mapDimensions.y >= mapDimensions.x)
	{
		m_fullMapCameraBounds = AABB2(Vec2(0.f, 0.f), Vec2(numTilesVf * m_worldAspect, numTilesVf));
	}

	else
	{
		if (numTilesHf < numTilesVf * m_worldAspect)
		{
			numTilesHf = numTilesVf * m_worldAspect;
		}

		m_fullMapCameraBounds = AABB2(Vec2(0.f, 0.f), Vec2(numTilesHf, numTilesHf * static_cast<float>(m_screenDimensions.y / m_screenDimensions.x)));
	}
}

//Audio and SFX
//-----------------------------------------------------------------------------------------------
void const Game::PlayGameMusic(GameMusic const& music)
{
	SoundID newMusic;
	StopGameMusic(m_gameMusic);
	switch (music)
	{
	case ATTRACT_SCREEN:
		newMusic = g_audioSystem->CreateOrGetSound("Data/Audio/Music/AttractMusic.mp3");
		m_gameMusic = g_audioSystem->StartSound(newMusic, true);
		break;

	case GAMEPLAY:
		newMusic = g_audioSystem->CreateOrGetSound("Data/Audio/Music/GameplayMusic.mp3");
		m_gameMusic = g_audioSystem->StartSound(newMusic, true);
		break;

	default:
		break;
	}
}

void const Game::PlayGameSFX(GameSFX const& sfx)
{
	auto found = m_sfxDataPathPairs.find(sfx);
	if (found == m_sfxDataPathPairs.end())
		return;

	SoundID newSound = g_audioSystem->CreateOrGetSound(found->second);
	g_audioSystem->StartSound(newSound);
}

void const Game::PlayGameSFX(GameSFX const& sfx, Vec2 const& worldPos)
{
	auto found = m_sfxDataPathPairs.find(sfx);
	if (found == m_sfxDataPathPairs.end())
		return;

	SoundID newSound = g_audioSystem->CreateOrGetSound(found->second);
	float balance = GetAudioBalanceFromWorldPosition(worldPos);
	g_audioSystem->StartSound(newSound, false, 1.f, balance);
}

void const Game::PlayLoopingGameSFX(GameSFX const& sfx)
{
	auto foundSFX = m_sfxDataPathPairs.find(sfx);
	if (foundSFX == m_sfxDataPathPairs.end())
		return;

	SoundID newSound = g_audioSystem->CreateOrGetSound(foundSFX->second);
	auto foundLoopingSFX = m_loopingSFXSoundPlaybackPairs.find(sfx);
	if (foundLoopingSFX == m_loopingSFXSoundPlaybackPairs.end())
	{
		m_loopingSFXSoundPlaybackPairs.insert({ sfx, g_audioSystem->StartSound(newSound, true) });
	}
}

void const Game::StopLoopingGameSFX(GameSFX const& sfx)
{
	auto foundLoopingSFX = m_loopingSFXSoundPlaybackPairs.find(sfx);
	if (foundLoopingSFX != m_loopingSFXSoundPlaybackPairs.end())
	{
		g_audioSystem->StopSound(foundLoopingSFX->second);
		m_loopingSFXSoundPlaybackPairs.erase(sfx);
	}
}

void const Game::StopGameMusic(SoundPlaybackID soundPlaybackID)
{
	g_audioSystem->StopSound(soundPlaybackID);
}

void Game::ChangeMusicSpeed(float speed)
{
	g_audioSystem->SetSoundPlaybackSpeed(m_gameMusic, speed);
}

float Game::GetAudioBalanceFromWorldPosition(Vec2 const& inPosition) const
{
	return RangeMapClamped(inPosition.x, m_currentWorldCameraBounds.m_mins.x, m_currentWorldCameraBounds.m_maxs.x, -1.f, 1.f);;
}

void const Game::SetAudioBalanceAndVolumeFromWorldPosition(SoundPlaybackID& sound, Vec2 const& worldPos)
{
	if (!m_fullMapCameraBounds.IsPointInside(worldPos)) // volume = 0 if position is not in camera view
	{
		g_audioSystem->SetSoundPlaybackVolume(sound, 0.f);
	}
	g_audioSystem->SetSoundPlaybackBalance(sound, RangeMapClamped(worldPos.x, m_fullMapCameraBounds.m_mins.x, m_fullMapCameraBounds.m_maxs.x, -1.f, 1.f));
}

//Events
//-----------------------------------------------------------------------------------------------
void Game::HealPlayer(EventArgs& args)
{
	UNUSED(args);
	m_player->ReplenishHealth();
}

void Game::ChangeTrackedLeo()
{
	m_currentMap->UpdateTrackedLeo();
}

void Game::RegenerateSolidMapsForMobileEntities()
{
	m_currentMap->RegenerateSolidMapsForMobileEntities();
}

bool Game::HealPlayerEvent(EventArgs& args)
{
	if (g_game != nullptr)
	{
		g_game->HealPlayer(args);
		return true;
	}

	return false;
}

bool Game::ChangeTrackedLeoEvent(EventArgs& args)
{
	UNUSED(args);
	if (g_game != nullptr)
	{
		g_game->ChangeTrackedLeo();
		return true;
	}
	return false;
}

bool Game::RegenerateSolidMapsForMobileEntitiesEvent(EventArgs& args)
{
	UNUSED(args);
	if (g_game != nullptr)
	{
		g_game->RegenerateSolidMapsForMobileEntities();
		return true;
	}
	return false;
}

bool Game::Event_ShowGameControls(EventArgs& args)
{
	UNUSED(args);
	if (g_game != nullptr)
	{
		g_devConsole->AddLine(Rgba8::YELLOW, "--Libra Controls--", 1.f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Select - SPACEBAR/P", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Quit   - ESC, B", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Pause  - P, START", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Move   - WASD, LJoystick", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Rotate Turret - IJKL, RJoystick", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Fire   - SPACEBAR, RTrigger/A", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Switch Gun   - R, RBumper", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "SlowMo - T", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "FastMo - Y", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Step   - O", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Debug Mode - F1", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Invincible - F2", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Clip Through Walls - F3", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Full Map   - F4", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Scroll Through Debug Maps - F6", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Restart    - F8", 0.75f, true);
		g_devConsole->AddLine(DevConsole::INFO_MINOR, "Next Level - F9", 0.75f, true);
		return true;
	}
	return false;
}
