#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/EngineCommon.hpp"

class RendererDX11;
class App;
class RandomNumberGenerator;
class SpriteSheet;
class InputSystem;
struct Rgba8;
class AudioSystem;
class Window;
class Game;
class DevConsole;

extern RendererDX11* g_renderer;
extern App* g_app;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_inputSystem;
extern AudioSystem* g_audioSystem;
extern Window* g_window;
extern Game* g_game;
extern SpriteSheet* g_terrainSprites;
extern DevConsole* g_devConsole;

extern bool g_debugMode;
extern bool g_noClipMode;
extern bool g_showEntireMap;

constexpr int MAX_NUM_WORM_TYPES = 4;
constexpr int NUM_DEBUG_HEAT_MAPS = 5;
constexpr float DEFAULT_HEAT_MAP_SOLID_VALUE = 9999.f;
constexpr int TERRAIN_SPRITES_WIDTH = 8;

constexpr float DEATH_EXPLOSION_SIZE = 1.f;
constexpr float DEATH_EXPLOSION_DURATION = 1.f;
constexpr float BULLET_DEATH_EXPLOSION_SIZE = 0.5f;
constexpr float BULLET_FIRE_EXPLOSION_SIZE = 0.35f;
constexpr float BULLET_FIRE_EXPLOSION_DURATION = 0.5f;

constexpr float SFX_PLAY_RATE = 0.2f;

void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);
void DebugDrawLine2D(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 color);


