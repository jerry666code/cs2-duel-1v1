#pragma once

#include <string>
#include <vector>
#include <set>
#include <chrono>
#include "vector.h"
#include "include/duel_api.h"

enum class WeaponCategory : int
{
	Melee = 0,
	Ranged,
	Special
};

// Урон реально применяется через хук OnTakeDamage (fDamageMultiplier).
// flFireRateMultiplier/flRange хранятся как метаданные оружия для менюшек и для
// сторонних плагинов через IDuelApi/README — модификация тиков атаки штатного оружия
// CS2 намеренно не форсируется ядром, чтобы не ломать сетевые предсказания клиента.
struct WeaponDef
{
	std::string szId;
	std::string szClassname;
	std::string szDisplayName;
	WeaponCategory eCategory = WeaponCategory::Ranged;
	float flDamageMultiplier = 1.0f;
	float flFireRateMultiplier = 1.0f;
	float flRange = 0.0f; // 0 = без ограничения
	bool bBanned = false;
	std::vector<std::string> vExtraItems; // доп. предметы, выдаваемые вместе с оружием (гранаты и т.п.)
};

struct ArenaSpawn
{
	Vector vecOrigin;
	QAngle angRotation;
};

struct ArenaDef
{
	std::string szId;
	std::string szDisplayName;
	ArenaSpawn hSpawnA;
	ArenaSpawn hSpawnB;
};

struct DuelPreset
{
	std::string szId;
	std::string szDisplayName;
	float flPrepTime = 5.0f;		// время на подготовку/обратный отсчёт до начала
	float flDuelDuration = 60.0f;	// 0 = без ограничения по времени
	int iMaxHealth = 100;
	int iArmor = 0;
	bool bHelmet = false;
	float flRegenPerSecond = 0.0f;	// 0 = регенерация выключена
	bool bRandomWeapon = false;
	bool bFixedArena = false;
	std::string szFixedArenaId;
	bool bRestoreOriginalPosition = true;
	std::vector<std::string> vPreCommands;		// произвольные серверные команды при старте (баффы/погода/освещение)
	std::vector<std::string> vPostCommands;	// произвольные серверные команды по завершении
};

struct AutoAcceptSettings
{
	DuelAutoAcceptMode eMode = DuelAutoAcceptMode::Manual;
	int iMinHealthPercent = 50;
	std::set<uint64> vWhitelist;
};

struct SavedTransform
{
	bool bValid = false;
	Vector vecOrigin;
	QAngle angRotation;
	int iHealth = 100;
	int iArmor = 0;
};

enum class ChallengeState : int
{
	AwaitingWeaponPick = 0,
	AwaitingArenaPick,
	AwaitingResponse,
	Confirmed // ждёт конца раунда
};

struct PendingChallenge
{
	int iChallenger = -1;
	int iTarget = -1;
	std::string szWeaponId;	// пусто = ещё не выбрано / "random"
	std::string szArenaId;		// пусто = ещё не выбрано / "random"
	std::string szPresetId;
	ChallengeState eState = ChallengeState::AwaitingWeaponPick;
	std::chrono::steady_clock::time_point tCreatedAt;
};

enum class DuelPhase : int
{
	Preparing = 0,	// отсчёт до телепорта/начала
	Active,			// бой идёт
	Ending
};

struct DuelSession
{
	int iSlotA = -1;
	int iSlotB = -1;
	std::string szWeaponId;
	std::string szArenaId;
	std::string szPresetId;
	DuelPhase ePhase = DuelPhase::Preparing;
	float flTimeLeft = 0.0f;		// обратный отсчёт подготовки либо оставшееся время дуэли
	std::chrono::steady_clock::time_point tStartedAt;
};
