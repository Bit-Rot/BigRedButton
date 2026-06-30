#include "PartyButtonsHUD.h"
#include "PartyInputController.h"
#include "PartyButtons.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

APartyButtonsHUD::APartyButtonsHUD()
{
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        ButtonLastPressed[i] = -10.f;  // far in the past; never lit on startup
    }
}

void APartyButtonsHUD::BeginPlay()
{
    Super::BeginPlay();

    if (APartyInputController* PC = Cast<APartyInputController>(GetOwningPlayerController()))
    {
        PC->OnButtonPressed.AddUObject(this, &APartyButtonsHUD::HandleButtonPressed);
        UE_LOG(LogPartyButtons, Log, TEXT("PartyButtonsHUD bound to OnButtonPressed."));
    }
    else
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("PartyButtonsHUD: owning controller is not APartyInputController. HUD will not respond to button presses."));
    }
}

void APartyButtonsHUD::HandleButtonPressed(int32 PlayerIndex)
{
    if (PlayerIndex >= 0 && PlayerIndex < NUM_BUTTONS)
    {
        ButtonLastPressed[PlayerIndex] = GetWorld()->GetTimeSeconds();
    }
}

void APartyButtonsHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas)
    {
        return;
    }

    const float Now       = GetWorld()->GetTimeSeconds();
    const float StartX    = 40.f;
    const float StartY    = 40.f;
    const float Step      = CELL_SIZE + CELL_MARGIN;

    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        const int32 Row = i / GRID_COLS;
        const int32 Col = i % GRID_COLS;
        const float X   = StartX + Col * Step;
        const float Y   = StartY + Row * Step;

        const bool bLit = (Now - ButtonLastPressed[i]) < FLASH_DURATION;

        // Background box
        const FLinearColor BoxColor = bLit
            ? FLinearColor(0.1f, 0.9f, 0.2f, 1.f)    // lit: bright green
            : FLinearColor(0.15f, 0.15f, 0.15f, 0.8f); // unlit: dark grey

        DrawRect(BoxColor, X, Y, CELL_SIZE, CELL_SIZE);

        // Border
        const FLinearColor BorderColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);
        DrawLine(X,              Y,              X + CELL_SIZE, Y,              BorderColor);
        DrawLine(X + CELL_SIZE,  Y,              X + CELL_SIZE, Y + CELL_SIZE,  BorderColor);
        DrawLine(X + CELL_SIZE,  Y + CELL_SIZE,  X,             Y + CELL_SIZE,  BorderColor);
        DrawLine(X,              Y + CELL_SIZE,  X,             Y,              BorderColor);

        // Button number label (1-based, matching physical button numbering)
        const FString NumLabel = FString::Printf(TEXT("%d"), i + 1);
        DrawText(NumLabel, FLinearColor::White, X + 6.f, Y + 6.f);

        // Keyboard emulation key label (bottom of cell): a w s e d r f g h j i k o l p ;
        const TArray<FKey>& EmulKeys = APartyInputController::GetKeyboardEmulationKeys();
        if (EmulKeys.IsValidIndex(i))
        {
            const FString KeyLabel = EmulKeys[i].GetDisplayName(/*bLongDisplayName=*/ false).ToString();
            DrawText(KeyLabel, FLinearColor(0.7f, 0.9f, 1.f, 1.f),
                     X + CELL_SIZE * 0.35f, Y + CELL_SIZE * 0.62f);
        }
    }

    // Title (mentions keyboard emulation)
    DrawText(TEXT("PartyButtons — keyboard: awsedrfghujikolp"), FLinearColor(0.8f, 0.8f, 0.8f, 1.f), StartX, StartY + GRID_ROWS * Step + 10.f);
}
