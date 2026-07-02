#include "PartySettingsGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"

const int32 APartySettingsGameMode::GamesPerSessionPresets[] = { 3, 5, 10, 15, 20 };

APartySettingsGameMode::APartySettingsGameMode()
{
    SelectedOption            = 0;
    GamesPerSessionPresetIndex = 2; // default to 10
}

FString APartySettingsGameMode::GetOptionLabel(int32 OptionIndex) const
{
    switch (OptionIndex)
    {
    case 0: return TEXT("Games Per Session");
    case 1: return TEXT("Keyboard Emulation");
    default: return TEXT("???");
    }
}

FString APartySettingsGameMode::GetOptionValue(int32 OptionIndex) const
{
    const UPartySessionSubsystem* S = Session();
    switch (OptionIndex)
    {
    case 0:
        return FString::Printf(TEXT("%d"), S ? S->GetGamesPerSession() : 10);
    case 1:
        return TEXT("(dev toggle — restart required)");
    default:
        return TEXT("");
    }
}

void APartySettingsGameMode::OnPlayerButton(int32 PlayerIndex)
{
    SelectedOption = (SelectedOption + 1) % NUM_OPTIONS;
    UE_LOG(LogPartyButtons, Log,
        TEXT("APartySettingsGameMode: selection -> %d (player %d)"),
        SelectedOption, PlayerIndex + 1);
}

void APartySettingsGameMode::OnMainTap()
{
    ActivateSelectedOption();
}

void APartySettingsGameMode::OnMainHold()
{
    UE_LOG(LogPartyButtons, Log, TEXT("APartySettingsGameMode: back to MainMenu."));
    TravelToPhase(EPartyPhase::MainMenu);
}

void APartySettingsGameMode::ActivateSelectedOption()
{
    UPartySessionSubsystem* S = Session();
    switch (SelectedOption)
    {
    case 0: // Cycle Games Per Session through presets.
    {
        const int32 NumPresets = static_cast<int32>(UE_ARRAY_COUNT(GamesPerSessionPresets));
        GamesPerSessionPresetIndex = (GamesPerSessionPresetIndex + 1) % NumPresets;
        const int32 NewValue = GamesPerSessionPresets[GamesPerSessionPresetIndex];
        if (S) { S->SetGamesPerSession(NewValue); }
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartySettingsGameMode: GamesPerSession -> %d"), NewValue);
        break;
    }
    case 1: // Keyboard emulation toggle is a runtime controller property — note only.
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartySettingsGameMode: Keyboard emulation toggle (restart required; set bEnableKeyboardEmulation on the PC subclass)."));
        break;
    default:
        break;
    }
}
