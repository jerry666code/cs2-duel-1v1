#pragma once

#include <array>
#include <map>
#include <vector>
#include <string>
#include "duel_types.h"

class CTakeDamageInfo;

class DuelManager
{
public:
	void Init();
	void Shutdown();
	void ReloadConfig();

	// --- Запросы состояния ---
	bool IsInDuel(int iSlot) const;
	bool HasPendingOutgoing(int iSlot) const;
	bool HasPendingIncoming(int iSlot) const;
	int GetOpponent(int iSlot) const;
	int GetActiveDuelCount() const { return (int)m_ActiveSessions.size(); }

	DuelSession* FindSessionBySlot(int iSlot);
	PendingChallenge* FindPendingByChallenger(int iSlot);
	PendingChallenge* FindPendingByTarget(int iSlot);

	// --- Поток вызова через меню (пошагово) ---
	bool BeginChallengeFlow(int iChallenger, int iTarget);
	void SetChallengeWeapon(int iChallenger, const std::string& szWeaponId);
	void SetChallengeArena(int iChallenger, const std::string& szArenaId);
	bool DispatchChallenge(int iChallenger); // отправляет вызов цели после выбора оружия/арены

	// --- Прямое API (используется IDuelApi/чат-командами) ---
	bool SendChallenge(int iChallenger, int iTarget, const std::string& szWeaponId, const std::string& szArenaId);
	bool AcceptChallenge(int iTarget);
	bool DeclineChallenge(int iTarget);
	bool CancelChallenge(int iSlot);
	bool ForceEnd(int iSlot, DuelEndReason eReason);

	// --- Автопринятие ---
	AutoAcceptSettings& GetAutoAccept(int iSlot);
	void SetAutoAcceptMode(int iSlot, DuelAutoAcceptMode eMode);
	void SetAutoAcceptHealthThreshold(int iSlot, int iPercent);
	void AddWhitelist(int iSlot, uint64 iSteamID64);
	void RemoveWhitelist(int iSlot, uint64 iSteamID64);
	bool IsWhitelisted(int iSlot, uint64 iSteamID64) const;

	// --- События движка ---
	void OnRoundEnd();
	void OnRoundPreStart();
	void OnPlayerDeath(int iVictim, int iAttacker);
	void OnPlayerDisconnect(int iSlot);
	void Tick(float flInterval);

	bool OnTakeDamagePre(int iVictimSlot, CTakeDamageInfo* pInfo);

private:
	bool IsPlayerEligible(int iSlot) const;
	void ConfirmChallenge(const PendingChallenge& c);
	void StartDuelSession(const PendingChallenge& c);
	void ActivateDuel(DuelSession& s);
	void EndDuelSession(DuelSession& s, int iWinnerSlot, DuelEndReason eReason);
	void AbortAllForRound();

	void SaveOriginalState(int iSlot);
	void RestoreOriginalState(int iSlot);
	void ApplyPreset(int iSlot, const DuelPreset& preset);
	void GiveDuelWeapon(int iSlot, const WeaponDef& weapon);
	void RunCommands(const std::vector<std::string>& vCommands);
	void LogDuel(const DuelSession& s, int iWinnerSlot, DuelEndReason eReason);
	const char* ReasonToString(DuelEndReason eReason) const;

	std::map<int, PendingChallenge> m_PendingByChallenger;
	std::vector<PendingChallenge> m_ConfirmedQueue;
	std::vector<DuelSession> m_ActiveSessions;

	std::array<SavedTransform, 64> m_SavedTransforms{};
	std::array<AutoAcceptSettings, 64> m_AutoAccept{};
	std::array<bool, 64> m_AutoAcceptInitialized{};
};

extern DuelManager g_DuelManager;
