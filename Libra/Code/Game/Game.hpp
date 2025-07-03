#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/EventSystem.hpp"
#include <vector>
#include <map>
#include <string>


class RandomNumberGenerator;
class Camera;
struct Vec2;
class Entity;
class Map;



enum GameMusic
{
	ATTRACT_SCREEN,
	GAMEPLAY,
	NUM_GAME_MUSIC
};

enum GameSFX
{
	BUTTON_SELECT,
	PAUSE,
	UNPAUSE,
	ENEMY_DAMAGED,
	PLAYER_DAMAGED,
	ENEMY_KILLED,
	PLAYER_KILLED,
	BOLT_FIRED,
	BULLET_FIRED,
	BULLET_BOUNCE,
	LOOPING_FIRE,
	RESPAWN,
	NEW_LEVEL,
	GAME_VICTORY,
	GAME_OVER,
	NUM_GAME_SFX
};

class Game
{
public:
	Game();
	~Game();

	//Game Flow Management
	void BeginFrame();
	void Update(float deltaSeconds);
	void Render() const;
	void EndFrame();

	//Game Modes
	void ToggleGameOverMode(bool isInGameOver);
	void ToggleGameWonMode(bool isInGameWon);

	//Map Functions
	void GoToMap(int mapIndex);
	void GoToNextMap();

	//Helpers
	//---------------------------------------------------------- 


	//Music and SFX
	void const PlayGameMusic(GameMusic const& music);
	void const PlayGameSFX(GameSFX const& sfx);
	void const PlayGameSFX(GameSFX const& sfx, Vec2 const& worldPos);
	void const PlayLoopingGameSFX(GameSFX const& sfx);
	void const StopLoopingGameSFX(GameSFX const& sfx);
	void const StopGameMusic(SoundPlaybackID soundPlaybackID);
	float GetAudioBalanceFromWorldPosition(Vec2 const& inPosition) const;
	void const SetAudioBalanceAndVolumeFromWorldPosition(SoundPlaybackID& sound, Vec2 const& worldPos); // mutes sound if it is off screen

	//Event calls
	void HealPlayer(EventArgs& args);
	void ChangeTrackedLeo();
	void RegenerateSolidMapsForMobileEntities();

private:

	//Game Start
	void ParseGameConfigData();
	void CreateAllMaps();
	void SubscribeToEvents();

	//Update Methods
	void UpdateCameras(float deltaSeconds);
	void UpdateAttractMode(float deltaSeconds);
	void UpdateGameplay(float deltaSeconds);

	//Render Methods
	void RenderAttractMode() const;
	void RenderGameplay() const;
	void RenderGameOverMode() const;
	void RenderGameWonMode() const;
	void RenderDevConsole() const;

	//Input
	void CheckKeyboardInputs();
	void CheckControllerInputs();

	//GameModes
	void ToggleGamePaused(bool isPaused);
	void ExitGame();
	void StartGame();

	//Audio
	void LoadAllAudioAssets();
	void TogglePausedMusic(bool isPaused);
	void ChangeMusicSpeed(float speed);

	//Helpers
	//---------------------------------------------------------- 
	void AdjustTimeDistortion(float& deltaSeconds);
	void ChangeFullMapCameraBounds(IntVec2 const& mapDimensions);

	//Events
	//---------------------------------------------------------- 
	static bool HealPlayerEvent(EventArgs& args);
	static bool ChangeTrackedLeoEvent(EventArgs& args);
	static bool RegenerateSolidMapsForMobileEntitiesEvent(EventArgs& args);
	static bool Event_ShowGameControls(EventArgs& args);


public:

	//Map Info
	Map* m_currentMap = nullptr;
	std::vector<Map*> m_allMaps;
	AABB2 m_fullMapCameraBounds;
	AABB2 m_currentWorldCameraBounds;
	int m_numberTilesInViewVertically = 10;

	//Game Modes
	bool m_isPaused = false;
	bool m_shouldIgnorePauseOverlay = false;
	bool m_isSlowMo = false;
	bool m_isFastMo = false;
	bool m_inAttractMode = true;
	bool m_inGameOverCountdownMode = false;
	bool m_inGameOverMode = false;
	bool m_inGameWonMode = false;

	//Player
	Entity* m_player = nullptr;

private:

	//Camera
	Camera* m_worldCamera;
	Camera* m_screenCamera;

	//Debug
	bool m_shouldRestart = false;
	bool m_shouldUpdateOneFrame = false;

	//Attract screen
	float m_attractModeCounterTime = .9f;
	float m_attractModeCounterElapsedTime = 0.f;
	float m_attractModeDiscRadius = 200.f;
	float m_attractModeRingThickness = 20.f;

	//Audio
	SoundPlaybackID m_gameMusic;
	std::map<GameSFX, std::string> m_sfxDataPathPairs;
	std::map<GameSFX, SoundPlaybackID> m_loopingSFXSoundPlaybackPairs;

	//Timer
	float m_gameOverCountdown = 3.f;

	//Game config data
	Vec2 m_screenDimensions;
	Vec2 m_screenCenter;
	float m_worldAspect = 2.f;
	float m_maxScreenShakeTrauma = 10.f;
	int m_numTotalMaps = 0;

	//Map info
	IntVec2 m_currentMapDimensions;
	int m_currentMapIndex = 0;
};

