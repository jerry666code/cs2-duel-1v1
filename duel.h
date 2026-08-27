#ifndef _INCLUDE_METAMOD_SOURCE_DUEL_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_DUEL_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include "utils.hpp"
#include <utlstring.h>
#include <KeyValues.h>
#include "CCSPlayerController.h"
#include "CGameRules.h"
#include "module.h"
#include "include/menus.h"
#include "include/cookies.h"
#include "include/duel_api.h"
#include <map>
#include <ctime>
#include <chrono>
#include <array>
#include <vector>

class Duel final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
};

class DuelApi final : public IDuelApi
{
public:
	bool Duel_IsInDuel(int iSlot) override;
	bool Duel_IsChallengePending(int iSlot) override;
	int  Duel_GetOpponent(int iSlot) override;
	int  Duel_GetActiveDuelCount() override;

	bool Duel_SendChallenge(int iChallenger, int iTarget, const char* szWeaponId, const char* szArenaId) override;
	bool Duel_AcceptChallenge(int iTarget) override;
	bool Duel_DeclineChallenge(int iTarget) override;
	bool Duel_CancelChallenge(int iSlot) override;
	bool Duel_ForceEnd(int iSlot, DuelEndReason eReason) override;

	void Duel_SetAutoAcceptMode(int iSlot, DuelAutoAcceptMode eMode) override;
	DuelAutoAcceptMode Duel_GetAutoAcceptMode(int iSlot) override;
	void Duel_SetAutoAcceptHealthThreshold(int iSlot, int iMinHealthPercent) override;
	void Duel_AddAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) override;
	void Duel_RemoveAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) override;
	bool Duel_IsAutoAcceptWhitelisted(int iSlot, uint64 iSteamID64) override;

	void Duel_OpenMenu(int iSlot) override;
	void Duel_OpenSettingsMenu(int iSlot) override;

	void Duel_OnChallengeSent(DuelChallengeCallback callback) override { m_vOnChallengeSent.push_back(callback); }
	void Duel_OnChallengeConfirmed(DuelConfirmedCallback callback) override { m_vOnChallengeConfirmed.push_back(callback); }
	void Duel_OnDuelStart(DuelStartCallback callback) override { m_vOnDuelStart.push_back(callback); }
	void Duel_OnDuelEnd(DuelEndCallback callback) override { m_vOnDuelEnd.push_back(callback); }

	const char* Duel_GetVersion() override;

	void Fire_OnChallengeSent(int iChallenger, int iTarget);
	void Fire_OnChallengeConfirmed(int iChallenger, int iTarget);
	void Fire_OnDuelStart(int iSlotA, int iSlotB, const char* szWeaponId, const char* szArenaId);
	void Fire_OnDuelEnd(int iSlotA, int iSlotB, int iWinner, DuelEndReason eReason);

private:
	std::vector<DuelChallengeCallback> m_vOnChallengeSent;
	std::vector<DuelConfirmedCallback> m_vOnChallengeConfirmed;
	std::vector<DuelStartCallback> m_vOnDuelStart;
	std::vector<DuelEndCallback> m_vOnDuelEnd;
};

extern IUtilsApi* g_pUtils;
extern IMenusApi* g_pMenus;
extern IPlayersApi* g_pPlayers;
extern ICookiesApi* g_pCookies;
extern IVEngineServer2* engine;
extern CGlobalVars* gpGlobals;
extern DuelApi* g_pDuelApi;

const char* Duel_Tr(const char* szKey);
void Duel_Chat(int iSlot, const char* szKey, ...);
void Duel_ChatAll(const char* szKey, ...);
void Duel_Center(int iSlot, const char* szKey, ...);

#endif //_INCLUDE_METAMOD_SOURCE_DUEL_PLUGIN_H_
