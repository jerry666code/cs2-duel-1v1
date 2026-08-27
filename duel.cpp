#include <stdio.h>
#include <cstdarg>
#include "duel.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "duel_config.h"
#include "duel_manager.h"
#include "duel_menu.h"

Duel g_Duel;
PLUGIN_EXPOSE(Duel, g_Duel);

IVEngineServer2* engine = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars* gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;
IMenusApi* g_pMenus = nullptr;
IPlayersApi* g_pPlayers = nullptr;
ICookiesApi* g_pCookies = nullptr;

DuelApi* g_pDuelApi = nullptr;
IDuelApi* g_pDuelCore = nullptr;

static std::map<std::string, std::string> g_DuelPhrases;
static CTimer* g_pDuelTickTimer = nullptr;

const char* Duel_Tr(const char* szKey)
{
	auto it = g_DuelPhrases.find(szKey);
	return it == g_DuelPhrases.end() ? szKey : it->second.c_str();
}

void Duel_Chat(int iSlot, const char* szKey, ...)
{
	char buf[512];
	va_list args;
	va_start(args, szKey);
	V_vsnprintf(buf, sizeof(buf), Duel_Tr(szKey), args);
	va_end(args);
	g_pUtils->PrintToChat(iSlot, "%s", buf);
}

void Duel_ChatAll(const char* szKey, ...)
{
	char buf[512];
	va_list args;
	va_start(args, szKey);
	V_vsnprintf(buf, sizeof(buf), Duel_Tr(szKey), args);
	va_end(args);
	g_pUtils->PrintToChatAll("%s", buf);
}

void Duel_Center(int iSlot, const char* szKey, ...)
{
	char buf[512];
	va_list args;
	va_start(args, szKey);
	V_vsnprintf(buf, sizeof(buf), Duel_Tr(szKey), args);
	va_end(args);
	g_pUtils->PrintToCenter(iSlot, "%s", buf);
}

static bool LoadDuelPhrases()
{
	g_DuelPhrases.clear();

	KeyValues* pKV = new KeyValues("Phrases");
	const char* pszPath = "addons/translations/duel.phrases.txt";
	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
		pKV->deleteThis();
		return false;
	}

	const char* szLang = g_pUtils->GetLanguage();
	for (KeyValues* pKey = pKV->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_DuelPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(szLang, pKey->GetString("en", "")));

	pKV->deleteThis();
	return true;
}

// --- Команды ---

static bool OnDuelCommand(int iSlot, const char* szContent)
{
	DuelMenu::ShowMainMenu(iSlot);
	return false;
}

static bool OnDuelSettingsCommand(int iSlot, const char* szContent)
{
	DuelMenu::ShowSettingsMenu(iSlot);
	return false;
}

static bool OnDuelCancelCommand(int iSlot, const char* szContent)
{
	if (!g_DuelManager.CancelChallenge(iSlot) && !g_DuelManager.ForceEnd(iSlot, DuelEndReason::Cancelled))
		Duel_Chat(iSlot, "NothingToCancel");
	return false;
}

static bool OnDuelAcceptCommand(int iSlot, const char* szContent)
{
	g_DuelManager.AcceptChallenge(iSlot);
	return false;
}

static bool OnDuelDeclineCommand(int iSlot, const char* szContent)
{
	g_DuelManager.DeclineChallenge(iSlot);
	return false;
}

static void OnStartupServer()
{
	static bool bDone = false;
	if (bDone) return;
	bDone = true;

	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

CON_COMMAND_F(duel_reload, "reload duel weapons/arenas/presets/autoaccept configs", FCVAR_NONE)
{
	if (g_DuelConfig.LoadAll())
		ConColorMsg({ 0, 255, 0, 255 }, "[Duel] Configs reloaded\n");
	else
		ConColorMsg({ 255, 0, 0, 255 }, "[Duel] Configs reloaded with errors, check the log\n");
}

// --- Хуки событий ---

static void OnRoundEnd(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	g_DuelManager.OnRoundEnd();
}

static void OnRoundPreStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	g_DuelManager.OnRoundPreStart();
}

static void OnPlayerDeath(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	CBasePlayerController* pVictim = static_cast<CBasePlayerController*>(pEvent->GetPlayerController("userid"));
	CBasePlayerController* pAttacker = static_cast<CBasePlayerController*>(pEvent->GetPlayerController("attacker"));
	if (!pVictim) return;

	int iVictim = pVictim->GetPlayerSlot();
	int iAttacker = pAttacker ? pAttacker->GetPlayerSlot() : -1;
	g_DuelManager.OnPlayerDeath(iVictim, iAttacker);
}

static void OnPlayerDisconnect(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	CBasePlayerController* pPlayer = static_cast<CBasePlayerController*>(pEvent->GetPlayerController("userid"));
	if (!pPlayer) return;
	g_DuelManager.OnPlayerDisconnect(pPlayer->GetPlayerSlot());
}

static bool OnTakeDamagePre(int iSlot, CTakeDamageInfo* pInfo)
{
	return g_DuelManager.OnTakeDamagePre(iSlot, pInfo);
}

void* Duel::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, DUEL_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pDuelCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

bool Duel::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener(this, this);

	ConVar_Register(FCVAR_GAMEDLL);

	g_pDuelApi = new DuelApi();
	g_pDuelCore = g_pDuelApi;

	return true;
}

bool Duel::Unload(char* error, size_t maxlen)
{
	g_DuelManager.Shutdown();
	if (g_pUtils)
	{
		if (g_pDuelTickTimer)
		{
			g_pUtils->RemoveTimer(g_pDuelTickTimer);
			g_pDuelTickTimer = nullptr;
		}
		g_pUtils->ClearAllHooks(g_PLID);
	}
	ConVar_Unregister();
	return true;
}

void Duel::AllPluginsLoaded()
{
	int ret;
	g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] Missing Utils system plugin\n", GetLogTag());
		std::string sBuffer = "meta unload " + std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}

	g_pMenus = (IMenusApi*)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Menus system plugin", GetLogTag());
		std::string sBuffer = "meta unload " + std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}

	g_pPlayers = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Players system plugin", GetLogTag());
		std::string sBuffer = "meta unload " + std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}

	g_pCookies = (ICookiesApi*)g_SMAPI->MetaFactory(COOKIES_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
		g_pCookies = nullptr; // необязательная зависимость: без неё автопринятие не переживёт реконнект

	g_pUtils->StartupServer(g_PLID, OnStartupServer);

	if (!LoadDuelPhrases())
		return;
	if (!g_DuelConfig.LoadAll())
		g_pUtils->ErrorLog("[%s] Some duel configs failed to load, see errors above", GetLogTag());

	g_DuelManager.Init();

	g_pUtils->RegCommand(g_PLID, { "mm_duel", "sm_duel" }, { "!duel" }, OnDuelCommand);
	g_pUtils->RegCommand(g_PLID, { "mm_duelsettings", "sm_duelsettings" }, { "!duelsettings" }, OnDuelSettingsCommand);
	g_pUtils->RegCommand(g_PLID, { "mm_duelcancel", "sm_duelcancel" }, { "!duelcancel" }, OnDuelCancelCommand);
	g_pUtils->RegCommand(g_PLID, { "mm_duelaccept", "sm_duelaccept" }, { "!duelaccept" }, OnDuelAcceptCommand);
	g_pUtils->RegCommand(g_PLID, { "mm_dueldecline", "sm_dueldecline" }, { "!dueldecline" }, OnDuelDeclineCommand);

	g_pUtils->HookEvent(g_PLID, "round_end", OnRoundEnd);
	g_pUtils->HookEvent(g_PLID, "round_prestart", OnRoundPreStart);
	g_pUtils->HookEvent(g_PLID, "player_death", OnPlayerDeath);
	g_pUtils->HookEvent(g_PLID, "player_disconnect", OnPlayerDisconnect);

	g_pUtils->HookOnTakeDamagePre(g_PLID, OnTakeDamagePre);

	g_pDuelTickTimer = g_pUtils->CreateTimer(1.0f, []() -> float {
		g_DuelManager.Tick(1.0f);
		return 1.0f;
	});
}

const char* Duel::GetLicense() { return "GPL"; }
const char* Duel::GetVersion() { return "1.0.0"; }
const char* Duel::GetDate() { return __DATE__; }
const char* Duel::GetLogTag() { return "Duel"; }
const char* Duel::GetAuthor() { return "Server Team"; }
const char* Duel::GetDescription() { return "[Duel] 1v1 end-of-round duel core"; }
const char* Duel::GetName() { return "[Duel] Core"; }
const char* Duel::GetURL() { return ""; }

// --- IDuelApi ---

bool DuelApi::Duel_IsInDuel(int iSlot) { return g_DuelManager.IsInDuel(iSlot); }
bool DuelApi::Duel_IsChallengePending(int iSlot) { return g_DuelManager.HasPendingOutgoing(iSlot) || g_DuelManager.HasPendingIncoming(iSlot); }
int DuelApi::Duel_GetOpponent(int iSlot) { return g_DuelManager.GetOpponent(iSlot); }
int DuelApi::Duel_GetActiveDuelCount() { return g_DuelManager.GetActiveDuelCount(); }

bool DuelApi::Duel_SendChallenge(int iChallenger, int iTarget, const char* szWeaponId, const char* szArenaId)
{
	return g_DuelManager.SendChallenge(iChallenger, iTarget, szWeaponId ? szWeaponId : "", szArenaId ? szArenaId : "");
}
bool DuelApi::Duel_AcceptChallenge(int iTarget) { return g_DuelManager.AcceptChallenge(iTarget); }
bool DuelApi::Duel_DeclineChallenge(int iTarget) { return g_DuelManager.DeclineChallenge(iTarget); }
bool DuelApi::Duel_CancelChallenge(int iSlot) { return g_DuelManager.CancelChallenge(iSlot); }
bool DuelApi::Duel_ForceEnd(int iSlot, DuelEndReason eReason) { return g_DuelManager.ForceEnd(iSlot, eReason); }

void DuelApi::Duel_SetAutoAcceptMode(int iSlot, DuelAutoAcceptMode eMode) { g_DuelManager.SetAutoAcceptMode(iSlot, eMode); }
DuelAutoAcceptMode DuelApi::Duel_GetAutoAcceptMode(int iSlot) { return g_DuelManager.GetAutoAccept(iSlot).eMode; }
void DuelApi::Duel_SetAutoAcceptHealthThreshold(int iSlot, int iMinHealthPercent) { g_DuelManager.SetAutoAcceptHealthThreshold(iSlot, iMinHealthPercent); }
void DuelApi::Duel_AddAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) { g_DuelManager.AddWhitelist(iSlot, iSteamID64); }
void DuelApi::Duel_RemoveAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) { g_DuelManager.RemoveWhitelist(iSlot, iSteamID64); }
bool DuelApi::Duel_IsAutoAcceptWhitelisted(int iSlot, uint64 iSteamID64) { return g_DuelManager.IsWhitelisted(iSlot, iSteamID64); }

void DuelApi::Duel_OpenMenu(int iSlot) { DuelMenu::ShowMainMenu(iSlot); }
void DuelApi::Duel_OpenSettingsMenu(int iSlot) { DuelMenu::ShowSettingsMenu(iSlot); }

const char* DuelApi::Duel_GetVersion() { return "1.0.0"; }

void DuelApi::Fire_OnChallengeSent(int iChallenger, int iTarget)
{
	for (auto& cb : m_vOnChallengeSent) if (cb) cb(iChallenger, iTarget);
}
void DuelApi::Fire_OnChallengeConfirmed(int iChallenger, int iTarget)
{
	for (auto& cb : m_vOnChallengeConfirmed) if (cb) cb(iChallenger, iTarget);
}
void DuelApi::Fire_OnDuelStart(int iSlotA, int iSlotB, const char* szWeaponId, const char* szArenaId)
{
	for (auto& cb : m_vOnDuelStart) if (cb) cb(iSlotA, iSlotB, szWeaponId, szArenaId);
}
void DuelApi::Fire_OnDuelEnd(int iSlotA, int iSlotB, int iWinner, DuelEndReason eReason)
{
	for (auto& cb : m_vOnDuelEnd) if (cb) cb(iSlotA, iSlotB, iWinner, eReason);
}
