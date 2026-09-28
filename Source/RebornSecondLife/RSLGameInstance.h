#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "RSLGameInstance.generated.h"

class USLSaveGameData;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSLLineEvent, const FString&, Speaker, const FString&, Text, bool, bFlashback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSLOptionsEvent, const TArray<FString>&, Options);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSLEndEvent, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSLStatsEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSLBattleGoEvent, const TArray<FString>&, EnemyIds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSLBattleLoseEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSLMapEvent, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSLTrainingDefeatEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSLMenuEvent, int32, Menu);

/** Состояние показа текущей сцены. */
UENUM()
enum class ESLStoryState : uint8
{
	Idle,           // ещё не начали
	AwaitingLine,   // ждём подтверждения (простая строка/наррация/флешбек/карточка главы)
	AwaitingChoice, // ждём выбора варианта (1..5)
	BattleIntro,    // показан вступительный текст боя, ждём подтверждения
	InBattle,       // идёт бой, враги живы
	BattleOutro,    // показан итог боя (win_text / lose_text)
	Ended           // финал
};

/**
 * Движок сюжета: читает Content/RS/Story/scenes.json (экспорт story_data.py),
 * проходит по сценам, применяет fx, ведёт статы, квесты и бой.
 */
UCLASS()
class URSLGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UPROPERTY(BlueprintAssignable) FSLLineEvent OnLine;
	UPROPERTY(BlueprintAssignable) FSLOptionsEvent OnOptions;
	UPROPERTY(BlueprintAssignable) FSLEndEvent OnEnd;
	UPROPERTY(BlueprintAssignable) FSLStatsEvent OnStats;
	UPROPERTY(BlueprintAssignable) FSLBattleGoEvent OnBattleGo;
	UPROPERTY(BlueprintAssignable) FSLBattleLoseEvent OnBattleLose;
	/** Открытие/закрытие карты мира (клавиша M). */
	UPROPERTY(BlueprintAssignable) FSLMapEvent OnMapChanged;
	/** Тренировка: HP достиг 1 — виджет показывает кровавую надпись «ВЫ ПРОИГРАЛИ». */
	UPROPERTY(BlueprintAssignable) FSLTrainingDefeatEvent OnTrainingDefeat;
	/** Панели «Герой» (G) и «Магия» (K): 0 — закрыты, 1 — герой, 2 — магия. */
	UPROPERTY(BlueprintAssignable) FSLMenuEvent OnMenuChanged;

	// --- состояние сюжета ---
	UPROPERTY() int32 SceneIndex = 0;
	UPROPERTY() ESLStoryState State = ESLStoryState::Idle;

	TArray<TSharedPtr<FJsonObject>> Scenes;
	TArray<TSharedPtr<FJsonObject>> Quests;
	TArray<TSharedPtr<FJsonObject>> Locations;
	TMap<FString, FString> RecordTitles;

	// текущий вход и варианты выбора
	TSharedPtr<FJsonObject> CurrentEntry;
	TArray<TSharedPtr<FJsonObject>> CurrentOptions;
	TSharedPtr<FJsonObject> PendingBattle;

	// --- статы героя ---
	UPROPERTY() float HP = 100.f, MaxHP = 100.f, MP = 50.f, MaxMP = 50.f;
	UPROPERTY() int32 XP = 0, Gold = 0, SP = 0, GuildPts = 0;
	/** Уровень и свободные очки характеристик (как в game.py: Level*120 XP на уровень). */
	UPROPERTY() int32 Level = 1;
	UPROPERTY() int32 StatPts = 0;

	UPROPERTY() TMap<FString, int32> Flags;
	UPROPERTY() TMap<FString, int32> Items;
	UPROPERTY() TMap<FString, int32> Trust;
	UPROPERTY() TMap<FString, int32> Stats;    // str, dex, int, vit, lck
	UPROPERTY() TMap<FString, int32> Kills;    // enemyId -> убито (квесты гильдии)
	UPROPERTY() TMap<FString, int32> Elements; // fire, water, ...

	UPROPERTY() TSet<FString> Spells;
	UPROPERTY() TSet<FString> Techs;
	UPROPERTY() TSet<FString> Styles;
	UPROPERTY() TSet<FString> Records;
	UPROPERTY() TSet<FString> Artifacts;
	UPROPERTY() TSet<FString> Unlocks;

	UPROPERTY() FString Ending;

	/** Текущая локация на карте (как p["loc"] в game.py), старт — деревня. */
	UPROPERTY() FString CurrentLoc = TEXT("village");

	// --- API ---
	bool WantsTextInput() const;
	void StartStory();
	void Advance();                       // подтверждение строки (Space/Enter)
	void SelectChoice(int32 Index);       // выбор варианта (1..5)
	void NotifyAllEnemiesDead();          // бой выигран
	void OnPlayerDamaged(float Dmg);
	void OnPlayerDeath();
	void OnEnemyKilled(const FString& EnemyId);
	void SaveProgress();
	bool LoadProgress();

	// --- карта мира (M) ---
	/** Открыть/закрыть карту. Недоступна в бою и до старта/после финала. */
	void ToggleMap();
	bool IsMapOpen() const { return bMapOpen; }
	/** Переезд в открытую локацию (кнопка «Отправиться сюда»). */
	void TravelTo(const FString& LocId);
	/** Принудительно закрыть карту (бой, финал). Безопасно, если карта закрыта. */
	void CloseMap();

	// --- панели «Герой» (G) и «Магия» (K) ---
	/** 0 — закрыты, 1 — герой, 2 — магия. Панель блокирует ввод сюжета, как карта. */
	void ToggleMenu(int32 Kind);
	/** Закрыть открытую панель (бой, финал). */
	void CloseMenu();
	int32 GetOpenMenu() const { return OpenMenu; }
	/** Потратить свободное очко характеристики (плюс в панели «Герой»). */
	void SpendStat(const FString& Key);
	/** Изучить заклинание за 1 очко навыка (проверка req/flag, как в game.py). */
	void LearnSpell(const FString& Id);
	/** Тренировка стихии: -1 очко навыка, +30 опыта стихии. */
	void TrainElement(const FString& Elem);
	/** Таблицы gamedata.json (заклинания, техники, ранги) — для панелей. */
	const TSharedPtr<FJsonObject>& GetGameData() const { return GameData; }
	/** Ранг гильдии по очкам (GUILD_NEEDS) — «ранг F»…«ранг SSS». */
	FString GuildTitle() const;
	/** Ранг героя по уровню (HERO_RANK_AT → RANKS). */
	FString HeroRankTitle() const;
	/** Ранг стихии: имя из RANKS по Elements[X] и порогам elem_xp. */
	FString ElemRankTitle(const FString& Elem) const;
	/** Следующий порог опыта для ранга стихии (для строки «30/150 до …»). */
	int32 ElemRankNeed(const FString& Elem) const;
	/** Максимум HP/MP по характеристикам (max_stats в game.py). */
	void RecalcMaxStats();

	// --- режим тренировки (train/outcome в узле боя) ---
	/** Текущий сюжетный бой — тренировка (исход всё равно каноничный). */
	bool IsTrainingBattle() const { return bTraining; }
	/** В тренировке с исходом 'lose' враг неубиваем — бой идёт до канонического финиша. */
	bool IsTrainingUnkillable() const;
	/** Принудительный канонический финиш тренировки (таймаут). */
	void ForceTrainingFinish();

	// --- аудио (как в pygame-версии: theme/scene/sad/battle/ending + sfx + голос) ---
	void PlaySFX(const FString& Name, float Vol = 0.6f);
	void PlayMusic(const FString& Kind);
	/** Подбирает трек по состоянию сюжета (пролог и mood:sad — «sad», бой — «battle»...). */
	void SyncMusic();
	void PlayVoice();
	void StopVoice();
	void ToggleVoice();

protected:
	void LoadStory();
	void ProcessCurrent();
	void ShowLine(const FString& Speaker, const FString& Text, bool bFlashback, bool bWithVoice = true);
	void ApplyFx(const TSharedPtr<FJsonObject>& Fx);
	/** Повышение уровня: XP >= Level*120 → Level+1, +3 очка характеристик, +1 очко навыка. */
	void LevelUpCheck();

	static FString GetStr(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& Def = TEXT(""));
	static int32 GetInt(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 Def = 0);

	bool bStoryLoaded = false;

	// --- режим тренировки (узел боя с 'train': True) ---
	/** Бой — тренировка: HP игрока не опускается ниже 1, исход задан сюжетом. */
	bool bTraining = false;
	/** Каноничный исход: 'win' (манекен) или 'lose' (спарринг с Полом). */
	FString TrainingOutcome;
	/** Кровавая надпись «ВЫ ПРОИГРАЛИ» уже показана в этом бою. */
	bool bTrainingDefeatShown = false;
	/** Принудительный финиш тренировки по таймауту (спарринг с Полом). */
	FTimerHandle TrainingTimerHandle;
	/** Сброс режима тренировки при выходе из боя (BattleOutro). */
	void ClearTrainingState();

	// --- аудио ---
	TMap<int32, FString> VoiceFiles;        // индекс сцены -> имя wav (voice_map.json)
	TWeakObjectPtr<UAudioComponent> MusicComp;
	TWeakObjectPtr<UAudioComponent> VoiceComp;
	FString CurrentMusicKind;
	int32 PrologEnd = 0;                    // индекс Главы 1: всё до него — пролог
	bool bVoiceOn = true;
	/** Таблицы gamedata.json (спells/techs/ranks) для панелей «Герой»/«Магия». */
	TSharedPtr<FJsonObject> GameData;
	/** Открытая панель: 0 — закрыта, 1 — «Герой», 2 — «Магия». */
	int32 OpenMenu = 0;
	bool bMapOpen = false;                  // карта мира открыта (блокирует ввод сюжета)
};
