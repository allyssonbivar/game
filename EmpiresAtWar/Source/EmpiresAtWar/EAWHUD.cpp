#include "EAWHUD.h"
#include "EAWCharacter.h"
#include "Effects/EAWBloodFX.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void AEAWHUD::DrawHUD()
{
	Super::DrawHUD();
	const AEAWCharacter* Soldier = PlayerOwner ? Cast<AEAWCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (!Soldier || !Canvas || !AEAWBloodFX::IsGoreEnabled())
	{
		return;
	}
	const float Flash = Soldier->GetDamageFlash();
	if (Flash > 0.01f)
	{
		// Borda mais forte que o centro: quatro faixas + um veu leve na tela toda.
		const float W = Canvas->SizeX, H = Canvas->SizeY;
		const FLinearColor Veil(0.55f, 0.f, 0.f, 0.22f * Flash);
		const FLinearColor Edge(0.45f, 0.f, 0.f, 0.45f * Flash);
		DrawRect(Veil, 0.f, 0.f, W, H);
		const float B = 0.12f * FMath::Min(W, H);
		DrawRect(Edge, 0.f, 0.f, W, B);
		DrawRect(Edge, 0.f, H - B, W, B);
		DrawRect(Edge, 0.f, B, B, H - 2.f * B);
		DrawRect(Edge, W - B, B, B, H - 2.f * B);
	}
}
