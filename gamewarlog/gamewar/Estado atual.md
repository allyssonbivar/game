---
tags: [estado]
atualizado: 2026-11-24
---
# Estado atual

## Unreal — protótipo de combate
Compila e foi testado sem janela (bot contra bot). Ver `EmpiresAtWar\README.md`.
- `MeleeCombatComponent`: Guarda → Preparação → Golpe → Recuperação; finta, troca de golpe, combo,
  parry com contra-ataque, guarda quebrada sem stamina. O servidor decide; a lâmina é rastreada quadro a quadro.
- `WoundComponent`: cabeça/pescoço vitais; 2º ferimento não vital incapacita; penalidades por braço, perna e tronco.
- Bot de treino, hitboxes por região, arma ainda procedural (sem animação).
- Tempos do sabre (30 fps): preparação 0,45 s · golpe 0,30 s · recuperação 0,55 s.
- Compilador fixado em MSVC 14.29; Windows SDK 10.0.19041.
- Teste sem janela: `UE4Editor-Cmd.exe <uproject> -game -nullrhi -forcelogflush -LogCmds="LogEAWCombat Verbose"`
  (+ `-ExecCmds="EAWBot"` para bot contra bot) e ler `Saved\Logs\EmpiresAtWar.log`.

## Unreal — personagem animado (passo 2, 2026-11-24) ✔
- `/Game/Personagem/`: `SK_Soldado` (180,5 cm, olha para +Y → girado −90° no personagem), esqueleto, física (ragdoll),
  8 animações `AS_Sabre_*`, materiais. Importado por script (ver receita em [[Bugs resolvidos]]).
- O `AEAWCharacter` usa o esqueleto e as animações; o bloco e o cilindro provisórios ficam escondidos.
- Cada estado do combate toca seu trecho da animação **ajustado ao tempo do código**
  (ex.: preparação = quadros 1–15 do corte, golpe = 15–24, recuperação = 24–41).
- Acerto pela lâmina real: do osso `weapon_r` (+7,5 cm) até `weapon_r_tip`.
- Incapacitado/morto → **ragdoll**; hitboxes desligadas.
- **1ª pessoa (2026-11-24):** `FirstPersonMesh` = cópia do soldado presa à câmera, vista **só pelo dono**, sem
  cabeça/pescoço e pernas, tocando as **mesmas animações** do corpo. Posição: inclinada 14° para cima,
  deslocada (X −14, Y +10, Z +14 cm) — ombro atrás da câmera, sabre à direita. FOV 95. O dono não vê o corpo
  inteiro (vê a sombra). Ao cair, os braços somem e o dono vê o ragdoll. Acerto continua pelo corpo inteiro no servidor.
  Ajuste sem recompilar: `-EAWFPPitch= -EAWFPX= -EAWFPY= -EAWFPZ= -EAWFPFov=`.
  Os braços tocam **versões próprias** `AS_Sabre_*_FP` (corte esq./dir., de cima, parry, bloqueado, finta);
  guarda e estocada usam a de corpo inteiro. Mesmos quadros-chave → mesmos tempos do combate.
- **Testes:** `-EAWBotPassivo` (bot parado) e `-EAWAutoAtaque` (seu soldado ataca sozinho em ciclo).
- **Animador em camadas (2026-11-24):** `Animation/EAWAnimInstance` (C++, sem Animation Blueprint).
  Pernas = mistura de andar 4 direções + correr, com o passo sincronizado à velocidade real; tronco/braços
  (spine_01 e filhos) = trecho de combate; parado = corpo inteiro no combate; correndo sem lutar = tronco também corre.
  Transição de 0,1 s entre trechos de combate (fim do "pulo" de pose). Braços de 1ª pessoa continuam no modo simples.
- **Velocidades:** andar 1,4 m/s, para trás ×0,71 (1,0), de lado ×0,61 (0,85), correr 4,0 m/s (antes 3,5 e 5,6).
- Animações de movimento importadas (19 animações no total). Testado em duelo: pernas andam com o tronco em guarda ✔.
- **Morte/incapacitado:** ragdoll + cápsula sem colisão + velocidades zeradas + amortecimento + empurrão na
  direção do último golpe → cai em ~0,5 s e fica quieto.
- **Peso e física (2026-11-25):**
  - Movimento com inércia (aceleração 650, frenagem 700, atrito 5); atacando anda mais devagar (×0,35–0,7); sem correr atacando.
  - Giro limitado durante o golpe: 220°/s na preparação, 110°/s no golpe (mouse e olhar).
  - Passo à frente no início do golpe: 160 cm/s (corte), 240 cm/s (estocada).
  - Impacto: trava de 0,07 s (0,09 s no parry) em atacante e atingido; atingido é empurrado e o **tronco vira física
    por ~0,35 s** (reage na direção do golpe e volta à animação); parry empurra os dois.
  - Tremor de câmera (dono) proporcional ao impacto; quem acerta também sente.
- Bot: luta a 120–150 cm.
- Plugins ativos: Python Editor Script, Editor Scripting Utilities, **ACL (Animation Compression Library) 2.1.0**
  em `EmpiresAtWar\Plugins\ACLPlugin` (2026-11-24). Feito para 4.25: marcado como 4.27 na cópia do projeto;
  compila sem erros. Reabrir o editor para carregar. Uso: comprimir as **nossas** animações.
- **Modo de teste:** `-EAWDuelo` (dois bots e câmera fixa de lado), `-EAWShots=N -EAWShotEvery=0.1 -EAWShotDelay=0.4`
  (capturas em `Saved\Testes\`, fecha sozinho). Ver [[Testes de animação]].

## Sangue e desmembramento (2026-11-25) ✔ testado em duelo
- **Gotas** (`Effects/EAWBloodFX`, C++, sem Niagara): borrifo a cada acerto não defendido (25 gotas; 40 na cabeça/pescoço),
  gravidade e arrasto, gota esticada na direção do movimento. Só visual (cada máquina desenha o seu; nada no servidor dedicado).
- **Manchas no chão:** decal `M_Sangue_Mancha` onde as gotas caem (35% delas), somem em 40 s, **máximo 60** no mundo.
- **Desmembramento no golpe que derruba:** cabeça/pescoço → **decapitação** (`neck_01`); braço → braço inteiro do lado
  atingido (`upperarm_l/r`, o direito leva o sabre junto); perna → **canela** (`calf_l/r`). Tronco → sem corte
  (corpo partido fica para canhão/golpes muito fortes).
- Como funciona: no corpo, o osso cortado é escondido (`HideBoneByName`, a pele encolhe até a junta) e os corpos físicos dele saem;
  a peça é uma malha separada (`/Game/Personagem/Pecas/SK_Peca_*`, mesmo esqueleto, só os vértices do membro) que nasce na mesma pose
  e voa com física (empurrão na direção do golpe + para cima + giro).
- **Tampas vermelhas** (disco achatado `M_Sangue`) nos dois lados do corte; **jato pulsante** do coto por 1,6 s (e da peça por 0,7 s).
- Limite de **20 peças soltas** no mundo (as mais antigas somem). Reset (`EAWReset`) devolve o membro e apaga as peças.
- **Tela vermelha** (`AEAWHUD`): véu + bordas quando o seu soldado é atingido, some em ~0,7 s.
- **`-EAWSemSangue`**: desliga gotas, manchas, cortes e tela vermelha (opção do jogador pedida no documento).
- **Teste:** `-EAWTesteCorte=Cabeca|Braco|Perna` faz qualquer golpe contar como essa região para o corte.
- **Física do esqueleto refeita:** 18 corpos com limites (antes 4) — o ragdoll agora dobra braços e pernas (bug #41).

## Blender — personagem `SK_Soldado` (`Modelos\Soldado.blend`, cena **Soldado**)
- Y Bot do Mixamo, 180,5 cm, pés no zero, frente = **−Y**, ~55 mil triângulos.
- Ossos com nomes do padrão Unreal (`pelvis`, `spine_01..03`, `upperarm_l`...), + `root`, `weapon_l/r`.
- `weapon_r/l` deslocados 2,5 cm para o lado da palma.
- Controles (não deformam): `ctrl_hand_l/r`, `ctrl_foot_l/r`, `pole_arm_l/r`, `pole_leg_l/r`, `twist_target_l/r`.
- IK em braços e pernas; cotovelo com pré-dobra de 2 cm; antebraço assume 50% da torção da mão.
- Sabre provisório `SM_Sabre` preso em `weapon_r` (lâmina 82 cm).
- Exportar para o Unreal com **"Only Deform Bones"** (receita completa em [[Bugs resolvidos]]).
- **Escala 1** desde 2026-11-24 (a escala 0,01 do Mixamo foi aplicada). Backup antes disso: `Modelos\Soldado_backup_antes_escala.blend`.
- Osso `weapon_r_tip` na ponta da lâmina; `SKM_Sabre_Mao` = cópia do sabre presa ao osso `weapon_r` (vai no FBX).

## Animações prontas — conjunto de teste do sabre ✔ (testes em [[Testes de animação]])
| Ação | Quadros (30 fps) | Conteúdo |
|---|---|---|
| `AS_Sabre_Guarda` | 1–60 (loop) | Guarda + respiração |
| `AS_Sabre_Corte_Esq` | 1–41 (prep 15, impacto 20, fim 24) | Corte da direita para a esquerda, palma para cima |
| `AS_Sabre_Corte_Dir` | 1–41 (prep 15, impacto 20, fim 24) | Revés da esquerda para a direita, palma para baixo |
| `AS_Sabre_DeCima` | 1–45 (prep 17, impacto 22, fim 27) | Lâmina armada atrás da cabeça, corta para baixo |
| `AS_Sabre_Estocada` | 1–35 (prep 13, meio 17, extensão 20) | Recua à cintura e avança com o quadril |
| `AS_Sabre_Parry` | 1–20 (defesa 5–10) | Lâmina cruzada na frente do rosto |
| `AS_Sabre_Bloqueado` | 1–17 | Do impacto do corte → ricochete para cima → guarda |
| `AS_Sabre_Finta` | 1–19 (prep 11) | Prepara o corte e volta à guarda |
Todas começam e terminam na guarda. Golpes aceleram até o impacto (ease-in) e freiam depois (ease-out).

## Animações de movimento (Blender, 2026-11-24) — no Unreal desde 2026-11-24
| Ação | Quadros | Velocidade que a animação pede | Observação |
|---|---|---|---|
| `AS_Sabre_Andar_Frente` | 1–25 (loop 24) | 1,40 m/s | passo curto de luta, sabre em guarda |
| `AS_Sabre_Andar_Tras` | 1–25 (loop 24) | 1,00 m/s | tronco mais ereto |
| `AS_Sabre_Andar_Esq` / `_Dir` | 1–25 (loop 24) | 0,85 m/s | pés afastados, sem cruzar |
| `AS_Sabre_Correr` | 1–19 (loop 18) | 4,00 m/s | sabre apoiado no ombro, braço esquerdo balança |
- Geradas por fórmula (`loco_pose`/`make_cycle` na `eaw_lib`): pé no chão ~55% do ciclo (33% na corrida), deslizando para trás
  exatamente na velocidade do corpo; no ar sobe 10 cm (16 cm correndo); quadril sobe/desce 2× por ciclo.
- **Limite físico:** a perna alcança ~33 cm à frente do quadril (com o corpo abaixado da guarda) — por isso o passo é curto.
- **No Unreal, a velocidade do personagem precisa bater** com esses números (hoje o código usa 3,5 m/s andando e 5,6 m/s correndo).

## ⚠ Como editar animações desde o passe fino (2026-11-24)
- As **fontes editáveis** são as ações `SRC_AS_*` (com controles de IK). As ações `AS_*` são **assadas** (pose final
  por quadro) **+ passe de mola** — são as que vão para o Unreal.
- Para mudar um golpe: `rig_mode("ik")` → editar a `SRC_...` → assar de novo → `spring_pass` → `rig_mode("fk")` → exportar.
- Para tocar as `AS_*` no Blender: `rig_mode("fk")` (restrições desligadas), senão o IK sobrescreve a pose.
- Backup antes do passe: `Modelos\Soldado_backup_antes_passe_fino.blend`.

## Ferramentas dentro do `Soldado.blend` (Text Editor)
- `eaw_lib` — funções: `body()`, `hand()`/`best_hand()` (lâmina exata + menor torção), `guard()`, `fingers()`,
  `key()`, `new_action()`, `ease()`, `measure()`. Uso: `exec(bpy.data.texts["eaw_lib"].as_string(), g)`.
- `eaw_poses` — poses-chave de cada golpe (para refazer/ajustar).
- `eaw_fp` — posições da mão **na tela** (espaço da câmera) usadas nas versões `_FP`.
- Na `eaw_lib`: `to_cam`/`from_cam` (câmera de 1ª pessoa igual à do Unreal) e `measure_fp` (quadros fora da tela).
- 14 ações: 8 de corpo inteiro + 6 `_FP`.

## Empunhadura do sabre
- Cabo **na diagonal da palma (30°)** — `weapon_r/l` girados; lâmina em repouso = (−0,49, −0,87, 0).
- Dedos (mão direita, eixo X positivo = fechar): indicador 62/85/55°, médio 66/84/50°, anelar 72/84/48°, mínimo 88/90/55°;
  polegar sobre o indicador (`thumb_01` Euler −60/15/30°, `02` 60°, `03` 42°). Mão esquerda relaxada.
