#include "EAWHUD.h"
#include "EAWCharacter.h"
#include "Effects/EAWBloodFX.h"
#include "Weapons/EAWDroppedWeapon.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

void AEAWHUD::DrawHUD()
{
	Super::DrawHUD();
	const AEAWCharacter* Soldier = PlayerOwner ? Cast<AEAWCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (!Soldier || !Canvas)
	{
		return;
	}
	const float W = Canvas->SizeX, H = Canvas->SizeY;

	// Tela vermelha ao levar dano.
	const float Flash = AEAWBloodFX::IsGoreEnabled() ? Soldier->GetDamageFlash() : 0.f;
	if (Flash > 0.01f)
	{
		// Borda mais forte que o centro: quatro faixas + um veu leve na tela toda.
		const FLinearColor Veil(0.55f, 0.f, 0.f, 0.22f * Flash);
		const FLinearColor Edge(0.45f, 0.f, 0.f, 0.45f * Flash);
		DrawRect(Veil, 0.f, 0.f, W, H);
		const float B = 0.12f * FMath::Min(W, H);
		DrawRect(Edge, 0.f, 0.f, W, B);
		DrawRect(Edge, 0.f, H - B, W, B);
		DrawRect(Edge, 0.f, B, B, H - 2.f * B);
		DrawRect(Edge, W - B, B, B, H - 2.f * B);
	}

	// Arma no chao: aviso no centro e barra de progresso (segurar F por 3 s).
	if (const AEAWDroppedWeapon* Target = Soldier->GetPickupTarget())
	{
		const EEAWWeapon New = Target->GetWeapon();
		const EEAWWeapon Old = AEAWCharacter::IsFirearmWeapon(New) ? Soldier->GetPrimaryWeapon() : Soldier->GetSecondaryWeapon();
		FString Line = FString::Printf(TEXT("Segure [F] para pegar: %s"), *AEAWDroppedWeapon::WeaponName(New).ToString());
		if (New == EEAWWeapon::Mosquete)
		{
			Line += Target->IsLoaded() ? TEXT(" (carregado)") : TEXT(" (descarregado)");
		}
		const FString Sub = FString::Printf(TEXT("larga: %s"), *AEAWDroppedWeapon::WeaponName(Old).ToString());
		UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		float TW = 0.f, TH = 0.f, SW = 0.f, SH = 0.f;
		GetTextSize(Line, TW, TH, Font, 1.f);
		GetTextSize(Sub, SW, SH, Font, 0.85f);
		const float Y = H * 0.58f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), W * 0.5f - TW * 0.5f - 12.f, Y - 6.f, TW + 24.f, TH + SH + 34.f);
		DrawText(Line, FLinearColor(1.f, 0.92f, 0.7f), W * 0.5f - TW * 0.5f, Y, Font, 1.f);
		DrawText(Sub, FLinearColor(0.8f, 0.8f, 0.8f), W * 0.5f - SW * 0.5f, Y + TH + 2.f, Font, 0.85f);
		const float BarW = FMath::Max(220.f, TW), BarX = W * 0.5f - BarW * 0.5f, BarY = Y + TH + SH + 10.f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.85f), BarX, BarY, BarW, 9.f);
		const float P = Soldier->GetPickupProgress();
		if (P > 0.f)
		{
			DrawRect(FLinearColor(0.95f, 0.75f, 0.3f, 1.f), BarX + 1.f, BarY + 1.f, (BarW - 2.f) * P, 7.f);
		}
	}
}
