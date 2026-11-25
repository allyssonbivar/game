#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EAWHUD.generated.h"

/** HUD minimo: tela vermelha quando o soldado do jogador e atingido (some em ~0,5 s). */
UCLASS()
class EMPIRESATWAR_API AEAWHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
