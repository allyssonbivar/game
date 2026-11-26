#include "EAWHUD.h"
#include "EAWCharacter.h"
#include "Effects/EAWBloodFX.h"
#include "Combat/MeleeCombatComponent.h"
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

	// Golpe especial: barra de carga (segurar o ataque com W por 3 s); cheia = solta com W.
	if (Soldier->GetCombat() && Soldier->GetCombat()->IsCharging())
	{
		const float P = Soldier->GetCombat()->GetChargeAlpha();
		const float BarW = 260.f, BarX = W * 0.5f - BarW * 0.5f, BarY = H * 0.62f;
		UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		const FString Label = P >= 1.f ? TEXT("GOLPE ESPECIAL - W para soltar") : TEXT("Golpe especial...");
		float TW = 0.f, TH = 0.f;
		GetTextSize(Label, TW, TH, Font, 1.f);
		DrawText(Label, P >= 1.f ? FLinearColor(1.f, 0.35f, 0.25f) : FLinearColor(1.f, 0.92f, 0.7f), W * 0.5f - TW * 0.5f, BarY - TH - 4.f, Font, 1.f);
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.85f), BarX, BarY, BarW, 9.f);
		DrawRect(P >= 1.f ? FLinearColor(0.9f, 0.15f, 0.1f, 1.f) : FLinearColor(0.95f, 0.75f, 0.3f, 1.f), BarX + 1.f, BarY + 1.f, (BarW - 2.f) * P, 7.f);
	}

	// Combo: mostra o passo (2o golpe) e o golpe final (3o clique ou 2 cliques + E).
	if (UMeleeCombatComponent* C = Soldier->GetCombat())
	{
		const EMeleeState S = C->GetState();
		if (!C->IsCharging() && C->GetComboStep() > 0 && (S == EMeleeState::Windup || S == EMeleeState::Release))
		{
			UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
			const FString Label = C->GetComboStep() >= 2 ? TEXT("GOLPE FINAL!") : TEXT("Combo 2");
			float TW = 0.f, TH = 0.f;
			GetTextSize(Label, TW, TH, Font, 1.2f);
			DrawText(Label, C->GetComboStep() >= 2 ? FLinearColor(1.f, 0.4f, 0.2f) : FLinearColor(1.f, 0.92f, 0.7f),
				W * 0.5f - TW * 0.5f, H * 0.66f, Font, 1.2f);
		}
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
