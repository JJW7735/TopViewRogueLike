#include "RLSaveGame.h"

URLSaveGame::URLSaveGame()
{
	SaveSlotName = TEXT("RL_SaveData");
	UserIndex = 0;
	RunData.bIsRunActive = false;
	RunData.StageNumber = 1;
	RunData.SaveTimestamp = FDateTime::Now();
}
