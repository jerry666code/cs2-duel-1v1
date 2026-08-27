#pragma once

#include <map>
#include <string>
#include "duel_types.h"

class DuelConfig
{
public:
	bool LoadAll();
	bool LoadWeapons();
	bool LoadArenas();
	bool LoadPresets();
	bool LoadAutoAccept();

	const WeaponDef* FindWeapon(const std::string& szId) const;
	const ArenaDef* FindArena(const std::string& szId) const;
	const DuelPreset* FindPreset(const std::string& szId) const;

	const WeaponDef* PickWeapon(const std::string& szId) const; // "" или "random" -> случайное не забаненное
	const ArenaDef* PickArena(const std::string& szId) const;   // "" или "random" -> случайная арена
	const DuelPreset* GetDefaultPreset() const;

	std::vector<const WeaponDef*> GetSelectableWeapons() const; // без забаненных
	const std::map<std::string, ArenaDef>& GetArenas() const { return m_Arenas; }

	DuelAutoAcceptMode GetDefaultAutoAcceptMode() const { return m_eDefaultAutoAcceptMode; }
	int GetDefaultHealthThreshold() const { return m_iDefaultHealthThreshold; }

private:
	std::map<std::string, WeaponDef> m_Weapons;
	std::map<std::string, ArenaDef> m_Arenas;
	std::map<std::string, DuelPreset> m_Presets;
	std::string m_szDefaultPresetId;

	DuelAutoAcceptMode m_eDefaultAutoAcceptMode = DuelAutoAcceptMode::Manual;
	int m_iDefaultHealthThreshold = 50;
};

extern DuelConfig g_DuelConfig;
