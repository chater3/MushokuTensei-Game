#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/EngineTypes.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"
#include "RSLStoryWidget.generated.h"

class STextBlock;
class SVerticalBox;
class SHorizontalBox;
class SBorder;
class SButton;
class SImage;
class SProgressBar;
class FJsonObject;
class UTexture2D;

/**
 * Виджет сюжета: реплики, выбор, HUD-статы, карточки глав и финал.
 * Строится полностью на Slate в RebuildWidget — Blueprint не требуется.
 */
UCLASS()
class URSLStoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION() void HandleLine(const FString& Speaker, const FString& Text, bool bFlashback);
	UFUNCTION() void HandleOptions(const TArray<FString>& Options);
	UFUNCTION() void HandleEnd(const FString& Text);
	UFUNCTION() void HandleStats();
	/** Карта мира открыта/закрыта (клавиша M). */
	UFUNCTION() void HandleMap(bool bOpen);
	/** Тренировка: HP достиг 1 — кровавая надпись «ВЫ ПРОИГРАЛИ» поверх экрана. */
	UFUNCTION() void HandleTrainingDefeat();
	/** Панель «Герой» (G) / «Магия» (K): Menu 0 — закрыть, 1 — герой, 2 — магия. */
	UFUNCTION() void HandleMenu(int32 Menu);

	/** Панель сюжета скрывается на время боя и возвращается с win/lose-текстом. */
	void SetInBattle(bool bInBattle);

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FReply OnOptionClicked(int32 Index);
	void ClearOptions();
	/** Переключает режим ввода: текстовые сцены — курсор/кнопки, бой — только игра. */
	void SetTextUI(bool bTextUI);

	// --- боевой HUD (имя врага, полоса HP, подсказка управления) ---
	/** Строки врагов под текущий состав боя (пересобираются при смене врагов). */
	void RebuildBattleRows(const TArray<FString>& DisplayNames);
	/** Каждый кадр в бою: HP живых врагов, счётчик строк. */
	void UpdateBattleHUD();

	// --- карта мира ---
	/** Список открытых локаций (заново при каждом открытии карты). */
	void RebuildMapList();
	/** Панель описания выбранной локации (опасность, флаги, «Вы здесь»). */
	void UpdateMapInfo();
	FReply OnTravelClicked();

	// --- панели «Герой» (G) и «Магия» (K) ---
	/** Пересобирает содержимое открытой панели (статы/заклинания/стихии). */
	void RebuildMenu();
	/** Одна строка характеристики с кнопкой «+» (очко тратится в GI). */
	TSharedRef<SWidget> MakeStatRow(const FString& Key, const FString& Name, int32 Value, int32 FreePts);

	TSharedPtr<STextBlock> SpeakerText;
	TSharedPtr<STextBlock> BodyText;
	TSharedPtr<STextBlock> StatsText;
	TSharedPtr<STextBlock> ChapterText;
	/** Кровавая надпись «ВЫ ПРОИГРАЛИ» (режим тренировки, HP = 1). */
	TSharedPtr<STextBlock> DefeatText;
	/** Подсказка «Тренировка» сверху во время учебного боя. */
	TSharedPtr<STextBlock> TrainHint;
	TSharedPtr<SVerticalBox> OptionsBox;
	TSharedPtr<SBorder> Panel;

	// --- боевой HUD ---
	TSharedPtr<SBorder> BattlePanel;
	TSharedPtr<SVerticalBox> BattleRows;
	TSharedPtr<STextBlock> BattleHint;
	/** Прицел по центру экрана — только во время боя. */
	TSharedPtr<STextBlock> Crosshair;
	/** Строки текущего боя: имя, «HP сейчас/макс», полоса. */
	TArray<TSharedPtr<STextBlock>> BattleNames;
	TArray<TSharedPtr<STextBlock>> BattleHPText;
	TArray<TSharedPtr<SProgressBar>> BattleBars;
	/** Состав боя, для которого собраны строки (id врагов через «|»). */
	FString BattleKey;
	/** true, пока идёт бой — обновляет HUD. */
	uint8 bInBattleMode : 1;
	FSlateBrush BattleBarFill;
	FSlateBrush BattleBarBack;

	/** Скрыть кровавую надпись через несколько секунд. */
	void HideDefeatText();
	FTimerHandle DefeatHideHandle;

	// --- фон локации (скачанные текстуры) ---
	/** Подменяет фон текущей локацией (T_<id>); повторный вызов при смене локации. */
	void UpdateBackdrop();
	/** Карточка главы/финал — фон на весь экран; реплики/бой — 3D-мир без фона. */
	void SetBackdropVisible(bool bVisible);
	TSharedPtr<SImage> BackdropImage;
	FSlateBrush BackdropBrush;
	/** UPROPERTY-ссылка: удерживает текстуру фона в памяти (иначе GC → краш Slate). */
	UPROPERTY(Transient)
	UTexture2D* BackdropTexture = nullptr;
	FString BackdropLocId;

	// карта
	TSharedPtr<SBorder> MapPanel;
	TSharedPtr<SVerticalBox> MapList;
	TSharedPtr<STextBlock> MapTitle;
	TSharedPtr<STextBlock> MapInfoText;
	TSharedPtr<SButton> TravelButton;
	FString SelectedLocId;

	// панели «Герой» (G) / «Магия» (K)
	TSharedPtr<SBorder> MenuPanel;
	TSharedPtr<SVerticalBox> MenuBox;
	TSharedPtr<STextBlock> MenuTitle;
};
