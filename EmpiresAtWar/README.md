# Empires at War — protótipo de combate corpo a corpo

Unreal Engine 4.27 · C++ · compilador MSVC 14.29 (fixado nos `*.Target.cs`) · Windows SDK 10.0.19041

## Como testar
1. Abra `EmpiresAtWar.uproject` (ou rode o comando de compilação abaixo).
2. Aperte **Play**. Um bot de treino aparece na sua frente.

| Tecla | Ação |
|---|---|
| Botão esquerdo | Corte horizontal (alterna esquerda/direita) |
| `E` / roda para cima | Golpe de cima |
| `C` / roda para baixo | Estocada |
| Botão direito | Parry |
| `Q` | Finta (só durante a preparação) |
| `Shift` | Correr |
| `B` | Novo bot |
| `H` | Mostrar/esconder hitboxes |
| Console `EAWReset` | Reinicia todos os soldados |
| Console `EAWBot` | Cria um bot |

Log detalhado do combate: `-LogCmds="LogEAWCombat Verbose"`.

## Como funciona
- `Combat/MeleeCombatComponent` — máquina de estados: Guarda → Preparação → Golpe → Recuperação.
  Finta, troca de golpe, combo, parry com contra-ataque, guarda quebrada sem stamina.
  O servidor decide; a lâmina é rastreada quadro a quadro durante o golpe (`ECC_Melee`).
- `Combat/WoundComponent` — ferimentos da seção 9 do documento: cabeça/pescoço vitais,
  2º ferimento não vital incapacita; braço, perna e tronco feridos têm penalidades.
- `Combat/MeleeTypes.h` — tempos, custos e poses de cada arma (valores iniciais de um sabre).
  Para outra arma, crie um Data Asset `MeleeWeaponData`.
- A arma se move por código (poses) até existirem animações próprias.
  As hitboxes são cápsulas por região até existir um esqueleto próprio.

## Compilar pela linha de comando
```
"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\DotNET\UnrealBuildTool.exe" EmpiresAtWarEditor Win64 Development -Project="<caminho>\EmpiresAtWar.uproject" -WaitMutex
```
