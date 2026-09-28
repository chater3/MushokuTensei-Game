#include "RebornSecondLife.h"

DEFINE_LOG_CATEGORY_STATIC(LogRebornSecondLife, Log, All);

void FRebornSecondLifeModule::StartupModule()
{
	UE_LOG(LogRebornSecondLife, Log, TEXT("REBORN: SECOND LIFE модуль запущен"));
}

void FRebornSecondLifeModule::ShutdownModule()
{
}

IMPLEMENT_PRIMARY_GAME_MODULE(FRebornSecondLifeModule, RebornSecondLife, "RebornSecondLife");
