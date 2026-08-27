#pragma once

#include <functional>
#include <string>

// Публичный API ядра дуэлей 1 на 1.
// Подключается сторонними плагинами так же, как include/vip.h у VIP:
//   IDuelApi* g_pDuel = (IDuelApi*)g_SMAPI->MetaFactory(DUEL_INTERFACE, &ret, NULL);

#define DUEL_INTERFACE "IDuelApi"

enum class DuelEndReason : int
{
	Kill = 0,			// один из дуэлянтов убил другого
	Timeout,			// истекло время дуэли (тайм-аут)
	Disconnect,			// один из дуэлянтов отключился
	Cancelled,			// дуэль отменена вручную/через API
	Draw,				// ничья (оба погибли одновременно/оба живы по истечению времени)
	AbortedDuringPrep,	// прервана во время подготовки (например, смерть до старта)
	ForcedByRound		// принудительно прервана стартом следующего раунда
};

enum class DuelAutoAcceptMode : int
{
	Manual = 0,			// требуется подтверждение через кнопку
	Always,				// принимать любой вызов автоматически
	WhitelistOnly,		// автопринятие только от игроков из белого списка
	HealthThreshold,	// автопринятие только если здоровье выше заданного порога
	Never				// никогда не принимать (вызовы автоматически отклоняются)
};

// (challenger, target)
typedef std::function<void(int iChallenger, int iTarget)> DuelChallengeCallback;
// (challenger, target) -> вызывается, когда вызов принят и дуэль поставлена в очередь на конец раунда
typedef std::function<void(int iChallenger, int iTarget)> DuelConfirmedCallback;
// (slotA, slotB, szWeaponId, szArenaId) -> дуэлянты телепортированы и бой начался
typedef std::function<void(int iSlotA, int iSlotB, const char* szWeaponId, const char* szArenaId)> DuelStartCallback;
// (slotA, slotB, iWinner (-1 = ничья/нет), reason)
typedef std::function<void(int iSlotA, int iSlotB, int iWinner, DuelEndReason eReason)> DuelEndCallback;

class IDuelApi
{
public:
	// --- Состояние ---
	virtual bool Duel_IsInDuel(int iSlot) = 0;
	virtual bool Duel_IsChallengePending(int iSlot) = 0;
	virtual int  Duel_GetOpponent(int iSlot) = 0; // -1, если оппонента нет
	virtual int  Duel_GetActiveDuelCount() = 0;

	// --- Управление вызовами/дуэлями из кода других плагинов ---
	virtual bool Duel_SendChallenge(int iChallenger, int iTarget, const char* szWeaponId = nullptr, const char* szArenaId = nullptr) = 0;
	virtual bool Duel_AcceptChallenge(int iTarget) = 0;
	virtual bool Duel_DeclineChallenge(int iTarget) = 0;
	virtual bool Duel_CancelChallenge(int iSlot) = 0;
	virtual bool Duel_ForceEnd(int iSlot, DuelEndReason eReason = DuelEndReason::Cancelled) = 0;

	// --- Автопринятие ---
	virtual void Duel_SetAutoAcceptMode(int iSlot, DuelAutoAcceptMode eMode) = 0;
	virtual DuelAutoAcceptMode Duel_GetAutoAcceptMode(int iSlot) = 0;
	virtual void Duel_SetAutoAcceptHealthThreshold(int iSlot, int iMinHealthPercent) = 0;
	virtual void Duel_AddAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) = 0;
	virtual void Duel_RemoveAutoAcceptWhitelist(int iSlot, uint64 iSteamID64) = 0;
	virtual bool Duel_IsAutoAcceptWhitelisted(int iSlot, uint64 iSteamID64) = 0;

	// --- UI (кнопки) ---
	virtual void Duel_OpenMenu(int iSlot) = 0;
	virtual void Duel_OpenSettingsMenu(int iSlot) = 0;

	// --- Подписки на события ---
	virtual void Duel_OnChallengeSent(DuelChallengeCallback callback) = 0;
	virtual void Duel_OnChallengeConfirmed(DuelConfirmedCallback callback) = 0;
	virtual void Duel_OnDuelStart(DuelStartCallback callback) = 0;
	virtual void Duel_OnDuelEnd(DuelEndCallback callback) = 0;

	virtual const char* Duel_GetVersion() = 0;
};
