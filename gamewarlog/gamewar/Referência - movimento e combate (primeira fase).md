---
tags: [referencia, estudo]
atualizado: 2026-11-26
---
# Referência — movimento, ataque e defesa (primeira fase)

Vídeos em `Modelos\ex\` (fora do Git). **Só estudo:** são de um pacote pago de animações (RPG Animations Pack). Não copiar animações nem curvas. Nossas animações são feitas do zero no Blender.

## baseMove (4 min) — movimento base com escudo + espada
- Transições entre **parado, andar, correr e virar**: Idle → andar, andar → Idle, virar no lugar (90° e 180°) e "andar virando" (Walk_C_F_Turn_L90, Jogging_F_Turn_R90).
- **Andar e correr em 8 direções**: frente, trás, lado e diagonais (L45, R45, L90, R90, R135). Hoje temos só 4 direções (frente, trás, esq., dir.).
- Correr para trás e para os lados (Jogging_B_L45, Jogging_B_To_Idle).
- **Ponto de melhoria:** as transições de pé parado para o andar e vice-versa, e as diagonais.

## ataqueOnehand (47 s) — uma mão (espada com escudo)
- Ataques: Attack_F_1, Attack_F_2 (ataque frontal).
- Combos: F_Combo_12, F_Combo_123 → **não mexer** (pedido).
- **Ataque de salto:** Jump_Attack_1 (preparação → parado no ar → aterrissagem, com Hold e Air_Loop).
- **Ataque com escudo:** Shield_Attack_1, Shield_Attack_3 (empurrão/golpe com o escudo), Shield_Attack_Combo12/123 (combos → não mexer).
- **Ponto de melhoria:** falta o golpe com o escudo (empurrão), e o ataque de salto.

## thohamds (1 min 31) — duas mãos (espada longa)
- Ataques: Attack_C_Combo_12 (combo → não mexer), Attack_Dash_1 (investida).
- **Habilidade com carga** (Skill_2, Skill_4, Skill_5 = Start / Loop / End): é o equivalente do nosso golpe especial. **Não mexer** no especial.
- Ataque de salto (Jump_Attack, Jump_Attack_Hold, Air_Loop, End).
- **Defesa em movimento:** Block_Run_B / F / L45 / R45 / L90 / R90 e Block_Walk_B / F / L45 / L90. O bloqueio com a espada acima da cabeça é mantido enquanto corre ou anda, em todas as direções. Hoje a nossa defesa é uma pose parada.
- **Ponto de melhoria (defesa):** bloqueio em movimento (andando e correndo) e as direções diagonais do bloqueio.

## Resumo para a primeira fase (sem mexer nos combos)
1. **Movimento:** andar e correr em 8 direções (diagonais), transições parado↔andar e virar 90°/180°.
2. **Ataques:** ataque de salto (uma mão e duas mãos) e golpe de escudo (uma mão). Dash frontal (investida).
3. **Defesas:** bloqueio em movimento (andando e correndo) nas 8 direções.
4. **Fora do escopo agora:** combos (Combo_12, Combo_123, C_Combo_12, Shield_Attack_Combo*) e a habilidade com carga (Skill_5 = nosso especial).
