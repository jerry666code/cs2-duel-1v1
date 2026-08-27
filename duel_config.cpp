#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <KeyValues.h>
#include "duel.h"
#include "duel_config.h"

DuelConfig g_DuelConfig;

static const char* WEAPONS_CFG = "addons/configs/duel/weapons.cfg";
static const char* ARENAS_CFG = "addons/configs/duel/arenas.cfg";
static const char* PRESETS_CFG = "addons/configs/duel/presets.cfg";
static const char* AUTOACCEPT_CFG = "addons/configs/duel/autoaccept.cfg";

static WeaponCategory ParseCategory(const char* szValue)
{
	if (!strcmp(szValue, "melee")) return WeaponCategory::Melee;
	if (!strcmp(szValue, "special")) return WeaponCategory::Special;
	return WeaponCategory::Ranged;
}

static bool ParseVector(const char* szValue, Vector& out)
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	if (sscanf(szValue, "%f %f %f", &x, &y, &z) != 3)
		return false;
	out.x = x; out.y = y; out.z = z;
	return true;
}

static bool ParseQAngle(const char* szValue, QAngle& out)
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	if (sscanf(szValue, "%f %f %f", &x, &y, &z) != 3)
		return false;
	out.x = x; out.y = y; out.z = z;
	return true;
}

static DuelAutoAcceptMode ParseAutoAcceptMode(const char* szValue)
{
	if (!strcmp(szValue, "always")) return DuelAutoAcceptMode::Always;
	if (!strcmp(szValue, "whitelist")) return DuelAutoAcceptMode::WhitelistOnly;
	if (!strcmp(szValue, "health")) return DuelAutoAcceptMode::HealthThreshold;
	if (!strcmp(szValue, "never")) return DuelAutoAcceptMode::Never;
	return DuelAutoAcceptMode::Manual;
}

bool DuelConfig::LoadWeapons()
{
	m_Weapons.clear();

	KeyValues* pKV = new KeyValues("Weapons");
	if (!pKV->LoadFromFile(g_pFullFileSystem, WEAPONS_CFG))
	{
		g_pUtils->ErrorLog("[Duel] Failed to load '%s'", WEAPONS_CFG);
		pKV->deleteThis();
		return false;
	}

	for (KeyValues* pKey = pKV->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
	{
		const char* szId = pKey->GetName();
		const char* szClassname = pKey->GetString("classname", "");
		if (!szClassname[0])
		{
			g_pUtils->ErrorLog("[Duel] weapons.cfg: weapon '%s' is missing 'classname', skipped", szId);
			continue;
		}

		WeaponDef def;
		def.szId = szId;
		def.szClassname = szClassname;
		def.szDisplayName = pKey->GetString("name", szId);
		def.eCategory = ParseCategory(pKey->GetString("category", "ranged"));
		def.flDamageMultiplier = pKey->GetFloat("damage_mult", 1.0f);
		def.flFireRateMultiplier = pKey->GetFloat("firerate_mult", 1.0f);
		def.flRange = pKey->GetFloat("range", 0.0f);
		def.bBanned = pKey->GetBool("banned", false);

		if (def.flDamageMultiplier <= 0.0f || def.flDamageMultiplier > 10.0f)
		{
			g_pUtils->ErrorLog("[Duel] weapons.cfg: weapon '%s' has an invalid damage_mult (%.2f), clamped to 1.0", szId, def.flDamageMultiplier);
			def.flDamageMultiplier = 1.0f;
		}

		KeyValues* pExtra = pKey->FindKey("extra_items");
		if (pExtra)
		{
			FOR_EACH_VALUE(pExtra, pValue)
			{
				const char* szItem = pValue->GetString(nullptr, nullptr);
				if (szItem && szItem[0])
					def.vExtraItems.push_back(szItem);
			}
		}

		m_Weapons[def.szId] = def;
	}

	pKV->deleteThis();

	if (m_Weapons.empty())
		g_pUtils->ErrorLog("[Duel] weapons.cfg loaded 0 weapons, duels will not be able to start");

	return true;
}

bool DuelConfig::LoadArenas()
{
	m_Arenas.clear();

	KeyValues* pKV = new KeyValues("Arenas");
	if (!pKV->LoadFromFile(g_pFullFileSystem, ARENAS_CFG))
	{
		g_pUtils->ErrorLog("[Duel] Failed to load '%s'", ARENAS_CFG);
		pKV->deleteThis();
		return false;
	}

	for (KeyValues* pKey = pKV->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
	{
		const char* szId = pKey->GetName();

		KeyValues* pSpawnA = pKey->FindKey("spawn_a");
		KeyValues* pSpawnB = pKey->FindKey("spawn_b");
		if (!pSpawnA || !pSpawnB)
		{
			g_pUtils->ErrorLog("[Duel] arenas.cfg: arena '%s' is missing spawn_a/spawn_b, skipped", szId);
			continue;
		}

		ArenaDef def;
		def.szId = szId;
		def.szDisplayName = pKey->GetString("name", szId);

		if (!ParseVector(pSpawnA->GetString("pos", "0 0 0"), def.hSpawnA.vecOrigin) ||
			!ParseVector(pSpawnB->GetString("pos", "0 0 0"), def.hSpawnB.vecOrigin))
		{
			g_pUtils->ErrorLog("[Duel] arenas.cfg: arena '%s' has an invalid 'pos', skipped", szId);
			continue;
		}
		ParseQAngle(pSpawnA->GetString("ang", "0 0 0"), def.hSpawnA.angRotation);
		ParseQAngle(pSpawnB->GetString("ang", "0 0 0"), def.hSpawnB.angRotation);

		m_Arenas[def.szId] = def;
	}

	pKV->deleteThis();

	if (m_Arenas.empty())
		g_pUtils->ErrorLog("[Duel] arenas.cfg loaded 0 arenas, duels will not be able to start");

	return true;
}

bool DuelConfig::LoadPresets()
{
	m_Presets.clear();

	KeyValues* pKV = new KeyValues("Presets");
	if (!pKV->LoadFromFile(g_pFullFileSystem, PRESETS_CFG))
	{
		g_pUtils->ErrorLog("[Duel] Failed to load '%s'", PRESETS_CFG);
		pKV->deleteThis();
		return false;
	}

	m_szDefaultPresetId = pKV->GetString("default_preset", "");

	KeyValues* pPresets = pKV->FindKey("presets") ? pKV->FindKey("presets") : pKV;
	for (KeyValues* pKey = pPresets->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
	{
		const char* szId = pKey->GetName();

		DuelPreset def;
		def.szId = szId;
		def.szDisplayName = pKey->GetString("name", szId);
		def.flPrepTime = pKey->GetFloat("prep_time", 5.0f);
		def.flDuelDuration = pKey->GetFloat("duel_time", 60.0f);
		def.iMaxHealth = pKey->GetInt("max_health", 100);
		def.iArmor = pKey->GetInt("armor", 0);
		def.bHelmet = pKey->GetBool("helmet", false);
		def.flRegenPerSecond = pKey->GetFloat("regen_per_sec", 0.0f);
		def.bRandomWeapon = pKey->GetBool("random_weapon", false);
		def.bFixedArena = pKey->GetBool("fixed_arena", false);
		def.szFixedArenaId = pKey->GetString("fixed_arena_id", "");
		def.bRestoreOriginalPosition = pKey->GetBool("restore_position", true);

		if (def.iMaxHealth <= 0 || def.iMaxHealth > 1000)
		{
			g_pUtils->ErrorLog("[Duel] presets.cfg: preset '%s' has an invalid max_health (%d), clamped to 100", szId, def.iMaxHealth);
			def.iMaxHealth = 100;
		}

		KeyValues* pPre = pKey->FindKey("pre_commands");
		if (pPre) FOR_EACH_VALUE(pPre, pValue)
		{
			const char* sz = pValue->GetString(nullptr, nullptr);
			if (sz && sz[0]) def.vPreCommands.push_back(sz);
		}

		KeyValues* pPost = pKey->FindKey("post_commands");
		if (pPost) FOR_EACH_VALUE(pPost, pValue)
		{
			const char* sz = pValue->GetString(nullptr, nullptr);
			if (sz && sz[0]) def.vPostCommands.push_back(sz);
		}

		m_Presets[def.szId] = def;
	}

	pKV->deleteThis();

	if (m_Presets.empty())
	{
		g_pUtils->ErrorLog("[Duel] presets.cfg loaded 0 presets, a hardcoded fallback preset will be used");
		DuelPreset fallback;
		fallback.szId = "fallback";
		fallback.szDisplayName = "Fallback";
		m_Presets[fallback.szId] = fallback;
		m_szDefaultPresetId = fallback.szId;
	}
	else if (m_Presets.find(m_szDefaultPresetId) == m_Presets.end())
	{
		g_pUtils->ErrorLog("[Duel] presets.cfg: default_preset '%s' not found, using first available preset", m_szDefaultPresetId.c_str());
		m_szDefaultPresetId = m_Presets.begin()->first;
	}

	return true;
}

bool DuelConfig::LoadAutoAccept()
{
	KeyValues* pKV = new KeyValues("AutoAccept");
	if (!pKV->LoadFromFile(g_pFullFileSystem, AUTOACCEPT_CFG))
	{
		g_pUtils->ErrorLog("[Duel] Failed to load '%s', using defaults", AUTOACCEPT_CFG);
		pKV->deleteThis();
		m_eDefaultAutoAcceptMode = DuelAutoAcceptMode::Manual;
		m_iDefaultHealthThreshold = 50;
		return false;
	}

	m_eDefaultAutoAcceptMode = ParseAutoAcceptMode(pKV->GetString("default_mode", "manual"));
	m_iDefaultHealthThreshold = pKV->GetInt("default_health_percent", 50);
	if (m_iDefaultHealthThreshold < 1 || m_iDefaultHealthThreshold > 100)
		m_iDefaultHealthThreshold = 50;

	pKV->deleteThis();
	return true;
}

bool DuelConfig::LoadAll()
{
	bool bOk = true;
	bOk &= LoadWeapons();
	bOk &= LoadArenas();
	bOk &= LoadPresets();
	bOk &= LoadAutoAccept();
	return bOk;
}

const WeaponDef* DuelConfig::FindWeapon(const std::string& szId) const
{
	auto it = m_Weapons.find(szId);
	return it == m_Weapons.end() ? nullptr : &it->second;
}

const ArenaDef* DuelConfig::FindArena(const std::string& szId) const
{
	auto it = m_Arenas.find(szId);
	return it == m_Arenas.end() ? nullptr : &it->second;
}

const DuelPreset* DuelConfig::FindPreset(const std::string& szId) const
{
	auto it = m_Presets.find(szId);
	return it == m_Presets.end() ? nullptr : &it->second;
}

std::vector<const WeaponDef*> DuelConfig::GetSelectableWeapons() const
{
	std::vector<const WeaponDef*> vOut;
	for (auto& [id, def] : m_Weapons)
		if (!def.bBanned)
			vOut.push_back(&def);
	return vOut;
}

const WeaponDef* DuelConfig::PickWeapon(const std::string& szId) const
{
	if (szId.empty() || szId == "random")
	{
		auto vChoices = GetSelectableWeapons();
		if (vChoices.empty()) return nullptr;
		return vChoices[rand() % vChoices.size()];
	}
	const WeaponDef* pDef = FindWeapon(szId);
	if (pDef && pDef->bBanned) return nullptr;
	return pDef;
}

const ArenaDef* DuelConfig::PickArena(const std::string& szId) const
{
	if (szId.empty() || szId == "random")
	{
		if (m_Arenas.empty()) return nullptr;
		auto it = m_Arenas.begin();
		std::advance(it, rand() % m_Arenas.size());
		return &it->second;
	}
	return FindArena(szId);
}

const DuelPreset* DuelConfig::GetDefaultPreset() const
{
	return FindPreset(m_szDefaultPresetId);
}
