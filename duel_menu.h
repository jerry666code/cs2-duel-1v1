#pragma once

// Весь пользовательский интерфейс дуэльной системы построен исключительно на
// кнопочном движке ядра (IMenusApi / include/menus.h). Каждая функция ниже
// формирует Menu из Items ("кнопок") и показывает её игроку через
// g_pMenus->DisplayPlayerMenu.

namespace DuelMenu
{
	void ShowMainMenu(int iSlot);				// "Дуэль" -> вызвать/настройки/отменить
	void ShowTargetMenu(int iSlot);			// список игроков для вызова (кнопка на каждого)
	void ShowWeaponMenu(int iChallenger);		// выбор оружия (кнопки по оружию + "Случайное")
	void ShowArenaMenu(int iChallenger);		// выбор арены (кнопки по аренам + "Случайная")
	void ShowChallengeResponseMenu(int iTarget, int iChallenger); // кнопки "Принять"/"Отклонить"
	void ShowSettingsMenu(int iSlot);			// автопринятие: режим/порог здоровья/белый список
	void ShowHealthThresholdMenu(int iSlot);
	void ShowWhitelistMenu(int iSlot);
	void ShowCancelConfirmMenu(int iSlot);
}
