#include <cstdlib>
#include <ctime>
#include <cmath>
#include "duel.h"
#include "duel_manager.h"
#include "duel_config.h"
#include "duel_menu.h"
#include "ctakedamageinfo.h"

DuelManager g_DuelManager;

static int DetermineWinnerByHealth(const DuelSession& s)
{
	CCSPlayerController* pA = CCSPlayerController::FromSlot(s.iSlotA);
	CCSPlayerController* pB = CCSPlayerController::FromSlot(s.iSlotB);
	CCSPlayerPawn* pawnA = pA ? pA->GetPlayerPawn() : nullptr;
	CCSPlayerPawn* pawnB = pB ? pB->GetPlayerPawn() : nullptr;
	int hpA = pawnA ? pawnA->m_iHealth() : 0;
	int hpB = pawnB ? pawnB->m_iHealth() : 0;
	if (hpA == hpB) return -1;
	return hpA > hpB ? s.iSlotA : s.iSlotB;
}

void DuelManager::Init()
{
	srand((unsigned int)time(nullptr));
}

void DuelManager::ReloadConfig()
{
	g_DuelConfig.LoadAll();
}

void DuelManager::Shutdown()
{
	m_PendingByChallenger.clear();
	m_ConfirmedQueue.clear();

	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); )
	{
		EndDuelSession(*it, -1, DuelEndReason::Cancelled);
		it = m_ActiveSessions.erase(it);
	}
}

bool DuelManager::IsInDuel(int iSlot) const
{
	for (auto& s : m_ActiveSessions)
		if (s.iSlotA == iSlot || s.iSlotB == iSlot) return true;
	return false;
}

bool DuelManager::HasPendingOutgoing(int iSlot) const
{
	return m_PendingByChallenger.find(iSlot) != m_PendingByChallenger.end();
}

bool DuelManager::HasPendingIncoming(int iSlot) const
{
	for (auto& [iChallenger, c] : m_PendingByChallenger)
		if (c.iTarget == iSlot && c.eState == ChallengeState::AwaitingResponse) return true;
	return false;
}

int DuelManager::GetOpponent(int iSlot) const
{
	for (auto& s : m_ActiveSessions)
	{
		if (s.iSlotA == iSlot) return s.iSlotB;
		if (s.iSlotB == iSlot) return s.iSlotA;
	}
	return -1;
}

DuelSession* DuelManager::FindSessionBySlot(int iSlot)
{
	for (auto& s : m_ActiveSessions)
		if (s.iSlotA == iSlot || s.iSlotB == iSlot) return &s;
	return nullptr;
}

PendingChallenge* DuelManager::FindPendingByChallenger(int iSlot)
{
	auto it = m_PendingByChallenger.find(iSlot);
	return it == m_PendingByChallenger.end() ? nullptr : &it->second;
}

PendingChallenge* DuelManager::FindPendingByTarget(int iSlot)
{
	for (auto& [iChallenger, c] : m_PendingByChallenger)
		if (c.iTarget == iSlot) return &c;
	return nullptr;
}

bool DuelManager::IsPlayerEligible(int iSlot) const
{
	if (iSlot < 0 || iSlot >= 64) return false;
	if (!g_pPlayers->IsConnected(iSlot) || !g_pPlayers->IsInGame(iSlot)) return false;
	if (g_pPlayers->IsFakeClient(iSlot)) return false;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController || !pController->m_bPawnIsAlive()) return false;
	if (IsInDuel(iSlot)) return false;
	return true;
}

bool DuelManager::BeginChallengeFlow(int iChallenger, int iTarget)
{
	if (iChallenger == iTarget)
	{
		Duel_Chat(iChallenger, "ErrorSelfChallenge");
		return false;
	}
	if (HasPendingOutgoing(iChallenger) || IsInDuel(iChallenger))
	{
		Duel_Chat(iChallenger, "ErrorAlreadyPending");
		return false;
	}
	if (!IsPlayerEligible(iChallenger))
	{
		Duel_Chat(iChallenger, "ErrorNotEligible");
		return false;
	}
	if (!IsPlayerEligible(iTarget) || HasPendingIncoming(iTarget))
	{
		Duel_Chat(iChallenger, "ErrorTargetNotEligible");
		return false;
	}

	PendingChallenge c;
	c.iChallenger = iChallenger;
	c.iTarget = iTarget;
	c.eState = ChallengeState::AwaitingWeaponPick;
	const DuelPreset* pDefault = g_DuelConfig.GetDefaultPreset();
	c.szPresetId = pDefault ? pDefault->szId : "";
	c.tCreatedAt = std::chrono::steady_clock::now();
	m_PendingByChallenger[iChallenger] = c;

	DuelMenu::ShowWeaponMenu(iChallenger);
	return true;
}

void DuelManager::SetChallengeWeapon(int iChallenger, const std::string& szWeaponId)
{
	auto it = m_PendingByChallenger.find(iChallenger);
	if (it == m_PendingByChallenger.end()) return;
	it->second.szWeaponId = szWeaponId;
	it->second.eState = ChallengeState::AwaitingArenaPick;
	DuelMenu::ShowArenaMenu(iChallenger);
}

void DuelManager::SetChallengeArena(int iChallenger, const std::string& szArenaId)
{
	auto it = m_PendingByChallenger.find(iChallenger);
	if (it == m_PendingByChallenger.end()) return;
	it->second.szArenaId = szArenaId;
	DispatchChallenge(iChallenger);
}

void DuelManager::ConfirmChallenge(const PendingChallenge& c)
{
	m_ConfirmedQueue.push_back(c);
	if (g_pDuelApi) g_pDuelApi->Fire_OnChallengeConfirmed(c.iChallenger, c.iTarget);

	const char* szA = g_pPlayers->GetPlayerName(c.iChallenger);
	const char* szB = g_pPlayers->GetPlayerName(c.iTarget);
	Duel_Chat(c.iChallenger, "DuelQueued", szA, szB);
	Duel_Chat(c.iTarget, "DuelQueued", szA, szB);
}

bool DuelManager::DispatchChallenge(int iChallenger)
{
	auto it = m_PendingByChallenger.find(iChallenger);
	if (it == m_PendingByChallenger.end()) return false;
	PendingChallenge& c = it->second;

	if (!IsPlayerEligible(c.iTarget))
	{
		Duel_Chat(iChallenger, "ErrorTargetNotEligible");
		m_PendingByChallenger.erase(it);
		return false;
	}

	if (g_pDuelApi) g_pDuelApi->Fire_OnChallengeSent(c.iChallenger, c.iTarget);

	const char* szTargetName = g_pPlayers->GetPlayerName(c.iTarget);
	const char* szChallengerName = g_pPlayers->GetPlayerName(c.iChallenger);
	Duel_Chat(c.iChallenger, "ChallengeSent", szTargetName);

	AutoAcceptSettings& settings = GetAutoAccept(c.iTarget);

	if (settings.eMode == DuelAutoAcceptMode::Never)
	{
		Duel_Chat(c.iChallenger, "ChallengeDeclined", szTargetName);
		m_PendingByChallenger.erase(it);
		return true;
	}

	bool bAuto = false;
	switch (settings.eMode)
	{
		case DuelAutoAcceptMode::Always:
			bAuto = true;
			break;
		case DuelAutoAcceptMode::WhitelistOnly:
			bAuto = settings.vWhitelist.count(g_pPlayers->GetSteamID64(c.iChallenger)) > 0;
			break;
		case DuelAutoAcceptMode::HealthThreshold:
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(c.iTarget);
			CCSPlayerPawn* pPawn = pController ? pController->GetPlayerPawn() : nullptr;
			if (pPawn)
			{
				int iMaxHp = pPawn->m_iMaxHealth() > 0 ? pPawn->m_iMaxHealth() : 100;
				int iPercent = (pPawn->m_iHealth() * 100) / iMaxHp;
				bAuto = iPercent >= settings.iMinHealthPercent;
			}
			break;
		}
		default:
			bAuto = false;
			break;
	}

	if (bAuto)
	{
		Duel_Chat(c.iChallenger, "ChallengeAutoAccepted", szTargetName);
		PendingChallenge copy = c;
		m_PendingByChallenger.erase(it);
		ConfirmChallenge(copy);
		return true;
	}

	c.eState = ChallengeState::AwaitingResponse;
	c.tCreatedAt = std::chrono::steady_clock::now();
	Duel_Chat(c.iTarget, "ChallengeReceived", szChallengerName);
	DuelMenu::ShowChallengeResponseMenu(c.iTarget, c.iChallenger);
	return true;
}

bool DuelManager::SendChallenge(int iChallenger, int iTarget, const std::string& szWeaponId, const std::string& szArenaId)
{
	if (!BeginChallengeFlow(iChallenger, iTarget)) return false;
	SetChallengeWeapon(iChallenger, szWeaponId);
	SetChallengeArena(iChallenger, szArenaId);
	return true;
}

bool DuelManager::AcceptChallenge(int iTarget)
{
	PendingChallenge* c = FindPendingByTarget(iTarget);
	if (!c || c->eState != ChallengeState::AwaitingResponse) return false;

	if (!IsPlayerEligible(c->iTarget) || !IsPlayerEligible(c->iChallenger))
	{
		int iChallenger = c->iChallenger;
		Duel_Chat(iTarget, "ErrorNotEligible");
		Duel_Chat(iChallenger, "ErrorTargetNotEligible");
		m_PendingByChallenger.erase(iChallenger);
		return false;
	}

	PendingChallenge copy = *c;
	m_PendingByChallenger.erase(copy.iChallenger);

	Duel_Chat(copy.iChallenger, "ChallengeAccepted", g_pPlayers->GetPlayerName(copy.iTarget));
	ConfirmChallenge(copy);
	return true;
}

bool DuelManager::DeclineChallenge(int iTarget)
{
	PendingChallenge* c = FindPendingByTarget(iTarget);
	if (!c) return false;

	int iChallenger = c->iChallenger;
	Duel_Chat(iChallenger, "ChallengeDeclined", g_pPlayers->GetPlayerName(iTarget));
	Duel_Chat(iTarget, "ChallengeDeclinedSelf", g_pPlayers->GetPlayerName(iChallenger));
	m_PendingByChallenger.erase(iChallenger);
	return true;
}

bool DuelManager::CancelChallenge(int iSlot)
{
	auto it = m_PendingByChallenger.find(iSlot);
	if (it != m_PendingByChallenger.end())
	{
		Duel_Chat(it->second.iTarget, "ChallengeCancelledByOpponent", g_pPlayers->GetPlayerName(iSlot));
		Duel_Chat(iSlot, "ChallengeCancelled");
		m_PendingByChallenger.erase(it);
		return true;
	}

	if (FindPendingByTarget(iSlot))
		return DeclineChallenge(iSlot);

	for (auto qit = m_ConfirmedQueue.begin(); qit != m_ConfirmedQueue.end(); ++qit)
	{
		if (qit->iChallenger == iSlot || qit->iTarget == iSlot)
		{
			int iOther = (qit->iChallenger == iSlot) ? qit->iTarget : qit->iChallenger;
			Duel_Chat(iOther, "ChallengeCancelledByOpponent", g_pPlayers->GetPlayerName(iSlot));
			Duel_Chat(iSlot, "ChallengeCancelled");
			m_ConfirmedQueue.erase(qit);
			return true;
		}
	}

	return false;
}

bool DuelManager::ForceEnd(int iSlot, DuelEndReason eReason)
{
	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); ++it)
	{
		if (it->iSlotA == iSlot || it->iSlotB == iSlot)
		{
			EndDuelSession(*it, -1, eReason);
			m_ActiveSessions.erase(it);
			return true;
		}
	}
	return false;
}

AutoAcceptSettings& DuelManager::GetAutoAccept(int iSlot)
{
	static AutoAcceptSettings dummy;
	if (iSlot < 0 || iSlot >= 64) return dummy;

	if (!m_AutoAcceptInitialized[iSlot])
	{
		m_AutoAccept[iSlot].eMode = g_DuelConfig.GetDefaultAutoAcceptMode();
		m_AutoAccept[iSlot].iMinHealthPercent = g_DuelConfig.GetDefaultHealthThreshold();
		m_AutoAcceptInitialized[iSlot] = true;
	}
	return m_AutoAccept[iSlot];
}

void DuelManager::SetAutoAcceptMode(int iSlot, DuelAutoAcceptMode eMode)
{
	GetAutoAccept(iSlot).eMode = eMode;
}

void DuelManager::SetAutoAcceptHealthThreshold(int iSlot, int iPercent)
{
	if (iPercent < 1) iPercent = 1;
	if (iPercent > 100) iPercent = 100;
	GetAutoAccept(iSlot).iMinHealthPercent = iPercent;
}

void DuelManager::AddWhitelist(int iSlot, uint64 iSteamID64)
{
	GetAutoAccept(iSlot).vWhitelist.insert(iSteamID64);
}

void DuelManager::RemoveWhitelist(int iSlot, uint64 iSteamID64)
{
	GetAutoAccept(iSlot).vWhitelist.erase(iSteamID64);
}

bool DuelManager::IsWhitelisted(int iSlot, uint64 iSteamID64) const
{
	if (iSlot < 0 || iSlot >= 64) return false;
	return m_AutoAccept[iSlot].vWhitelist.count(iSteamID64) > 0;
}

void DuelManager::RunCommands(const std::vector<std::string>& vCommands)
{
	for (auto& cmd : vCommands)
		engine->ServerCommand(cmd.c_str());
}

void DuelManager::SaveOriginalState(int iSlot)
{
	if (iSlot < 0 || iSlot >= 64) return;
	SavedTransform& t = m_SavedTransforms[iSlot];
	t.bValid = false;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn) return;

	t.bValid = true;
	t.vecOrigin = pPawn->GetAbsOrigin();
	t.angRotation = pPawn->GetAbsRotation();
	t.iHealth = pPawn->m_iHealth();
	t.iArmor = pPawn->m_ArmorValue();
}

void DuelManager::RestoreOriginalState(int iSlot)
{
	if (iSlot < 0 || iSlot >= 64) return;
	SavedTransform& t = m_SavedTransforms[iSlot];
	if (!t.bValid) return;

	Vector vecZero(0, 0, 0);
	g_pPlayers->Teleport(iSlot, &t.vecOrigin, &t.angRotation, &vecZero);

	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	CCSPlayerPawn* pPawn = pController ? pController->GetPlayerPawn() : nullptr;
	if (pPawn)
	{
		pPawn->m_iHealth(t.iHealth);
		g_pUtils->SetStateChanged(pPawn, "CBaseEntity", "m_iHealth");
		pPawn->m_ArmorValue(t.iArmor);
		g_pUtils->SetStateChanged(pPawn, "CCSPlayerPawn", "m_ArmorValue");
	}

	t.bValid = false;
}

void DuelManager::ApplyPreset(int iSlot, const DuelPreset& preset)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn) return;

	pPawn->m_iHealth(preset.iMaxHealth);
	g_pUtils->SetStateChanged(pPawn, "CBaseEntity", "m_iHealth");

	if (pPawn->m_pItemServices())
	{
		if (preset.bHelmet)
			pPawn->m_pItemServices()->GiveNamedItem("item_assaultsuit");
		else if (preset.iArmor > 0)
			pPawn->m_pItemServices()->GiveNamedItem("item_kevlar");
	}

	pPawn->m_ArmorValue(preset.iArmor);
	g_pUtils->SetStateChanged(pPawn, "CCSPlayerPawn", "m_ArmorValue");
}

void DuelManager::GiveDuelWeapon(int iSlot, const WeaponDef& weapon)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn || !pPawn->m_pItemServices()) return;

	pPawn->m_pItemServices()->GiveNamedItem(weapon.szClassname.c_str());
	for (auto& szItem : weapon.vExtraItems)
		pPawn->m_pItemServices()->GiveNamedItem(szItem.c_str());
}

void DuelManager::StartDuelSession(const PendingChallenge& c)
{
	if (!IsPlayerEligible(c.iChallenger) || !IsPlayerEligible(c.iTarget))
	{
		for (int iSlot : { c.iChallenger, c.iTarget })
			if (g_pPlayers->IsConnected(iSlot) && g_pPlayers->IsInGame(iSlot))
				Duel_Chat(iSlot, "ErrorNotEligible");
		return;
	}

	const DuelPreset* pPreset = g_DuelConfig.FindPreset(c.szPresetId);
	if (!pPreset) pPreset = g_DuelConfig.GetDefaultPreset();

	std::string szArenaId = c.szArenaId;
	if (pPreset && pPreset->bFixedArena) szArenaId = pPreset->szFixedArenaId;
	const ArenaDef* pArena = g_DuelConfig.PickArena(szArenaId);

	std::string szWeaponId = c.szWeaponId;
	if (pPreset && pPreset->bRandomWeapon) szWeaponId = "random";
	const WeaponDef* pWeapon = g_DuelConfig.PickWeapon(szWeaponId);

	if (!pArena)
	{
		Duel_Chat(c.iChallenger, "ErrorNoArenas");
		Duel_Chat(c.iTarget, "ErrorNoArenas");
		return;
	}
	if (!pWeapon)
	{
		Duel_Chat(c.iChallenger, "ErrorNoWeapons");
		Duel_Chat(c.iTarget, "ErrorNoWeapons");
		return;
	}

	SaveOriginalState(c.iChallenger);
	SaveOriginalState(c.iTarget);

	DuelSession s;
	s.iSlotA = c.iChallenger;
	s.iSlotB = c.iTarget;
	s.szWeaponId = pWeapon->szId;
	s.szArenaId = pArena->szId;
	s.szPresetId = pPreset ? pPreset->szId : "";
	s.ePhase = DuelPhase::Preparing;
	s.flTimeLeft = pPreset ? pPreset->flPrepTime : 5.0f;
	s.tStartedAt = std::chrono::steady_clock::now();

	m_ActiveSessions.push_back(s);

	RunCommands(pPreset ? pPreset->vPreCommands : std::vector<std::string>());

	Vector vecZero(0, 0, 0);
	g_pPlayers->RemoveWeapons(s.iSlotA);
	g_pPlayers->RemoveWeapons(s.iSlotB);
	g_pPlayers->Teleport(s.iSlotA, &pArena->hSpawnA.vecOrigin, &pArena->hSpawnA.angRotation, &vecZero);
	g_pPlayers->Teleport(s.iSlotB, &pArena->hSpawnB.vecOrigin, &pArena->hSpawnB.angRotation, &vecZero);

	Duel_Center(s.iSlotA, "PrepCountdown", (int)ceil(s.flTimeLeft));
	Duel_Center(s.iSlotB, "PrepCountdown", (int)ceil(s.flTimeLeft));
}

void DuelManager::ActivateDuel(DuelSession& s)
{
	const DuelPreset* pPreset = g_DuelConfig.FindPreset(s.szPresetId);
	if (!pPreset) pPreset = g_DuelConfig.GetDefaultPreset();
	const WeaponDef* pWeapon = g_DuelConfig.FindWeapon(s.szWeaponId);

	for (int iSlot : { s.iSlotA, s.iSlotB })
	{
		if (!g_pPlayers->IsConnected(iSlot) || !g_pPlayers->IsInGame(iSlot)) continue;
		if (pPreset) ApplyPreset(iSlot, *pPreset);
		if (pWeapon) GiveDuelWeapon(iSlot, *pWeapon);
	}

	s.ePhase = DuelPhase::Active;
	s.flTimeLeft = pPreset ? pPreset->flDuelDuration : 60.0f;
	s.tStartedAt = std::chrono::steady_clock::now();

	const char* szWeaponName = pWeapon ? pWeapon->szDisplayName.c_str() : "";
	Duel_Center(s.iSlotA, "DuelStart", szWeaponName);
	Duel_Center(s.iSlotB, "DuelStart", szWeaponName);

	if (g_pDuelApi) g_pDuelApi->Fire_OnDuelStart(s.iSlotA, s.iSlotB, s.szWeaponId.c_str(), s.szArenaId.c_str());
}

const char* DuelManager::ReasonToString(DuelEndReason eReason) const
{
	switch (eReason)
	{
		case DuelEndReason::Kill: return "kill";
		case DuelEndReason::Timeout: return "timeout";
		case DuelEndReason::Disconnect: return "disconnect";
		case DuelEndReason::Cancelled: return "cancelled";
		case DuelEndReason::Draw: return "draw";
		case DuelEndReason::AbortedDuringPrep: return "aborted_prep";
		case DuelEndReason::ForcedByRound: return "forced_by_round";
	}
	return "unknown";
}

void DuelManager::LogDuel(const DuelSession& s, int iWinnerSlot, DuelEndReason eReason)
{
	char szTime[32];
	time_t now = time(nullptr);
	strftime(szTime, sizeof(szTime), "%Y-%m-%d %H:%M:%S", localtime(&now));

	const char* szA = g_pPlayers->GetPlayerName(s.iSlotA);
	const char* szB = g_pPlayers->GetPlayerName(s.iSlotB);
	const char* szWinner = (iWinnerSlot == s.iSlotA) ? szA : (iWinnerSlot == s.iSlotB) ? szB : "none";

	g_pUtils->LogToFile("addons/logs/duel/duels.log",
		"[%s] %s vs %s | weapon=%s arena=%s preset=%s | winner=%s | reason=%s",
		szTime, szA, szB, s.szWeaponId.c_str(), s.szArenaId.c_str(), s.szPresetId.c_str(), szWinner, ReasonToString(eReason));
}

void DuelManager::EndDuelSession(DuelSession& s, int iWinnerSlot, DuelEndReason eReason)
{
	const DuelPreset* pPreset = g_DuelConfig.FindPreset(s.szPresetId);
	RunCommands(pPreset ? pPreset->vPostCommands : std::vector<std::string>());

	bool bRestorePos = pPreset ? pPreset->bRestoreOriginalPosition : true;

	int slots[2] = { s.iSlotA, s.iSlotB };
	for (int i = 0; i < 2; ++i)
	{
		int iSlot = slots[i];
		int iOpponent = slots[1 - i];
		if (iSlot < 0 || !g_pPlayers->IsConnected(iSlot) || !g_pPlayers->IsInGame(iSlot))
			continue;

		const char* szOpponentName = (iOpponent >= 0) ? g_pPlayers->GetPlayerName(iOpponent) : "";

		switch (eReason)
		{
			case DuelEndReason::Kill:
			case DuelEndReason::Timeout:
				if (iWinnerSlot == iSlot) Duel_Chat(iSlot, "DuelWin", szOpponentName);
				else if (iWinnerSlot == iOpponent) Duel_Chat(iSlot, "DuelLose", szOpponentName);
				else Duel_Chat(iSlot, "DuelDraw", szOpponentName);
				break;
			case DuelEndReason::Disconnect:
				if (iSlot == iWinnerSlot) Duel_Chat(iSlot, "DuelOpponentDisconnected");
				break;
			case DuelEndReason::AbortedDuringPrep:
				Duel_Chat(iSlot, "DuelAbortedPrep");
				break;
			case DuelEndReason::ForcedByRound:
				Duel_Chat(iSlot, "DuelForcedByRound");
				break;
			default:
				Duel_Chat(iSlot, "DuelForceEndedAdmin");
				break;
		}

		if (bRestorePos)
			RestoreOriginalState(iSlot);
	}

	LogDuel(s, iWinnerSlot, eReason);

	if (g_pDuelApi) g_pDuelApi->Fire_OnDuelEnd(s.iSlotA, s.iSlotB, iWinnerSlot, eReason);
}

void DuelManager::AbortAllForRound()
{
	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); )
	{
		EndDuelSession(*it, -1, DuelEndReason::ForcedByRound);
		it = m_ActiveSessions.erase(it);
	}
}

void DuelManager::OnRoundEnd()
{
	if (m_ConfirmedQueue.empty()) return;
	std::vector<PendingChallenge> queue = std::move(m_ConfirmedQueue);
	m_ConfirmedQueue.clear();
	for (auto& c : queue)
		StartDuelSession(c);
}

void DuelManager::OnRoundPreStart()
{
	AbortAllForRound();
}

void DuelManager::OnPlayerDeath(int iVictim, int iAttacker)
{
	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); ++it)
	{
		DuelSession& s = *it;
		if (s.iSlotA != iVictim && s.iSlotB != iVictim) continue;

		int iOpponent = (s.iSlotA == iVictim) ? s.iSlotB : s.iSlotA;

		if (s.ePhase == DuelPhase::Preparing)
			EndDuelSession(s, -1, DuelEndReason::AbortedDuringPrep);
		else
			EndDuelSession(s, iOpponent, DuelEndReason::Kill);

		m_ActiveSessions.erase(it);
		return;
	}
}

void DuelManager::OnPlayerDisconnect(int iSlot)
{
	auto pit = m_PendingByChallenger.find(iSlot);
	if (pit != m_PendingByChallenger.end())
	{
		Duel_Chat(pit->second.iTarget, "ChallengeCancelledByOpponent", g_pPlayers->GetPlayerName(iSlot));
		m_PendingByChallenger.erase(pit);
	}
	else
	{
		PendingChallenge* pIncoming = FindPendingByTarget(iSlot);
		if (pIncoming)
		{
			int iChallenger = pIncoming->iChallenger;
			Duel_Chat(iChallenger, "ChallengeDeclined", g_pPlayers->GetPlayerName(iSlot));
			m_PendingByChallenger.erase(iChallenger);
		}
	}

	for (auto qit = m_ConfirmedQueue.begin(); qit != m_ConfirmedQueue.end(); ++qit)
	{
		if (qit->iChallenger == iSlot || qit->iTarget == iSlot)
		{
			int iOther = (qit->iChallenger == iSlot) ? qit->iTarget : qit->iChallenger;
			Duel_Chat(iOther, "ChallengeCancelledByOpponent", g_pPlayers->GetPlayerName(iSlot));
			m_ConfirmedQueue.erase(qit);
			break;
		}
	}

	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); ++it)
	{
		if (it->iSlotA == iSlot || it->iSlotB == iSlot)
		{
			int iWinner = (it->iSlotA == iSlot) ? it->iSlotB : it->iSlotA;
			EndDuelSession(*it, iWinner, DuelEndReason::Disconnect);
			m_ActiveSessions.erase(it);
			break;
		}
	}
}

void DuelManager::Tick(float flInterval)
{
	for (auto it = m_ActiveSessions.begin(); it != m_ActiveSessions.end(); )
	{
		DuelSession& s = *it;

		if (s.ePhase == DuelPhase::Preparing)
		{
			s.flTimeLeft -= flInterval;
			if (s.flTimeLeft > 0.0f)
			{
				Duel_Center(s.iSlotA, "PrepCountdown", (int)ceil(s.flTimeLeft));
				Duel_Center(s.iSlotB, "PrepCountdown", (int)ceil(s.flTimeLeft));
				++it;
				continue;
			}
			ActivateDuel(s);
			++it;
			continue;
		}

		// DuelPhase::Active
		const DuelPreset* pPreset = g_DuelConfig.FindPreset(s.szPresetId);
		if (pPreset && pPreset->flRegenPerSecond > 0.0f)
		{
			for (int iSlot : { s.iSlotA, s.iSlotB })
			{
				CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
				CCSPlayerPawn* pPawn = pController ? pController->GetPlayerPawn() : nullptr;
				if (!pPawn) continue;
				int iNewHealth = pPawn->m_iHealth() + (int)(pPreset->flRegenPerSecond * flInterval);
				if (iNewHealth > pPreset->iMaxHealth) iNewHealth = pPreset->iMaxHealth;
				if (iNewHealth != pPawn->m_iHealth())
				{
					pPawn->m_iHealth(iNewHealth);
					g_pUtils->SetStateChanged(pPawn, "CBaseEntity", "m_iHealth");
				}
			}
		}

		if (pPreset && pPreset->flDuelDuration > 0.0f)
		{
			s.flTimeLeft -= flInterval;
			if (s.flTimeLeft <= 0.0f)
			{
				int iWinner = DetermineWinnerByHealth(s);
				EndDuelSession(s, iWinner, DuelEndReason::Timeout);
				it = m_ActiveSessions.erase(it);
				continue;
			}
		}

		++it;
	}
}

bool DuelManager::OnTakeDamagePre(int iVictimSlot, CTakeDamageInfo* pInfo)
{
	DuelSession* s = FindSessionBySlot(iVictimSlot);
	if (!s || s->ePhase != DuelPhase::Active) return true;

	int iOpponent = (s->iSlotA == iVictimSlot) ? s->iSlotB : s->iSlotA;

	const AttackerInfo_t& hAttackerInfo = pInfo->m_AttackerInfo();
	if (hAttackerInfo.m_bNeedInit)
		return true; // источник урона ещё не разрешён движком - не блокируем и не трогаем это попадание

	if ((int)hAttackerInfo.m_nAttackerPlayerSlot != iOpponent)
		return false; // во время дуэли урон принимается только от назначенного соперника

	const WeaponDef* pWeapon = g_DuelConfig.FindWeapon(s->szWeaponId);
	if (pWeapon && pWeapon->flDamageMultiplier != 1.0f)
		pInfo->m_flDamage(pInfo->m_flDamage() * pWeapon->flDamageMultiplier);

	return true;
}
