#include "RSLStoryWidget.h"
#include "RSLGameInstance.h"
#include "RSLGameMode.h"
#include "RSLEnemyCharacter.h"

#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// ---------------- Инициализация ----------------

void URSLStoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->OnLine.AddDynamic(this, &URSLStoryWidget::HandleLine);
		GI->OnOptions.AddDynamic(this, &URSLStoryWidget::HandleOptions);
		GI->OnEnd.AddDynamic(this, &URSLStoryWidget::HandleEnd);
		GI->OnStats.AddDynamic(this, &URSLStoryWidget::HandleStats);
		GI->OnMapChanged.AddDynamic(this, &URSLStoryWidget::HandleMap);
		GI->OnTrainingDefeat.AddDynamic(this, &URSLStoryWidget::HandleTrainingDefeat);
		GI->OnMenuChanged.AddDynamic(this, &URSLStoryWidget::HandleMenu);
	}
}

TSharedRef<SWidget> URSLStoryWidget::RebuildWidget()
{
	const FSlateFontInfo SpeakerFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);
	const FSlateFontInfo StatsFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	const FSlateFontInfo ChapterFont = FCoreStyle::GetDefaultFontStyle("Bold", 40);
	const FSlateFontInfo ChoiceFont = FCoreStyle::GetDefaultFontStyle("Regular", 20);

	SpeakerText = SNew(STextBlock)
		.Font(SpeakerFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)))
		.Visibility(EVisibility::Collapsed);

	// Фон локации (скачанная текстура /Game/RS/Backgrounds/Locations/T_<id>).
	// Показывается на карточках глав и финальном экране — как в оригинальном оформлении.
	BackdropBrush = FSlateBrush();
	BackdropBrush.DrawAs = ESlateBrushDrawType::Image;
	BackdropImage = SNew(SImage)
		.Image(&BackdropBrush)
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f))
		.Visibility(EVisibility::Collapsed);
	UpdateBackdrop();

	BodyText = SNew(STextBlock)
		.Font(BodyFont)
		.AutoWrapText(true)
		.ColorAndOpacity(FSlateColor(FLinearColor::White));

	StatsText = SNew(STextBlock)
		.Font(StatsFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.9f, 1.f, 1.f)));

	ChapterText = SNew(STextBlock)
		.Font(ChapterFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.9f, 0.7f, 1.f)))
		.Visibility(EVisibility::Collapsed);

	// Кровавая надпись «ВЫ ПРОИГРАЛИ»: огромный красный шрифт поверх экрана (тренировка, HP = 1).
	const FSlateFontInfo DefeatFont = FCoreStyle::GetDefaultFontStyle("Bold", 120);
	DefeatText = SNew(STextBlock)
		.Font(DefeatFont)
		.Text(FText::FromString(TEXT("ВЫ ПРОИГРАЛИ")))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.02f, 0.02f, 1.f)))
		.ShadowOffset(FVector2D(4.f, 4.f))
		.ShadowColorAndOpacity(FLinearColor(0.25f, 0.f, 0.f, 0.9f))
		.Visibility(EVisibility::Collapsed);

	// Подсказка «Тренировка» — верхний центр во время учебного боя.
	TrainHint = SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
		.Text(FText::FromString(TEXT("— ТРЕНИРОВКА —")))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.75f, 0.9f, 1.f)))
		.Visibility(EVisibility::Collapsed);

	OptionsBox = SNew(SVerticalBox);

	// Панель реплик (полупрозрачный тёмный фон с репликами и кнопками выбора).
	// static — кисть должна жить дольше, чем функция (SBorder хранит указатель).
	static FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::Box;
	PanelBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.75f));

	Panel = SNew(SBorder)
		.BorderImage(&PanelBrush)
		.Padding(FMargin(24.f, 16.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
			[
				SpeakerText.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				BodyText.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				OptionsBox.ToSharedRef()
			]
		];

	// --- Боевой HUD: имя врага + полоса HP + подсказка управления. ---
	BattleBarFill.DrawAs = ESlateBrushDrawType::Box;
	BattleBarFill.TintColor = FSlateColor(FLinearColor(0.72f, 0.14f, 0.10f, 1.f));
	BattleBarBack.DrawAs = ESlateBrushDrawType::Box;
	BattleBarBack.TintColor = FSlateColor(FLinearColor(0.10f, 0.10f, 0.12f, 0.85f));

	BattleRows = SNew(SVerticalBox);

	BattleHint = SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
		.Text(FText::FromString(TEXT("ЛКМ — удар   ·   ПКМ — заклинание   ·   WASD — движение")))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.86f, 0.95f, 0.9f)));

	static FSlateBrush BattleBrush;
	BattleBrush.DrawAs = ESlateBrushDrawType::Box;
	BattleBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.72f));

	BattlePanel = SNew(SBorder)
		.BorderImage(&BattleBrush)
		.Padding(FMargin(22.f, 12.f))
		.Visibility(EVisibility::Collapsed)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				BattleRows.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				BattleHint.ToSharedRef()
			]
		];

	// Прицел — центр экрана, только во время боя.
	Crosshair = SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
		.Text(FText::FromString(TEXT("·")))
		.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.85f)))
		.ShadowOffset(FVector2D(2.f, 2.f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		.Visibility(EVisibility::Collapsed);

	bInBattleMode = false;

	// Верхний правый угол: HP/MP/Gold/XP.
	const TSharedRef<SWidget> StatsSlot =
		SNew(SBorder)
		.Padding(FMargin(12.f, 8.f))
		[
			StatsText.ToSharedRef()
		];

	// --- Карта мира (M): полноэкранная панель поверх всего, по умолчанию скрыта. ---
	static FSlateBrush MapBrush;
	MapBrush.DrawAs = ESlateBrushDrawType::Box;
	MapBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.06f, 0.93f));

	MapTitle = SNew(STextBlock)
		.Font(ChapterFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)));

	MapList = SNew(SVerticalBox);

	MapInfoText = SNew(STextBlock)
		.Font(BodyFont)
		.AutoWrapText(true)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.9f, 0.95f, 1.f)));

	TravelButton = SNew(SButton)
		.ContentPadding(FMargin(14.f, 8.f))
		.Visibility(EVisibility::Collapsed)
		.OnClicked_Lambda([this]() { return OnTravelClicked(); })
		[
			SNew(STextBlock)
			.Font(ChoiceFont)
			.Text(FText::FromString(TEXT("Отправиться сюда")))
		];

	MapPanel = SNew(SBorder)
		.BorderImage(&MapBrush)
		.Padding(FMargin(40.f, 30.f))
		.Visibility(EVisibility::Collapsed)
		[
			SNew(SHorizontalBox)

			// левая колонка: список открытых локаций
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.f, 0.f, 32.f, 0.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
				[
					MapTitle.ToSharedRef()
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder)
					.Padding(FMargin(12.f))
					[
						MapList.ToSharedRef()
					]
				]
			]

			// правая колонка: описание выбранной локации
			+ SHorizontalBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					MapInfoText.ToSharedRef()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 16.f, 0.f, 0.f))
				[
					TravelButton.ToSharedRef()
				]
			]
		];

	// --- Панели «Герой» (G) / «Магия» (K): полноэкранные, по умолчанию скрыты. ---
	static FSlateBrush MenuBrush;
	MenuBrush.DrawAs = ESlateBrushDrawType::Box;
	MenuBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.95f));

	MenuTitle = SNew(STextBlock)
		.Font(ChapterFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)));

	MenuBox = SNew(SVerticalBox);

	// Содержимое длинное (14 заклинаний) — прокрутка колесом мыши.
	MenuPanel = SNew(SBorder)
		.BorderImage(&MenuBrush)
		.Padding(FMargin(48.f, 34.f))
		.Visibility(EVisibility::Collapsed)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 18.f))
			[
				MenuTitle.ToSharedRef()
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					MenuBox.ToSharedRef()
				]
			]
		];

	return SNew(SOverlay)

	// Фон локации — нижний слой, затемнён полупрозрачностью.
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		BackdropImage.ToSharedRef()
	]

	// Карточка главы / финальный экран — поверх всего, по центру.
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		ChapterText.ToSharedRef()
	]

	// Подсказка «Тренировка» — верхний центр, только в учебном бою.
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f, 0.f, 0.f)
	[
		TrainHint.ToSharedRef()
	]

	// HUD-статы — верхний правый угол.
	+ SOverlay::Slot()
	.HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 16.f, 24.f, 0.f)
	[
		StatsSlot
	]

	// Панель сюжета — низ экрана.
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(80.f, 0.f, 80.f, 40.f)
	[
		Panel.ToSharedRef()
	]

	// Боевой HUD — низ экрана (панель реплик в бою свёрнута, место свободно).
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 48.f)
	[
		BattlePanel.ToSharedRef()
	]

	// Прицел — центр экрана во время боя.
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		Crosshair.ToSharedRef()
	]

	// Кровавая надпись «ВЫ ПРОИГРАЛИ» — поверх панели, по центру экрана.
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		DefeatText.ToSharedRef()
	]

	// Карта мира — поверх всех слоёв (показывается по M).
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(60.f, 40.f)
	[
		MapPanel.ToSharedRef()
	]

	// Панели «Герой» (G) / «Магия» (K) — поверх карты и сюжета.
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(120.f, 60.f)
	[
		MenuPanel.ToSharedRef()
	];
}

// ---------------- Обработчики движка сюжета ----------------

void URSLStoryWidget::HandleLine(const FString& Speaker, const FString& Text, bool bFlashback)
{
	ClearOptions();

	// Скрываем центральную карточку от предыдущей главы/финала.
	if (ChapterText.IsValid())
	{
		ChapterText->SetVisibility(EVisibility::Collapsed);
	}
	SetBackdropVisible(false);

	// Карточка главы ("ГЛАВА N" из сцен типа ch) — крупно по центру, без нижней панели.
	if (Text.StartsWith(TEXT("ГЛАВА")))
	{
		if (SpeakerText.IsValid()) { SpeakerText->SetVisibility(EVisibility::Collapsed); }
		if (BodyText.IsValid()) { BodyText->SetVisibility(EVisibility::Collapsed); }
		if (ChapterText.IsValid())
		{
			ChapterText->SetText(FText::FromString(Text));
			ChapterText->SetVisibility(EVisibility::Visible);
		}
		SetBackdropVisible(true);
		SetTextUI(true);
		return;
	}

	// Обычная реплика — фон локации, как в текстовых сценах оригинала.
	// 3D-мир от первого лица виден в бою, на карте и при свободном движении.
	SetBackdropVisible(true);

	if (BodyText.IsValid())
	{
		BodyText->SetVisibility(EVisibility::Visible);
	}

	if (SpeakerText.IsValid())
	{
		if (Speaker.IsEmpty())
		{
			SpeakerText->SetVisibility(EVisibility::Collapsed);
		}
		else
		{
			SpeakerText->SetVisibility(EVisibility::Visible);
			SpeakerText->SetText(FText::FromString(Speaker));
		}
	}

	if (BodyText.IsValid())
	{
		BodyText->SetText(FText::FromString(Text));
		// Флешбек рисуем сепией, как в оригинальном оформлении.
		BodyText->SetColorAndOpacity(bFlashback
			? FSlateColor(FLinearColor(0.85f, 0.72f, 0.5f, 1.f))
			: FSlateColor(FLinearColor::White));
	}

	if (Panel.IsValid())
	{
		Panel->SetVisibility(EVisibility::Visible);
	}

	SetTextUI(true);
}

void URSLStoryWidget::HandleOptions(const TArray<FString>& Options)
{
	if (!OptionsBox.IsValid())
	{
		return;
	}

	ClearOptions();

	const FSlateFontInfo OptFont = FCoreStyle::GetDefaultFontStyle("Regular", 20);

	for (int32 i = 0; i < Options.Num() && i < 5; ++i)
	{
		const int32 Index = i;
		const FText Label = FText::FromString(FString::Printf(TEXT("%d. %s"), i + 1, *Options[i]));

		OptionsBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 4.f))
		[
			SNew(SButton)
			.OnClicked_Lambda([this, Index]() { return OnOptionClicked(Index); })
			.ContentPadding(FMargin(12.f, 6.f))
			[
				SNew(STextBlock)
				.Font(OptFont)
				.Text(Label)
			]
		];
	}

	// Во время выбора показываем курсор и даём кликать кнопки.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
}

void URSLStoryWidget::HandleEnd(const FString& Text)
{
	ClearOptions();

	if (SpeakerText.IsValid())
	{
		SpeakerText->SetVisibility(EVisibility::Collapsed);
	}
	if (BodyText.IsValid())
	{
		BodyText->SetText(FText::FromString(Text));
		BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.9f, 0.7f, 1.f)));
	}
	if (Panel.IsValid())
	{
		Panel->SetVisibility(EVisibility::Visible);
	}

	SetBackdropVisible(true);
	SetTextUI(true);
}

// ---------------- Тренировка: кровавая надпись ----------------

void URSLStoryWidget::HandleTrainingDefeat()
{
	if (!DefeatText.IsValid())
	{
		return;
	}
	DefeatText->SetVisibility(EVisibility::HitTestInvisible);

	// Надпись держится несколько секунд, затем гаснет — сюжет уже продолжается.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(DefeatHideHandle, this,
			&URSLStoryWidget::HideDefeatText, 4.f, false);
	}
}

void URSLStoryWidget::HideDefeatText()
{
	if (DefeatText.IsValid())
	{
		DefeatText->SetVisibility(EVisibility::Collapsed);
	}
}

// ---------------- Фон локации (карточки глав / финал) ----------------

void URSLStoryWidget::UpdateBackdrop()
{
	if (!BackdropImage.IsValid())
	{
		return;
	}

	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	// Текстура /Game/RS/Backgrounds/Locations/T_<id>.T_<id> меняется со сменой локации.
	const FString Want = GI->CurrentLoc;
	if (Want == BackdropLocId && BackdropBrush.GetResourceObject())
	{
		return;
	}
	BackdropLocId = Want;

	const FString Path = FString::Printf(
		TEXT("/Game/RS/Backgrounds/Locations/T_%s.T_%s"), *Want, *Want);
	UTexture2D* Tex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Path));
	if (Tex)
	{
		BackdropTexture = Tex; // UPROPERTY-холд: GC не уберёт текстуру из-под Slate.
		BackdropBrush.SetResourceObject(Tex);
		BackdropBrush.ImageSize = FVector2D((float)Tex->GetSizeX(), (float)Tex->GetSizeY());
		BackdropBrush.DrawAs = ESlateBrushDrawType::Image;
		// Чуть притемняем, чтобы золотая надпись читалась поверх панорамы.
		BackdropImage->SetColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f, 1.f));
	}
	else
	{
		BackdropTexture = nullptr;
		BackdropBrush.SetResourceObject(nullptr);
	}
	BackdropImage->SetImage(&BackdropBrush);
}

void URSLStoryWidget::SetBackdropVisible(bool bVisible)
{
	if (!BackdropImage.IsValid())
	{
		return;
	}
	if (bVisible)
	{
		UpdateBackdrop();
	}
	BackdropImage->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void URSLStoryWidget::HandleStats()
{
	if (!StatsText.IsValid())
	{
		return;
	}

	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	const FString Stats = FString::Printf(
		TEXT("Ур %d   HP %d/%d   MP %d/%d   XP %d/%d   Gold %d"),
		GI->Level,
		FMath::RoundToInt(GI->HP), FMath::RoundToInt(GI->MaxHP),
		FMath::RoundToInt(GI->MP), FMath::RoundToInt(GI->MaxMP),
		GI->XP, GI->Level * 120, GI->Gold);

	StatsText->SetText(FText::FromString(Stats));

	// Открытая панель «Герой»/«Магия» должна показывать новые значения.
	if (GI->GetOpenMenu() != 0)
	{
		RebuildMenu();
	}
}

void URSLStoryWidget::SetInBattle(bool bInBattle)
{
	if (bInBattle)
	{
		// Бой начинается — карту и панели принудительно закрываем до виджета и ввода.
		if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
		{
			GI->CloseMap();
			GI->CloseMenu();
		}
	}
	if (MapPanel.IsValid())
	{
		MapPanel->SetVisibility(EVisibility::Collapsed);
	}
	if (MenuPanel.IsValid())
	{
		MenuPanel->SetVisibility(EVisibility::Collapsed);
	}
	if (Panel.IsValid())
	{
		Panel->SetVisibility(bInBattle ? EVisibility::Collapsed : EVisibility::Visible);
	}
	if (ChapterText.IsValid())
	{
		ChapterText->SetVisibility(EVisibility::Collapsed);
	}
	SetBackdropVisible(false);
	// Подсказка «Тренировка» — только в учебном бою (train в узле сюжета).
	if (TrainHint.IsValid())
	{
		const URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
		const bool bTrain = bInBattle && GI && GI->IsTrainingBattle();
		TrainHint->SetVisibility(bTrain ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	// Новая сцена/бой — старую кровавую надпись гасим сразу.
	if (bInBattle)
	{
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().ClearTimer(DefeatHideHandle);
		}
		HideDefeatText();
	}

	// Боевой HUD: строки врагов и прицел — только пока идёт бой.
	bInBattleMode = bInBattle;
	BattleKey.Reset(); // состав боя пересоберётся на первом кадре
	// Бой: фон локации убираем — виден 3D-мир от первого лица.
	SetBackdropVisible(!bInBattle);
	if (BattlePanel.IsValid())
	{
		BattlePanel->SetVisibility(bInBattle ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (Crosshair.IsValid())
	{
		Crosshair->SetVisibility(bInBattle ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	SetTextUI(!bInBattle);
}

// ---------------- Боевой HUD ----------------

void URSLStoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateBattleHUD();
}

void URSLStoryWidget::RebuildBattleRows(const TArray<FString>& DisplayNames)
{
	if (!BattleRows.IsValid())
	{
		return;
	}

	BattleRows->ClearChildren();
	BattleNames.Empty();
	BattleHPText.Empty();
	BattleBars.Empty();

	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	const FSlateFontInfo HpFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);

	for (const FString& Name : DisplayNames)
	{
		TSharedPtr<STextBlock> NameText = SNew(STextBlock)
			.Font(NameFont)
			.Justification(ETextJustify::Right)
			.Text(FText::FromString(Name))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)));

		TSharedPtr<STextBlock> HpText = SNew(STextBlock)
			.Font(HpFont)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.92f, 0.95f, 1.f)));

		TSharedPtr<SProgressBar> Bar = SNew(SProgressBar)
			.Percent(1.f)
			.FillImage(&BattleBarFill)
			.BackgroundImage(&BattleBarBack)
			.FillColorAndOpacity(FSlateColor(FLinearColor::White));

		BattleRows->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 3.f))
		[
			SNew(SHorizontalBox)

			// имя — фиксированная колонка, чтобы полосы шли ровно
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 14.f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(220.f)
				[
					NameText.ToSharedRef()
				]
			]

			// полоса HP
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(360.f)
				.HeightOverride(14.f)
				[
					Bar.ToSharedRef()
				]
			]

			// цифры HP
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(14.f, 0.f, 0.f, 0.f))
			[
				HpText.ToSharedRef()
			]
		];

		BattleNames.Add(NameText); // имя уже вставлено текстом выше, храним для порядка
		BattleHPText.Add(HpText);
		BattleBars.Add(Bar);
	}
}

void URSLStoryWidget::UpdateBattleHUD()
{
	if (!bInBattleMode)
	{
		return;
	}

	UWorld* World = GetWorld();
	ARSLGameMode* GM = World ? World->GetAuthGameMode<ARSLGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	TArray<FString> Names;
	FString Key;
	for (const TWeakObjectPtr<ARSLEnemyCharacter>& Ptr : GM->GetAliveEnemies())
	{
		ARSLEnemyCharacter* Enemy = Ptr.Get();
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}
		Names.Add(Enemy->GetDisplayName());
		Key += Enemy->EnemyId + TEXT("|");
	}

	if (Key != BattleKey)
	{
		BattleKey = Key;
		RebuildBattleRows(Names);
	}

	int32 Row = 0;
	for (const TWeakObjectPtr<ARSLEnemyCharacter>& Ptr : GM->GetAliveEnemies())
	{
		ARSLEnemyCharacter* Enemy = Ptr.Get();
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}
		if (Row < BattleBars.Num())
		{
			const float MaxHP = FMath::Max(1.f, Enemy->GetMaxHealth());
			BattleBars[Row]->SetPercent(FMath::Clamp(Enemy->GetHealth() / MaxHP, 0.f, 1.f));
			if (Row < BattleHPText.Num())
			{
				BattleHPText[Row]->SetText(FText::FromString(FString::Printf(
					TEXT("%d / %d"),
					FMath::RoundToInt(FMath::Max(0.f, Enemy->GetHealth())),
					FMath::RoundToInt(MaxHP))));
			}
		}
		++Row;
	}
}

// ---------------- Внутренности ----------------

FReply URSLStoryWidget::OnOptionClicked(int32 Index)
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->SelectChoice(Index);
	}
	ClearOptions();
	return FReply::Handled();
}

void URSLStoryWidget::SetTextUI(bool bTextUI)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bTextUI)
		{
			// Текстовые сцены: клик/Enter/Space, курсор доступен для выборов.
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
		else
		{
			PC->bShowMouseCursor = false;
			FInputModeGameOnly Mode;
			PC->SetInputMode(Mode);
		}
	}
}

void URSLStoryWidget::ClearOptions()
{
	if (OptionsBox.IsValid())
	{
		OptionsBox->ClearChildren();
	}

	// После клика по варианту возвращаем управление в игру (курсор скрыт).
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
	}
}

// ---------------- Карта мира ----------------

void URSLStoryWidget::HandleMap(bool bOpen)
{
	if (!MapPanel.IsValid())
	{
		return;
	}

	if (bOpen)
	{
		RebuildMapList();
		MapPanel->SetVisibility(EVisibility::Visible);
		SetTextUI(true); // курсор для кликов по локациям
	}
	else
	{
		MapPanel->SetVisibility(EVisibility::Collapsed);
		// Возврат к вводу сюжета, если тот ждёт подтверждения.
		URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
		SetTextUI(GI && GI->WantsTextInput());
	}
}

void URSLStoryWidget::RebuildMapList()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI || !MapList.IsValid() || !MapTitle.IsValid())
	{
		return;
	}

	MapList->ClearChildren();

	if (GI->Unlocks.Num() == 0)
	{
		GI->Unlocks.Add(TEXT("village")); // стартовая локация всегда открыта
	}

	if (SelectedLocId.IsEmpty() || !GI->Unlocks.Contains(SelectedLocId))
	{
		SelectedLocId = GI->CurrentLoc;
	}

	MapTitle->SetText(FText::FromString(FString::Printf(
		TEXT("КАРТА МИРА   открыто %d из %d"),
		GI->Unlocks.Num(), GI->Locations.Num())));

	const FSlateFontInfo LocFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);

	for (const TSharedPtr<FJsonObject>& Loc : GI->Locations)
	{
		FString Id, Name;
		if (!Loc.IsValid() || !Loc->TryGetStringField(TEXT("id"), Id)) { continue; }
		if (!GI->Unlocks.Contains(Id)) { continue; }
		Loc->TryGetStringField(TEXT("name"), Name);

		const bool bHere = (Id == GI->CurrentLoc);
		const bool bSelected = (Id == SelectedLocId);
		const FString Label = bHere
			? FString::Printf(TEXT("> %s   [вы здесь]"), *Name)
			: FString::Printf(TEXT("  %s"), *Name);

		MapList->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 3.f))
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.f, 6.f))
			.OnClicked_Lambda([this, Id]()
			{
				SelectedLocId = Id;
				RebuildMapList();
				UpdateMapInfo();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(LocFont)
				.ColorAndOpacity(FSlateColor(bSelected
					? FLinearColor(0.95f, 0.85f, 0.55f, 1.f)
					: FLinearColor::White))
				.Text(FText::FromString(Label))
			]
		];
	}

	UpdateMapInfo();
}

void URSLStoryWidget::UpdateMapInfo()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI || !MapInfoText.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Found = GI->Locations.FindByPredicate(
		[this](const TSharedPtr<FJsonObject>& L)
		{
			FString Id;
			return L.IsValid() && L->TryGetStringField(TEXT("id"), Id) && Id == SelectedLocId;
		});

	if (!Found || !Found->IsValid())
	{
		MapInfoText->SetText(FText::FromString(TEXT("Выберите локацию слева.")));
		if (TravelButton.IsValid()) { TravelButton->SetVisibility(EVisibility::Collapsed); }
		return;
	}

	const TSharedPtr<FJsonObject>& Loc = *Found;
	FString Name, Desc;
	Loc->TryGetStringField(TEXT("name"), Name);
	Loc->TryGetStringField(TEXT("desc"), Desc);
	double Danger = 1.0;
	Loc->TryGetNumberField(TEXT("danger"), Danger);

	FString Out = FString::Printf(TEXT("%s\n\n%s\n\nОпасность: %s (%d/9)\n"),
		*Name, *Desc,
		*FString::ChrN(FMath::Clamp((int32)Danger, 0, 9), TEXT('·')),
		(int32)Danger);

	// Флаги локации (лавка, гильдия, запись, артефакт) — как в pygame-панели.
	TArray<FString> Flags;
	bool bTmp;
	if (Loc->TryGetBoolField(TEXT("shop"), bTmp) && bTmp) { Flags.Add(TEXT("лавка")); }
	if (Loc->TryGetBoolField(TEXT("guild"), bTmp) && bTmp) { Flags.Add(TEXT("гильдия")); }
	FString RecId, ArtId;
	if (Loc->TryGetStringField(TEXT("record"), RecId) && !RecId.IsEmpty())
	{
		Flags.Add(GI->Records.Contains(RecId)
			? TEXT("запись (найдена)") : TEXT("запись"));
	}
	if (Loc->TryGetStringField(TEXT("artifact"), ArtId) && !ArtId.IsEmpty())
	{
		Flags.Add(GI->Artifacts.Contains(ArtId)
			? TEXT("артефакт (найден)") : TEXT("артефакт"));
	}
	if (Flags.Num() > 0)
	{
		Out += TEXT("· ") + FString::Join(Flags, TEXT(" · ")) + TEXT("\n");
	}

	const bool bHere = (SelectedLocId == GI->CurrentLoc);
	Out += bHere
		? TEXT("\nВы здесь")
		: TEXT("\nЛокация открыта");

	MapInfoText->SetText(FText::FromString(Out));

	if (TravelButton.IsValid())
	{
		TravelButton->SetVisibility(bHere ? EVisibility::Collapsed : EVisibility::Visible);
	}
}

FReply URSLStoryWidget::OnTravelClicked()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->TravelTo(SelectedLocId);
		RebuildMapList();
	}
	return FReply::Handled();
}

// ---------------- Панели «Герой» (G) и «Магия» (K) ----------------

void URSLStoryWidget::HandleMenu(int32 Kind)
{
	if (!MenuPanel.IsValid() || !MenuBox.IsValid() || !MenuTitle.IsValid())
	{
		return;
	}

	if (Kind == 0)
	{
		MenuPanel->SetVisibility(EVisibility::Collapsed);
		// Возврат к вводу сюжета, если тот ждёт подтверждения.
		URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
		SetTextUI(GI && GI->WantsTextInput());
		return;
	}

	RebuildMenu();
	MenuPanel->SetVisibility(EVisibility::Visible);
	SetTextUI(true); // курсор для кнопок «+» / «Изучить»
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
	}
}

TSharedRef<SWidget> URSLStoryWidget::MakeStatRow(
	const FString& Key, const FString& Name, int32 Value, int32 FreePts)
{
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	Row->AddSlot().FillWidth(1.f)
	[
		SNew(STextBlock)
		.Font(RowFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.9f, 1.f)))
		.Text(FText::FromString(FString::Printf(TEXT("%s: %d"), *Name, Value)))
	];

	if (FreePts > 0)
	{
		Row->AddSlot().AutoWidth().Padding(FMargin(12.f, 0.f, 0.f, 0.f))
		[
			SNew(SButton)
			.ContentPadding(FMargin(14.f, 2.f))
			.OnClicked_Lambda([this, Key]()
			{
				if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
				{
					GI->SpendStat(Key);
					RebuildMenu();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(RowFont)
				.Text(FText::FromString(TEXT("+")))
			]
		];
	}

	return Row;
}

void URSLStoryWidget::RebuildMenu()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI || !MenuBox.IsValid() || !MenuTitle.IsValid())
	{
		return;
	}

	MenuBox->ClearChildren();

	const TSharedPtr<FJsonObject> Data = GI->GetGameData();
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
	const FSlateFontInfo HeadFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);

	auto MakeLabel = [RowFont](const FString& Txt,
		const FLinearColor& Col = FLinearColor(0.9f, 0.9f, 0.88f, 1.f)) -> TSharedRef<STextBlock>
	{
		return SNew(STextBlock)
			.Font(RowFont)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Col))
			.Text(FText::FromString(Txt));
	};

	// ---- Панель «Герой» (G) ----
	if (GI->GetOpenMenu() == 1)
	{
		MenuTitle->SetText(FText::FromString(FString::Printf(
			TEXT("ГЕРОЙ · Рудеус Грейрат · %s · ур.%d"), *GI->HeroRankTitle(), GI->Level)));

		// Характеристики (с выдачей очков, как в game.py: 5 характеристик).
		MenuBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			SNew(STextBlock)
			.Font(HeadFont)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)))
			.Text(FText::FromString(FString::Printf(
				TEXT("Характеристики (свободно очков: %d)"), GI->StatPts)))
		];

		static const TCHAR* StatOrder[] = { TEXT("str"), TEXT("dex"), TEXT("int"), TEXT("vit"), TEXT("lck") };
		for (const TCHAR* Key : StatOrder)
		{
			FString Name = Key;
			if (Data.IsValid())
			{
				const TSharedPtr<FJsonObject>* Names = nullptr;
				if (Data->TryGetObjectField(TEXT("stat_names"), Names) && Names)
				{
					FString N;
					if ((*Names)->TryGetStringField(Key, N)) { Name = N; }
				}
			}
			MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 4.f))
			[
				MakeStatRow(Key, Name, GI->Stats.FindRef(Key), GI->StatPts)
			];
		}

		// Опыт / золото / очки навыка.
		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 14.f, 0.f, 0.f))
		[
			MakeLabel(FString::Printf(TEXT("Опыт: %d/%d   Золото: %d   Очки навыка: %d"),
				GI->XP, GI->Level * 120, GI->Gold, GI->SP),
				FLinearColor(0.75f, 0.73f, 0.68f, 1.f))
		];

		// HP/MP и гильдия.
		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 6.f, 0.f, 0.f))
		[
			MakeLabel(FString::Printf(TEXT("HP %d/%d   MP %d/%d   Гильдия: %s (pts %d)"),
				FMath::RoundToInt(GI->HP), FMath::RoundToInt(GI->MaxHP),
				FMath::RoundToInt(GI->MP), FMath::RoundToInt(GI->MaxMP),
				*GI->GuildTitle(), GI->GuildPts),
				FLinearColor(0.75f, 0.73f, 0.68f, 1.f))
		];

		// Инвентарь.
		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 6.f, 0.f, 0.f))
		[
			MakeLabel(FString::Printf(TEXT("Зелья: %d   Большие зелья: %d   Эфиры: %d   Бомбы: %d"),
				GI->Items.FindRef(TEXT("potion")), GI->Items.FindRef(TEXT("hi_potion")),
				GI->Items.FindRef(TEXT("ether")), GI->Items.FindRef(TEXT("bomb"))),
				FLinearColor(0.75f, 0.73f, 0.68f, 1.f))
		];

		// Сюжетные достижения.
		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 6.f, 0.f, 0.f))
		[
			MakeLabel(FString::Printf(TEXT("Заклинаний изучено: %d   Техник: %d   Стилей: %d   Записей: %d   Артефактов: %d"),
				GI->Spells.Num(), GI->Techs.Num(), GI->Styles.Num(),
				GI->Records.Num(), GI->Artifacts.Num()),
				FLinearColor(0.75f, 0.73f, 0.68f, 1.f))
		];

		// Доверие спутников.
		{
			TArray<FString> TrustLine;
			static const TCHAR* TrustOrder[] = { TEXT("silfy"), TEXT("eris"), TEXT("roxy"), TEXT("rujerd") };
			for (const TCHAR* Id : TrustOrder)
			{
				FString Name = Id;
				if (Data.IsValid())
				{
					const TSharedPtr<FJsonObject>* Names = nullptr;
					if (Data->TryGetObjectField(TEXT("trust_names"), Names) && Names)
					{
						FString N;
						if ((*Names)->TryGetStringField(Id, N)) { Name = N; }
					}
				}
				TrustLine.Add(FString::Printf(TEXT("%s %d"), *Name, GI->Trust.FindRef(Id)));
			}
			MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 6.f, 0.f, 0.f))
			[
				MakeLabel(TEXT("Доверие: ") + FString::Join(TrustLine, TEXT("   ")),
					FLinearColor(0.75f, 0.73f, 0.68f, 1.f))
			];
		}

		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 18.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(SmallFont)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.58f, 1.f)))
			.Text(FText::FromString(TEXT("G — закрыть · Магия — K · Карта — M")))
		];
		return;
	}

	// ---- Панель «Магия» (K) ----
	MenuTitle->SetText(FText::FromString(FString::Printf(
		TEXT("МАГИЯ · очки навыка: %d"), GI->SP)));

	// Стихии: ранг, прогресс и тренировка за 1 очко (как в game.py).
	MenuBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[
		SNew(STextBlock)
		.Font(HeadFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)))
		.Text(FText::FromString(TEXT("Стихии")))
	];

	static const TCHAR* ElemOrder[] = { TEXT("fire"), TEXT("water"), TEXT("air"), TEXT("earth") };
	for (const TCHAR* Elem : ElemOrder)
	{
		FString ElemName = Elem;
		if (Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* Names = nullptr;
			if (Data->TryGetObjectField(TEXT("element_names"), Names) && Names)
			{
				FString N;
				if ((*Names)->TryGetStringField(Elem, N)) { ElemName = N; }
			}
		}

		const int32 X = GI->Elements.FindRef(Elem);
		const int32 Need = GI->ElemRankNeed(Elem);
		const FString Row = FString::Printf(TEXT("%s — %s   (%d/%d до следующего ранга)"),
			*ElemName, *GI->ElemRankTitle(Elem), X, Need);

		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		Box->AddSlot().FillWidth(1.f)
		[
			SNew(STextBlock).Font(RowFont)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.9f, 1.f)))
			.Text(FText::FromString(Row))
		];

		if (GI->SP >= 1)
		{
			Box->AddSlot().AutoWidth().Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ContentPadding(FMargin(12.f, 2.f))
				.OnClicked_Lambda([this, Elem]()
				{
					if (URSLGameInstance* G2 = Cast<URSLGameInstance>(GetGameInstance()))
					{
						G2->TrainElement(Elem);
						RebuildMenu();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(SmallFont)
					.Text(FText::FromString(FString::Printf(TEXT("Тренировка: %s (1 очко)"), *ElemName)))
				]
			];
		}

		MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 4.f))
		[
			Box
		];
	}

	// Заклинания: список всех, доступных к изучению.
	MenuBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 8.f))
	[
		SNew(STextBlock)
		.Font(HeadFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 1.f)))
		.Text(FText::FromString(TEXT("Заклинания")))
	];

	if (Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* SpellsObj = nullptr;
		if (Data->TryGetObjectField(TEXT("spells"), SpellsObj) && SpellsObj)
		{
			for (const auto& KV : (*SpellsObj)->Values)
			{
				const TSharedPtr<FJsonObject> D =
					(KV.Value.IsValid() && KV.Value->Type == EJson::Object) ? KV.Value->AsObject() : nullptr;
				if (!D) { continue; }

				FString Name, El, Desc, Req, Flag;
				D->TryGetStringField(TEXT("name"), Name);
				D->TryGetStringField(TEXT("el"), El);
				D->TryGetStringField(TEXT("desc"), Desc);
				D->TryGetStringField(TEXT("req"), Req);
				D->TryGetStringField(TEXT("flag"), Flag);
				int32 MP = 0, Pow = 0, Heal = 0;
				double Num = 0.0;
				if (D->TryGetNumberField(TEXT("mp"), Num)) { MP = (int32)Num; }
				if (D->TryGetNumberField(TEXT("pow"), Num)) { Pow = (int32)Num; }
				if (D->TryGetNumberField(TEXT("heal"), Num)) { Heal = (int32)Num; }

				FString ElName = El;
				if (Data.IsValid())
				{
					const TSharedPtr<FJsonObject>* Names = nullptr;
					if (Data->TryGetObjectField(TEXT("element_names"), Names) && Names)
					{
						FString N;
						if ((*Names)->TryGetStringField(El, N)) { ElName = N; }
					}
				}

				const bool bKnown = GI->Spells.Contains(KV.Key);
				FString Info = FString::Printf(TEXT("%s (%s, MP %d, %s) — %s"),
					*Name, *ElName, MP,
					Heal > 0 ? *FString::Printf(TEXT("лечение %d"), Heal)
					         : *FString::Printf(TEXT("урон %d"), Pow),
					*Desc);
				if (!bKnown)
				{
					if (!Req.IsEmpty() && !GI->Spells.Contains(Req))
					{
						Info += TEXT("   [нужно: предыдущее заклинание стихии]");
					}
					else if (!Flag.IsEmpty() && !GI->Flags.Contains(Flag))
					{
						Info += TEXT("   [нужен сюжетный ключ — учёба в университете]");
					}
				}

				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				Row->AddSlot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(SmallFont)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(bKnown
						? FLinearColor(0.55f, 0.75f, 0.55f, 1.f)
						: FLinearColor(0.88f, 0.88f, 0.86f, 1.f)))
					.Text(FText::FromString(bKnown ? TEXT("✓ ") + Info : Info))
				];

				if (bKnown)
				{
					Row->AddSlot().AutoWidth().Padding(FMargin(12.f, 0.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Font(SmallFont)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.75f, 0.55f, 1.f)))
						.Text(FText::FromString(TEXT("изучено")))
					];
				}
				else
				{
					const bool bReqOk = Req.IsEmpty() || GI->Spells.Contains(Req);
					const bool bFlagOk = Flag.IsEmpty() || GI->Flags.Contains(Flag);
					const bool bCan = bReqOk && bFlagOk && GI->SP >= 1;

					Row->AddSlot().AutoWidth().Padding(FMargin(12.f, 0.f, 0.f, 0.f))
					[
						SNew(SButton)
						.ContentPadding(FMargin(12.f, 2.f))
						.IsEnabled(bCan)
						.OnClicked_Lambda([this, Id = KV.Key]()
						{
							if (URSLGameInstance* G2 = Cast<URSLGameInstance>(GetGameInstance()))
							{
								G2->LearnSpell(Id);
								RebuildMenu();
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Font(SmallFont)
							.Text(FText::FromString(TEXT("Изучить (1 очко)")))
						]
					];
				}

				MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 5.f))
				[
					Row
				];
			}
		}
	}

	MenuBox->AddSlot().AutoHeight().Padding(FMargin(12.f, 18.f, 0.f, 0.f))
	[
		SNew(STextBlock)
		.Font(SmallFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.58f, 1.f)))
		.Text(FText::FromString(TEXT("K — закрыть · Герой — G · Карта — M")))
	];
}
