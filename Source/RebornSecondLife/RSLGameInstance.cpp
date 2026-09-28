#include "RSLGameInstance.h"
#include "RSLSaveGame.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRSLStory, Log, All);

namespace
{
	TSharedPtr<FJsonObject> AsObj(const TSharedPtr<FJsonValue>& V)
	{
		return (V.IsValid() && V->Type == EJson::Object) ? V->AsObject() : nullptr;
	}
}

void URSLGameInstance::Init()
{
	Super::Init();

	// Стартовые характеристики как в game.py (новая игра), если сейва ещё нет.
	if (!Stats.Contains(TEXT("str")))
	{
		Stats.Add(TEXT("str"), 5);
		Stats.Add(TEXT("dex"), 6);
		Stats.Add(TEXT("int"), 8);
		Stats.Add(TEXT("vit"), 5);
		Stats.Add(TEXT("lck"), 5);
	}

	LoadStory();
}

void URSLGameInstance::LoadStory()
{
	Scenes.Reset();
	Quests.Reset();
	Locations.Reset();
	RecordTitles.Reset();

	const FString Dir = FPaths::ProjectContentDir() / TEXT("RS/Story");

	// scenes.json — верхнеуровневый массив
	{
		FString Data;
		if (FFileHelper::LoadFileToString(Data, *(Dir / TEXT("scenes.json"))))
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			if (FJsonSerializer::Deserialize(Reader, Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : Arr)
				{
					TSharedPtr<FJsonObject> O = AsObj(V);
					if (O) { Scenes.Add(O); }
				}
			}
		}
	}

	// quests.json / locations.json — массивы
	auto LoadArray = [&Dir](const TCHAR* FileName, TArray<TSharedPtr<FJsonObject>>& Out)
	{
		FString Data;
		if (!FFileHelper::LoadFileToString(Data, *(Dir / FileName))) { return; }
		TArray<TSharedPtr<FJsonValue>> Arr;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
		if (!FJsonSerializer::Deserialize(Reader, Arr)) { return; }
		for (const TSharedPtr<FJsonValue>& V : Arr)
		{
			TSharedPtr<FJsonObject> O = AsObj(V);
			if (O) { Out.Add(O); }
		}
	};
	LoadArray(TEXT("quests.json"), Quests);
	LoadArray(TEXT("locations.json"), Locations);

	// record_titles.json — объект
	{
		FString Data;
		if (FFileHelper::LoadFileToString(Data, *(Dir / TEXT("record_titles.json"))))
		{
			TSharedPtr<FJsonObject> Obj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				for (const auto& KV : Obj->Values)
				{
					RecordTitles.Add(KV.Key, KV.Value->AsString());
				}
			}
		}
	}

	// gamedata.json — таблицы панелей «Герой» (G) и «Магия» (K).
	{
		GameData.Reset();
		FString Data;
		if (FFileHelper::LoadFileToString(Data, *(Dir / TEXT("gamedata.json"))))
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			TSharedPtr<FJsonObject> Obj;
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				GameData = Obj;
			}
		}
	}

	bStoryLoaded = Scenes.Num() > 0;

	// Граница пролога: индекс сцены "ГЛАВА 1" — всё до неё играет под «sad».
	PrologEnd = Scenes.Num();
	for (int32 i = 0; i < Scenes.Num(); ++i)
	{
		if (GetStr(Scenes[i], TEXT("t")) == TEXT("ch") && GetInt(Scenes[i], TEXT("num")) == 1)
		{
			PrologEnd = i;
			break;
		}
	}

	// voice_map.json: индекс сцены -> имя wav-файла озвучки (make_voice.py).
	VoiceFiles.Reset();
	{
		FString Data;
		if (FFileHelper::LoadFileToString(Data, *(Dir / TEXT("voice_map.json"))))
		{
			TSharedPtr<FJsonObject> Obj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				for (const auto& KV : Obj->Values)
				{
					VoiceFiles.Add(FCString::Atoi(*KV.Key), KV.Value->AsString());
				}
			}
		}
	}

	UE_LOG(LogRSLStory, Log, TEXT("Сюжет загружен: сцен=%d, квестов=%d, локаций=%d, голосов=%d, пролог до %d"),
		Scenes.Num(), Quests.Num(), Locations.Num(), VoiceFiles.Num(), PrologEnd);
}

bool URSLGameInstance::WantsTextInput() const
{
	if (bMapOpen || OpenMenu != 0) { return false; } // карта/панели блокируют ввод сюжета
	return State == ESLStoryState::AwaitingLine
		|| State == ESLStoryState::AwaitingChoice
		|| State == ESLStoryState::BattleIntro
		|| State == ESLStoryState::BattleOutro;
}

void URSLGameInstance::StartStory()
{
	if (State != ESLStoryState::Idle) { return; }
	if (!bStoryLoaded)
	{
		State = ESLStoryState::Ended;
		OnEnd.Broadcast(TEXT("Файл сюжета не найден"));
		return;
	}
	if (!LoadProgress())
	{
		SceneIndex = 0;
		Unlocks.Reset();
		Unlocks.Add(TEXT("village")); // новая игра стартует из деревни (как в game.py)
		CurrentLoc = TEXT("village");
	}
	ProcessCurrent();
	SyncMusic();
}

void URSLGameInstance::ProcessCurrent()
{
	// защёлка от зацикливания скрытых fx и пустых карточек
	int32 Guard = 0;
	while (SceneIndex < Scenes.Num() && Guard++ < 1000)
	{
		const TSharedPtr<FJsonObject> E = Scenes[SceneIndex];
		const FString T = GetStr(E, TEXT("t"));

		if (T == TEXT("e")) // скрытый эффект — сразу применяем
		{
			if (const TSharedPtr<FJsonObject>* Fx; E->TryGetObjectField(TEXT("fx"), Fx)) { ApplyFx(*Fx); }
			++SceneIndex;
			continue;
		}

		if (T == TEXT("end"))
		{
			State = ESLStoryState::Ended;
			FString Kind = GetStr(E, TEXT("kind"));
			if (Kind.IsEmpty()) { Kind = Ending; }
			SyncMusic(); // тема финала
			OnEnd.Broadcast(Kind);
			return;
		}

		if (T == TEXT("ch"))
		{
			const int32 Num = GetInt(E, TEXT("num"));
			const FString Title = GetStr(E, TEXT("title"));
			const FString Sub = GetStr(E, TEXT("sub"));
			PlaySFX(TEXT("chapter"), 0.5f);
			ShowLine(TEXT(""), FString::Printf(TEXT("ГЛАВА %d\n%s\n%s"), Num, *Title, *Sub), false);
			State = ESLStoryState::AwaitingLine;
			SaveProgress(); // автосейв на границе глав
			return;
		}

		if (T == TEXT("n") || T == TEXT("f"))
		{
			ShowLine(TEXT(""), GetStr(E, TEXT("text")), T == TEXT("f"));
			State = ESLStoryState::AwaitingLine;
			return;
		}

		if (T == TEXT("s"))
		{
			ShowLine(GetStr(E, TEXT("who")), GetStr(E, TEXT("text")), false);
			State = ESLStoryState::AwaitingLine;
			return;
		}

		if (T == TEXT("c")) // выбор
		{
			CurrentOptions.Reset();
			if (const TArray<TSharedPtr<FJsonValue>>* Opts; E->TryGetArrayField(TEXT("opts"), Opts))
			{
				for (const TSharedPtr<FJsonValue>& V : *Opts)
				{
					TSharedPtr<FJsonObject> O = AsObj(V);
					if (O) { CurrentOptions.Add(O); }
				}
			}
			ShowLine(TEXT(""), GetStr(E, TEXT("text")), false);
			TArray<FString> Labels;
			for (const TSharedPtr<FJsonObject>& O : CurrentOptions)
			{
				FString Label = GetStr(O, TEXT("label"));
				const FString Note = GetStr(O, TEXT("note"));
				if (!Note.IsEmpty()) { Label += FString::Printf(TEXT(" (%s)"), *Note); }
				Labels.Add(Label);
			}
			OnOptions.Broadcast(Labels);
			State = ESLStoryState::AwaitingChoice;
			return;
		}

		if (T == TEXT("b")) // бой
		{
			PendingBattle = E;

			// Тренировка (train/outcome): исход каноничный, HP не падает ниже 1.
			bTraining = false;
			TrainingOutcome.Reset();
			bTrainingDefeatShown = false;
			bool bTrainFlag = false;
			if (E->TryGetBoolField(TEXT("train"), bTrainFlag) && bTrainFlag)
			{
				bTraining = true;
				TrainingOutcome = GetStr(E, TEXT("outcome"));
				if (TrainingOutcome.IsEmpty()) { TrainingOutcome = TEXT("win"); }
			}

			ShowLine(TEXT(""), GetStr(E, TEXT("intro")), false);
			State = ESLStoryState::BattleIntro;
			return;
		}

		// неизвестный тип — пропускаем
		++SceneIndex;
	}

	State = ESLStoryState::Ended;
	SyncMusic();
	OnEnd.Broadcast(Ending.IsEmpty() ? TEXT("КОНЕЦ") : Ending);
}

void URSLGameInstance::Advance()
{
	if (bMapOpen) { return; } // пока открыта карта — сюжет не двигаем

	if (WantsTextInput())
	{
		PlaySFX(TEXT("click"), 0.45f);
	}

	switch (State)
	{
	case ESLStoryState::AwaitingLine:
		++SceneIndex;
		ProcessCurrent();
		break;

	case ESLStoryState::BattleIntro:
	{
		StopVoice();
		State = ESLStoryState::InBattle;
		TArray<FString> Ids;
		if (PendingBattle.IsValid())
		{
			if (const TArray<TSharedPtr<FJsonValue>>* Arr; PendingBattle->TryGetArrayField(TEXT("enemies"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr) { Ids.Add(V->AsString()); }
			}
		}
		if (Ids.Num() == 0) { Ids.Add(TEXT("goblin")); }
		OnBattleGo.Broadcast(Ids);

		// Тренировка с исходом 'lose' (спарринг с Полом): враг неубиваем,
		// по таймауту — принудительный канонический финиш боя.
		if (bTraining && TrainingOutcome == TEXT("lose"))
		{
			if (UWorld* W = GetWorld())
			{
				W->GetTimerManager().SetTimer(TrainingTimerHandle, this,
					&URSLGameInstance::ForceTrainingFinish, 90.f, false);
			}
		}
		break;
	}

	case ESLStoryState::BattleOutro:
		ClearTrainingState();
		PendingBattle.Reset();
		++SceneIndex;
		ProcessCurrent();
		break;

	default:
		break;
	}

	SyncMusic();
}

void URSLGameInstance::SelectChoice(int32 Index)
{
	if (bMapOpen) { return; }
	if (State != ESLStoryState::AwaitingChoice) { return; }
	if (!CurrentOptions.IsValidIndex(Index)) { return; }

	PlaySFX(TEXT("click"), 0.45f);

	const TSharedPtr<FJsonObject> Opt = CurrentOptions[Index];
	if (const TSharedPtr<FJsonObject>* Fx; Opt->TryGetObjectField(TEXT("fx"), Fx)) { ApplyFx(*Fx); }

	CurrentOptions.Reset();
	++SceneIndex;
	ProcessCurrent();
	SyncMusic();
}

void URSLGameInstance::NotifyAllEnemiesDead()
{
	if (State != ESLStoryState::InBattle || !PendingBattle.IsValid()) { return; }
	PlaySFX(TEXT("win"), 0.6f);
	ShowLine(TEXT(""), GetStr(PendingBattle, TEXT("win_text")), false, /*голоса для win_text нет*/ false);
	State = ESLStoryState::BattleOutro;
	SyncMusic();
}

void URSLGameInstance::OnPlayerDamaged(float Dmg)
{
	if (State == ESLStoryState::Ended) { return; }

	// Тренировка: игрок не умирает — HP замирает на 1.
	// При достижении 1 — кровавая надпись «ВЫ ПРОИГРАЛИ», но бой и сюжет продолжаются.
	if (bTraining)
	{
		HP = FMath::Max(1.f, HP - Dmg);
		OnStats.Broadcast();
		if (HP <= 1.f && !bTrainingDefeatShown)
		{
			bTrainingDefeatShown = true;
			OnTrainingDefeat.Broadcast();

			// Исход 'lose' (спарринг с Полом): после надписи бой завершается по канону,
			// чтобы игрок успел прочитать «ВЫ ПРОИГРАЛИ».
			if (TrainingOutcome == TEXT("lose"))
			{
				if (UWorld* W = GetWorld())
				{
					W->GetTimerManager().SetTimer(TrainingTimerHandle, this,
						&URSLGameInstance::ForceTrainingFinish, 3.5f, false);
				}
			}
		}
		return;
	}

	HP = FMath::Max(0.f, HP - Dmg);
	if (HP <= 0.f)
	{
		OnPlayerDeath();
	}
	else
	{
		OnStats.Broadcast();
	}
}

bool URSLGameInstance::IsTrainingUnkillable() const
{
	return bTraining && State == ESLStoryState::InBattle && TrainingOutcome == TEXT("lose");
}

void URSLGameInstance::ForceTrainingFinish()
{
	if (State != ESLStoryState::InBattle || !bTraining || !PendingBattle.IsValid()) { return; }

	// Каноничный исход спарринга: Рудеус проигрывает, сюжет идёт дальше.
	if (!bTrainingDefeatShown)
	{
		bTrainingDefeatShown = true;
		OnTrainingDefeat.Broadcast();
	}
	PlaySFX(TEXT("lose"), 0.6f);
	OnBattleLose.Broadcast();
	HP = FMath::Max(1.f, HP);
	OnStats.Broadcast();
	ShowLine(TEXT(""), GetStr(PendingBattle, TEXT("lose_text")), false, /*голоса для lose_text нет*/ false);
	State = ESLStoryState::BattleOutro;
	SyncMusic();
}

void URSLGameInstance::ClearTrainingState()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(TrainingTimerHandle);
	}
	bTraining = false;
	TrainingOutcome.Reset();
	bTrainingDefeatShown = false;
}

void URSLGameInstance::OnPlayerDeath()
{
	const bool bStoryBattle = (State == ESLStoryState::InBattle)
		&& PendingBattle.IsValid()
		&& !GetStr(PendingBattle, TEXT("lose_text")).IsEmpty();

	if (bStoryBattle)
	{
		// сюжетный бой с проигрышем: реплика lose_text, без повтора
		PlaySFX(TEXT("lose"), 0.6f);
		OnBattleLose.Broadcast();
		HP = 1.f;
		ShowLine(TEXT(""), GetStr(PendingBattle, TEXT("lose_text")), false, /*голоса для lose_text нет*/ false);
		State = ESLStoryState::BattleOutro;
		SyncMusic();
	}
	else
	{
		PlaySFX(TEXT("lose"), 0.6f);
		HP = MaxHP; // обычная смерть: возрождение со полным здоровьем
		OnStats.Broadcast();
	}
}

void URSLGameInstance::OnEnemyKilled(const FString& EnemyId)
{
	const int32 Prev = Kills.FindRef(EnemyId);
	Kills.Add(EnemyId, Prev + 1);
	OnStats.Broadcast();
}

void URSLGameInstance::ShowLine(const FString& Speaker, const FString& Text, bool bFlashback, bool bWithVoice)
{
	OnLine.Broadcast(Speaker, Text, bFlashback);

	// Озвучка текущей реплики (файл есть не у каждой сцены — тогда просто тишина).
	if (bWithVoice && VoiceFiles.Contains(SceneIndex))
	{
		PlayVoice();
	}
	else
	{
		StopVoice();
	}

	OnStats.Broadcast();
}

void URSLGameInstance::ApplyFx(const TSharedPtr<FJsonObject>& Fx)
{
	if (!Fx.IsValid()) { return; }

	bool bNeedLevelCheck = false;

	auto Num = [](const TSharedPtr<FJsonValue>& V) { return V.IsValid() ? V->AsNumber() : 0.0; };
	auto Str = [](const TSharedPtr<FJsonValue>& V) { return V.IsValid() ? V->AsString() : FString(); };

	for (const auto& KV : Fx->Values)
	{
		const FString& K = KV.Key;
		const TSharedPtr<FJsonValue>& V = KV.Value;

		if (K == TEXT("spell")) { Spells.Add(Str(V)); }
		else if (K == TEXT("tech")) { Techs.Add(Str(V)); }
		else if (K == TEXT("style")) { Styles.Add(Str(V)); }
		else if (K == TEXT("xp")) { XP += FMath::RoundToInt(Num(V)); bNeedLevelCheck = true; }
		else if (K == TEXT("gold")) { Gold = FMath::Max(0, Gold + FMath::RoundToInt(Num(V))); }
		else if (K == TEXT("hp")) { HP = FMath::Clamp(HP + (float)Num(V), 1.f, MaxHP); }
		else if (K == TEXT("mp")) { MP = FMath::Clamp(MP + (float)Num(V), 0.f, MaxMP); }
		else if (K == TEXT("sp")) { SP += FMath::RoundToInt(Num(V)); }
		else if (K == TEXT("guild_pts")) { GuildPts += FMath::RoundToInt(Num(V)); }
		else if (K == TEXT("flag")) { Flags.Add(Str(V), 1); }
		else if (K == TEXT("record")) { Records.Add(Str(V)); }
		else if (K == TEXT("artifact")) { Artifacts.Add(Str(V)); }
		else if (K == TEXT("ending")) { Ending = Str(V); }
		else if (K == TEXT("stat"))
		{
			TSharedPtr<FJsonObject> Obj = AsObj(V);
			if (Obj) { for (const auto& S : Obj->Values) { Stats.Add(S.Key, Stats.FindRef(S.Key) + (int32)S.Value->AsNumber()); } }
		}
		else if (K == TEXT("trust"))
		{
			TSharedPtr<FJsonObject> Obj = AsObj(V);
			if (Obj) { for (const auto& S : Obj->Values) { Trust.Add(S.Key, Trust.FindRef(S.Key) + (int32)S.Value->AsNumber()); } }
		}
		else if (K == TEXT("items"))
		{
			TSharedPtr<FJsonObject> Obj = AsObj(V);
			if (Obj) { for (const auto& S : Obj->Values) { Items.Add(S.Key, Items.FindRef(S.Key) + (int32)S.Value->AsNumber()); } }
		}
		else if (K == TEXT("elem"))
		{
			TSharedPtr<FJsonObject> Obj = AsObj(V);
			if (Obj) { for (const auto& S : Obj->Values) { Elements.Add(S.Key, Elements.FindRef(S.Key) + (int32)S.Value->AsNumber()); } }
		}
		else if (K == TEXT("flags") || K == TEXT("unlock"))
		{
			if (const TArray<TSharedPtr<FJsonValue>>* Arr; V.IsValid() && V->TryGetArray(Arr))
			{
				for (const TSharedPtr<FJsonValue>& Item : *Arr)
				{
					if (K == TEXT("flags")) { Flags.Add(Item->AsString(), 1); }
					else { Unlocks.Add(Item->AsString()); }
				}
			}
		}
	}

	if (bNeedLevelCheck)
	{
		LevelUpCheck();
	}

	OnStats.Broadcast();
}

// ---------------- Уровень ----------------

void URSLGameInstance::LevelUpCheck()
{
	// Как в game.py: пока XP >= Level*120 — уровень вверх, +3 очка характеристик, +1 очко навыка.
	bool bLeveled = false;
	while (XP >= Level * 120)
	{
		XP -= Level * 120;
		++Level;
		StatPts += 3;
		++SP;
		HP = MaxHP;
		MP = MaxMP;
		bLeveled = true;
	}
	if (bLeveled)
	{
		PlaySFX(TEXT("level"), 0.6f);
	}
}

FString URSLGameInstance::GetStr(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& Def)
{
	FString Out;
	if (Obj.IsValid() && Obj->TryGetStringField(Field, Out)) { return Out; }
	return Def;
}

int32 URSLGameInstance::GetInt(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 Def)
{
	double Out;
	if (Obj.IsValid() && Obj->TryGetNumberField(Field, Out)) { return (int32)Out; }
	return Def;
}

void URSLGameInstance::SaveProgress()
{
	USLSaveGameData* Save = Cast<USLSaveGameData>(
		UGameplayStatics::CreateSaveGameObject(USLSaveGameData::StaticClass()));
	if (!Save) { return; }

	Save->SceneIndex = SceneIndex;
	Save->HP = HP; Save->MaxHP = MaxHP; Save->MP = MP; Save->MaxMP = MaxMP;
	Save->XP = XP; Save->Gold = Gold; Save->SP = SP; Save->GuildPts = GuildPts;
	Save->Level = Level; Save->StatPts = StatPts;
	Save->Flags = Flags; Save->Items = Items; Save->Trust = Trust;
	Save->Stats = Stats; Save->Kills = Kills; Save->Elements = Elements;
	Save->Spells = Spells; Save->Techs = Techs; Save->Styles = Styles;
	Save->Records = Records; Save->Artifacts = Artifacts; Save->Unlocks = Unlocks;
	Save->Ending = Ending;
	Save->CurrentLoc = CurrentLoc;

	UGameplayStatics::SaveGameToSlot(Save, TEXT("SL0"), 0);
	PlaySFX(TEXT("save"), 0.4f);
}

// ---------------- Аудио ----------------

void URSLGameInstance::PlaySFX(const FString& Name, float Vol)
{
	if (Name.IsEmpty()) { return; }
	const FString Path = FString::Printf(TEXT("/Game/RS/Audio/Sfx/S_%s.S_%s"), *Name, *Name);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
	{
		UGameplayStatics::PlaySound2D(this, Sound, Vol);
	}
}

void URSLGameInstance::PlayMusic(const FString& Kind)
{
	if (Kind == CurrentMusicKind) { return; }
	CurrentMusicKind = Kind;

	const FString Path = FString::Printf(TEXT("/Game/RS/Audio/Music/M_%s.M_%s"), *Kind, *Kind);
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);

	// старый трек — плавно гасим
	if (UAudioComponent* Old = MusicComp.Get())
	{
		Old->FadeOut(1.0f, 0.0f);
		MusicComp = nullptr;
	}

	if (!Sound) { return; }

	// как в game.py: бесконечный цикл, battle 0.45 / остальное 0.28, кроссфейд 1 с
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = true;
	}
	const float Vol = (Kind == TEXT("battle")) ? 0.45f : 0.28f;
	UAudioComponent* Comp = UGameplayStatics::CreateSound2D(this, Sound, Vol, 1.f, 0.f, nullptr, true, false);
	if (Comp)
	{
		MusicComp = Comp;
		Comp->FadeIn(1.0f);
	}
}

void URSLGameInstance::SyncMusic()
{
	FString Kind;
	if (State == ESLStoryState::Idle)
	{
		Kind = TEXT("theme");
	}
	else if (State == ESLStoryState::Ended)
	{
		Kind = TEXT("ending");
	}
	else if (State == ESLStoryState::InBattle || State == ESLStoryState::BattleIntro)
	{
		Kind = TEXT("battle");
	}
	else if (SceneIndex < PrologEnd)
	{
		Kind = TEXT("sad"); // пролог до Главы 1 — грустная тема
	}
	else if (Scenes.IsValidIndex(SceneIndex) && GetStr(Scenes[SceneIndex], TEXT("mood")) == TEXT("sad"))
	{
		Kind = TEXT("sad");
	}
	else
	{
		Kind = TEXT("scene");
	}
	PlayMusic(Kind);
}

void URSLGameInstance::PlayVoice()
{
	StopVoice();
	if (!bVoiceOn) { return; }

	const FString* File = VoiceFiles.Find(SceneIndex);
	if (!File || File->IsEmpty()) { return; }

	const FString Base = FPaths::GetBaseFilename(*File); // без .wav
	const FString Path = FString::Printf(TEXT("/Game/RS/Audio/Voice/V_%s.V_%s"), *Base, *Base);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
	{
		VoiceComp = UGameplayStatics::SpawnSound2D(this, Sound, 1.f);
	}
}

void URSLGameInstance::StopVoice()
{
	if (UAudioComponent* Comp = VoiceComp.Get())
	{
		Comp->Stop();
	}
	VoiceComp = nullptr;
}

void URSLGameInstance::ToggleVoice()
{
	bVoiceOn = !bVoiceOn;
	if (!bVoiceOn) { StopVoice(); }
	else if (WantsTextInput()) { PlayVoice(); } // догоняем текущую реплику
}

void URSLGameInstance::ToggleMap()
{
	// Как в game.py: карта не в бою, толькоระหวом текстовых состояний.
	if (State == ESLStoryState::Idle || State == ESLStoryState::Ended
		|| State == ESLStoryState::InBattle || State == ESLStoryState::BattleIntro)
	{
		return;
	}
	if (OpenMenu != 0) { CloseMenu(); } // карта и панели не одновременно
	bMapOpen = !bMapOpen;
	OnMapChanged.Broadcast(bMapOpen);
}

void URSLGameInstance::CloseMap()
{
	if (!bMapOpen) { return; }
	bMapOpen = false;
	OnMapChanged.Broadcast(false);
}

// ---------------- Панели «Герой» (G) и «Магия» (K) ----------------

void URSLGameInstance::ToggleMenu(int32 Kind)
{
	// Как в game.py: панели только в текстовых состояниях, не в бою.
	if (State == ESLStoryState::Idle || State == ESLStoryState::Ended
		|| State == ESLStoryState::InBattle || State == ESLStoryState::BattleIntro)
	{
		return;
	}
	OpenMenu = (OpenMenu == Kind) ? 0 : Kind; // повторное нажатие — закрыть
	if (OpenMenu != 0 && bMapOpen) { CloseMap(); } // карта и панели не одновременно
	OnMenuChanged.Broadcast(OpenMenu);
}

void URSLGameInstance::CloseMenu()
{
	if (OpenMenu == 0) { return; }
	OpenMenu = 0;
	OnMenuChanged.Broadcast(0);
}

void URSLGameInstance::RecalcMaxStats()
{
	// Как max_stats() в game.py, но не урезает текущий максимум:
	// база UE (100/50) уже выше формулы при стартовых характеристиках.
	const int32 Vit = Stats.FindRef(TEXT("vit"));
	const int32 Int = Stats.FindRef(TEXT("int"));
	MaxHP = FMath::Max(MaxHP, 55.f + Vit * 9.f + Level * 6.f);
	MaxMP = FMath::Max(MaxMP, 35.f + Int * 8.f + Level * 5.f);
	HP = FMath::Clamp(HP, 0.f, MaxHP);
	MP = FMath::Clamp(MP, 0.f, MaxMP);
}

void URSLGameInstance::SpendStat(const FString& Key)
{
	if (StatPts <= 0) { return; }
	--StatPts;
	Stats.Add(Key, Stats.FindRef(Key) + 1);
	RecalcMaxStats();
	PlaySFX(TEXT("coin"), 0.45f);
	OnStats.Broadcast();
}

void URSLGameInstance::LearnSpell(const FString& Id)
{
	if (Spells.Contains(Id) || !GameData.IsValid()) { return; }
	const TSharedPtr<FJsonObject>* SpellsObj = nullptr;
	if (!GameData->TryGetObjectField(TEXT("spells"), SpellsObj)) { return; }
	const TSharedPtr<FJsonObject>* D = nullptr;
	if (!(*SpellsObj)->TryGetObjectField(Id, D)) { return; }

	if (SP < 1)
	{
		PlaySFX(TEXT("click"), 0.4f);
		return;
	}
	FString Req;
	if ((*D)->TryGetStringField(TEXT("req"), Req) && !Spells.Contains(Req))
	{
		return; // сначала предыдущее заклинание стихии
	}
	FString Flag;
	if ((*D)->TryGetStringField(TEXT("flag"), Flag) && !Flags.Contains(Flag))
	{
		return; // нужен сюжетный ключ (учёба в университете)
	}
	--SP;
	Spells.Add(Id);
	PlaySFX(TEXT("level"), 0.55f);
	OnStats.Broadcast();
}

void URSLGameInstance::TrainElement(const FString& Elem)
{
	if (SP < 1) { return; }
	--SP;
	Elements.Add(Elem, Elements.FindRef(Elem) + 30);
	PlaySFX(TEXT("level"), 0.5f);
	OnStats.Broadcast();
}

FString URSLGameInstance::GuildTitle() const
{
	if (!GameData.IsValid()) { return TEXT("ранг F"); }
	const TArray<TSharedPtr<FJsonValue>>* Needs = nullptr;
	GameData->TryGetArrayField(TEXT("guild_needs"), Needs);
	int32 Rank = 0;
	if (Needs)
	{
		for (int32 i = 0; i < Needs->Num(); ++i)
		{
			if (GuildPts >= (int32)(*Needs)[i]->AsNumber()) { Rank = i; }
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	GameData->TryGetArrayField(TEXT("guild_names"), Names);
	return (Names && Names->IsValidIndex(Rank)) ? (*Names)[Rank]->AsString() : TEXT("ранг F");
}

FString URSLGameInstance::HeroRankTitle() const
{
	if (!GameData.IsValid()) { return FString(); }
	const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
	GameData->TryGetArrayField(TEXT("hero_rank_at"), At);
	int32 Idx = 0;
	if (At)
	{
		for (int32 i = 0; i < At->Num(); ++i)
		{
			if (Level >= (int32)(*At)[i]->AsNumber()) { Idx = i; }
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Ranks = nullptr;
	GameData->TryGetArrayField(TEXT("ranks"), Ranks);
	return (Ranks && Ranks->IsValidIndex(Idx)) ? (*Ranks)[Idx]->AsString() : FString();
}

FString URSLGameInstance::ElemRankTitle(const FString& Elem) const
{
	if (!GameData.IsValid()) { return FString(); }
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	GameData->TryGetArrayField(TEXT("elem_xp"), Steps);
	const int32 X = Elements.FindRef(Elem);
	int32 Rank = 0;
	if (Steps)
	{
		for (int32 i = 0; i < Steps->Num(); ++i)
		{
			if (X >= (int32)(*Steps)[i]->AsNumber()) { Rank = i; }
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Ranks = nullptr;
	GameData->TryGetArrayField(TEXT("ranks"), Ranks);
	return (Ranks && Ranks->IsValidIndex(Rank)) ? (*Ranks)[Rank]->AsString() : FString();
}

int32 URSLGameInstance::ElemRankNeed(const FString& Elem) const
{
	if (!GameData.IsValid()) { return 0; }
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	GameData->TryGetArrayField(TEXT("elem_xp"), Steps);
	if (!Steps) { return 1000000000; }
	const int32 X = Elements.FindRef(Elem);
	for (int32 i = 0; i < Steps->Num(); ++i)
	{
		const int32 Threshold = (int32)(*Steps)[i]->AsNumber();
		if (X < Threshold) { return Threshold; }
	}
	return 1000000000;
}

void URSLGameInstance::TravelTo(const FString& LocId)
{
	if (!Unlocks.Contains(LocId)) { return; } // закрытые локации не открываем в обход сюжета
	if (LocId == CurrentLoc) { return; }
	CurrentLoc = LocId;
	PlaySFX(TEXT("level"), 0.6f); // звук переезда
	OnStats.Broadcast();
}

bool URSLGameInstance::LoadProgress()
{
	if (!UGameplayStatics::DoesSaveGameExist(TEXT("SL0"), 0)) { return false; }

	USLSaveGameData* Save = Cast<USLSaveGameData>(
		UGameplayStatics::LoadGameFromSlot(TEXT("SL0"), 0));
	if (!Save) { return false; }

	SceneIndex = FMath::Clamp(Save->SceneIndex, 0, Scenes.Num() - 1);
	HP = Save->HP; MaxHP = Save->MaxHP; MP = Save->MP; MaxMP = Save->MaxMP;
	XP = Save->XP; Gold = Save->Gold; SP = Save->SP; GuildPts = Save->GuildPts;
	Level = FMath::Max(1, Save->Level); StatPts = Save->StatPts;
	Flags = Save->Flags; Items = Save->Items; Trust = Save->Trust;
	Stats = Save->Stats; Kills = Save->Kills; Elements = Save->Elements;
	// Сейвы старой версии без характеристик — докомплектовываем стартовыми.
	if (!Stats.Contains(TEXT("str")) || Stats.FindRef(TEXT("str")) + Stats.FindRef(TEXT("dex"))
		+ Stats.FindRef(TEXT("int")) + Stats.FindRef(TEXT("vit")) + Stats.FindRef(TEXT("lck")) == 0)
	{
		Stats.Add(TEXT("str"), 5);
		Stats.Add(TEXT("dex"), 6);
		Stats.Add(TEXT("int"), 8);
		Stats.Add(TEXT("vit"), 5);
		Stats.Add(TEXT("lck"), 5);
	}
	Spells = Save->Spells; Techs = Save->Techs; Styles = Save->Styles;
	Records = Save->Records; Artifacts = Save->Artifacts; Unlocks = Save->Unlocks;
	Ending = Save->Ending;
	CurrentLoc = Save->CurrentLoc.IsEmpty() ? TEXT("village") : Save->CurrentLoc;
	return true;
}
