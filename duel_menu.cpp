#include <cstdio>
#include <cstdlib>
#include "duel.h"
#include "duel_menu.h"
#include "duel_manager.h"
#include "duel_config.h"

static bool IsSelectablePlayer(int iSlot, int iExcludeSlot)
{
	if (iSlot == iExcludeSlot) return false;
	if (!g_pPlayers->IsConnected(iSlot) || !g_pPlayers->IsInGame(iSlot)) return false;
	if (g_pPlayers->IsFakeClient(iSlot)) return false;
	return true;
}

static void MainMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	std::string sKey(szBack);
	if (sKey == "challenge") DuelMenu::ShowTargetMenu(iSlot);
	else if (sKey == "settings") DuelMenu::ShowSettingsMenu(iSlot);
	else if (sKey == "cancel") DuelMenu::ShowCancelConfirmMenu(iSlot);
}

void DuelMenu::ShowMainMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("MainTitle"));

	bool bBusy = g_DuelManager.IsInDuel(iSlot) || g_DuelManager.HasPendingOutgoing(iSlot) || g_DuelManager.HasPendingIncoming(iSlot);

	if (g_DuelManager.IsInDuel(iSlot))
		g_pMenus->AddItemMenu(hMenu, "status", Duel_Tr("MenuStatusInDuel"), ITEM_DISABLED);
	else if (g_DuelManager.HasPendingOutgoing(iSlot) || g_DuelManager.HasPendingIncoming(iSlot))
		g_pMenus->AddItemMenu(hMenu, "status", Duel_Tr("MenuStatusPending"), ITEM_DISABLED);

	g_pMenus->AddItemMenu(hMenu, "challenge", Duel_Tr("MenuChallenge"), bBusy ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "settings", Duel_Tr("MenuSettings"), ITEM_DEFAULT);
	if (bBusy)
		g_pMenus->AddItemMenu(hMenu, "cancel", Duel_Tr("MenuCancel"), ITEM_DEFAULT);

	g_pMenus->SetBackMenu(hMenu, false);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, MainMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void TargetMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	int iTarget = atoi(szBack);
	g_DuelManager.BeginChallengeFlow(iSlot, iTarget);
}

void DuelMenu::ShowTargetMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("TargetTitle"));

	int iCount = 0;
	for (int i = 0; i < 64; ++i)
	{
		if (!IsSelectablePlayer(i, iSlot)) continue;
		char szKey[8];
		snprintf(szKey, sizeof(szKey), "%d", i);
		g_pMenus->AddItemMenu(hMenu, szKey, g_pMenus->escapeString(g_pPlayers->GetPlayerName(i)).c_str());
		++iCount;
	}

	if (iCount == 0)
	{
		Duel_Chat(iSlot, "NoTargets");
		return;
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, TargetMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void WeaponMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	g_DuelManager.SetChallengeWeapon(iSlot, std::string(szBack));
}

void DuelMenu::ShowWeaponMenu(int iChallenger)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("WeaponTitle"));
	g_pMenus->AddItemMenu(hMenu, "random", Duel_Tr("WeaponRandom"));

	auto vWeapons = g_DuelConfig.GetSelectableWeapons();
	if (vWeapons.empty())
	{
		Duel_Chat(iChallenger, "ErrorNoWeapons");
		return;
	}

	for (auto* pWeapon : vWeapons)
		g_pMenus->AddItemMenu(hMenu, pWeapon->szId.c_str(), g_pMenus->escapeString(pWeapon->szDisplayName).c_str());

	g_pMenus->SetBackMenu(hMenu, false);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, WeaponMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iChallenger, true, true);
}

static void ArenaMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	g_DuelManager.SetChallengeArena(iSlot, std::string(szBack));
}

void DuelMenu::ShowArenaMenu(int iChallenger)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("ArenaTitle"));
	g_pMenus->AddItemMenu(hMenu, "random", Duel_Tr("ArenaRandom"));

	if (g_DuelConfig.GetArenas().empty())
	{
		Duel_Chat(iChallenger, "ErrorNoArenas");
		return;
	}

	for (auto& [id, arena] : g_DuelConfig.GetArenas())
		g_pMenus->AddItemMenu(hMenu, arena.szId.c_str(), g_pMenus->escapeString(arena.szDisplayName).c_str());

	g_pMenus->SetBackMenu(hMenu, false);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, ArenaMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iChallenger, true, true);
}

static void ResponseMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	std::string sKey(szBack);
	if (sKey == "accept") g_DuelManager.AcceptChallenge(iSlot);
	else if (sKey == "decline") g_DuelManager.DeclineChallenge(iSlot);
}

void DuelMenu::ShowChallengeResponseMenu(int iTarget, int iChallenger)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("ResponseTitle"));

	char szInfo[192];
	snprintf(szInfo, sizeof(szInfo), Duel_Tr("ResponseInfo"), g_pPlayers->GetPlayerName(iChallenger));
	g_pMenus->AddItemMenu(hMenu, "info", szInfo, ITEM_DISABLED);

	g_pMenus->AddItemMenu(hMenu, "accept", Duel_Tr("Accept"));
	g_pMenus->AddItemMenu(hMenu, "decline", Duel_Tr("Decline"));

	g_pMenus->SetBackMenu(hMenu, false);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, ResponseMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iTarget, true, true);
}

static void SettingsMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	std::string sKey(szBack);
	if (sKey == "mode_manual") g_DuelManager.SetAutoAcceptMode(iSlot, DuelAutoAcceptMode::Manual);
	else if (sKey == "mode_always") g_DuelManager.SetAutoAcceptMode(iSlot, DuelAutoAcceptMode::Always);
	else if (sKey == "mode_whitelist") g_DuelManager.SetAutoAcceptMode(iSlot, DuelAutoAcceptMode::WhitelistOnly);
	else if (sKey == "mode_health") g_DuelManager.SetAutoAcceptMode(iSlot, DuelAutoAcceptMode::HealthThreshold);
	else if (sKey == "mode_never") g_DuelManager.SetAutoAcceptMode(iSlot, DuelAutoAcceptMode::Never);
	else if (sKey == "threshold") { DuelMenu::ShowHealthThresholdMenu(iSlot); return; }
	else if (sKey == "whitelist") { DuelMenu::ShowWhitelistMenu(iSlot); return; }

	DuelMenu::ShowSettingsMenu(iSlot);
}

void DuelMenu::ShowSettingsMenu(int iSlot)
{
	AutoAcceptSettings& settings = g_DuelManager.GetAutoAccept(iSlot);

	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("SettingsTitle"));

	g_pMenus->AddItemMenu(hMenu, "mode_manual", Duel_Tr("ModeManual"), settings.eMode == DuelAutoAcceptMode::Manual ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "mode_always", Duel_Tr("ModeAlways"), settings.eMode == DuelAutoAcceptMode::Always ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "mode_whitelist", Duel_Tr("ModeWhitelist"), settings.eMode == DuelAutoAcceptMode::WhitelistOnly ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "mode_health", Duel_Tr("ModeHealth"), settings.eMode == DuelAutoAcceptMode::HealthThreshold ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "mode_never", Duel_Tr("ModeNever"), settings.eMode == DuelAutoAcceptMode::Never ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenus->AddItemMenu(hMenu, "threshold", Duel_Tr("HealthThresholdTitle"));
	g_pMenus->AddItemMenu(hMenu, "whitelist", Duel_Tr("WhitelistTitle"));

	g_pMenus->SetBackMenu(hMenu, false);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, SettingsMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void HealthThresholdMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	g_DuelManager.SetAutoAcceptHealthThreshold(iSlot, atoi(szBack));
	DuelMenu::ShowSettingsMenu(iSlot);
}

void DuelMenu::ShowHealthThresholdMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("HealthThresholdTitle"));

	const int vPercents[] = { 25, 50, 75, 100 };
	for (int p : vPercents)
	{
		char szKey[8], szText[16];
		snprintf(szKey, sizeof(szKey), "%d", p);
		snprintf(szText, sizeof(szText), "%d%%", p);
		g_pMenus->AddItemMenu(hMenu, szKey, szText);
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, HealthThresholdMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void WhitelistMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	int iTarget = atoi(szBack);
	if (iTarget < 0 || iTarget >= 64) return;

	uint64 iSteamID64 = g_pPlayers->GetSteamID64(iTarget);
	if (iSteamID64 == 0) return;

	if (g_DuelManager.IsWhitelisted(iSlot, iSteamID64))
		g_DuelManager.RemoveWhitelist(iSlot, iSteamID64);
	else
		g_DuelManager.AddWhitelist(iSlot, iSteamID64);

	DuelMenu::ShowWhitelistMenu(iSlot);
}

void DuelMenu::ShowWhitelistMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("WhitelistTitle"));

	int iCount = 0;
	for (int i = 0; i < 64; ++i)
	{
		if (!IsSelectablePlayer(i, iSlot)) continue;

		uint64 iSteamID64 = g_pPlayers->GetSteamID64(i);
		bool bListed = iSteamID64 != 0 && g_DuelManager.IsWhitelisted(iSlot, iSteamID64);

		char szKey[8];
		snprintf(szKey, sizeof(szKey), "%d", i);
		char szText[192];
		snprintf(szText, sizeof(szText), Duel_Tr(bListed ? "WhitelistRemove" : "WhitelistAdd"), g_pPlayers->GetPlayerName(i));
		g_pMenus->AddItemMenu(hMenu, szKey, szText);
		++iCount;
	}

	if (iCount == 0)
	{
		Duel_Chat(iSlot, "NoTargets");
		return;
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, WhitelistMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void CancelConfirmMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
	if (std::string(szBack) == "yes")
	{
		if (!g_DuelManager.CancelChallenge(iSlot) && !g_DuelManager.ForceEnd(iSlot, DuelEndReason::Cancelled))
			Duel_Chat(iSlot, "NothingToCancel");
	}
}

void DuelMenu::ShowCancelConfirmMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, Duel_Tr("CancelConfirmTitle"));
	g_pMenus->AddItemMenu(hMenu, "yes", Duel_Tr("CancelConfirmYes"));
	g_pMenus->AddItemMenu(hMenu, "no", Duel_Tr("CancelConfirmNo"));

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, CancelConfirmMenuCallback);
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}
