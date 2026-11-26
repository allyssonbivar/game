---
tags: [estado]
atualizado: 2026-11-26
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

## Movimento e giro "profissional" (2026-11-25, referência medida no Chivalry)
- Medido no vídeo: giro em luta 100–130°/s; câmera balança ~1,5 Hz (andar) e ~3,6 Hz (correr), 1–1,8°; a arma atrasa ~1–2° a 100°/s (~70 ms).
- **Velocidades:** trote 3,0 m/s (trás 1,95, lado 2,25), corrida 5,0 m/s; arranque/freada ~0,15 s. Animações `AS_Trote_*` (ciclo 0,633 s, 4 direções); corrida só para a frente.
- **Giro do corpo:** a câmera gira na hora; o corpo segue a 540°/s andando/lutando; parado, os pés só viram depois de 70° (320°/s, com passinhos de lado) e até lá o tronco e a arma torcem na cintura (`EAWAnimInstance`).
- **1ª pessoa viva:** arma atrasa o giro, balanço do passo (arma e câmera), inclinação de 1,5° andando de lado, câmera afunda ao aterrissar. Mirando com o mosquete, quase tudo isso some (sobra o balanço da mira).
- Andar usa a direção da câmera (não a do corpo).

## Armas (2026-11-25)
- **Regra (decisão do usuário, 2026-11-25): o soldado carrega só 2 armas** — 1 de fogo (mosquete ou pistola) + 1 de corpo a corpo (sabre, faca ou machado). **1** = arma de fogo, **2** = corpo a corpo (troca "desce/sobe" de ~0,4 s, sem golpe até subir). **X** alterna o mosquete entre tiro e combate.
- **Pegar do chão:** olhar para a arma a até 2,2 m → aviso "Segure [F] para pegar: … / larga: …" + barra; **segurar F por 3 s**. A nova ocupa a vaga do mesmo tipo e a antiga cai no chão (com física). Soltar F, olhar para outra ou se afastar cancela. Mosquete largado guarda se estava carregado.
- **Ao morrer**, o soldado larga a arma da mão (cai com física e pode ser pega).
- **Mosquete no combate (X):** botão esquerdo = coronhada (atordoa, dano 38), E/roda cima = estocada longa (alcance ~2,1 m, mão esquerda solta, dano 62), C/roda baixo = estocada (~1,9 m, 55), direito = aparo (à direita ou à esquerda conforme o golpe do inimigo), Q = finta. Animações a partir da imagem "Exercice de la baïonnette". A coronhada fere com a coronha; as estocadas com a baioneta.
- **Faca** (lâmina 20 cm, golpes 30% mais rápidos) e **machado de sapador** (cabo 70 cm, corta com a cabeça, 20% mais lento, dano alto) usam as animações de uma mão do sabre com tempos/dano próprios.
- **Tiro testado em jogo:** clarão, coice, fumaça translúcida (~6 s), bala acerta e fere (bug #48).

## Modos de teste novos
`-EAWAutoAndar` (anda e gira sozinho), `-EAWDueloDist=500` (bots começam longe), `-EAWArmaBot=Mosquete|Faca|Machado|Misto`,
`-EAWArma=Sabre|Faca|Machado` (jogador), `-EAWBaioneta` (começa no combate do mosquete), `-EAWRecargaRapida` (3 s),
`-EAWCampo` (chão de 600 m), `-EAWDiagBala` (diagnóstico do rastreio da bala), `-EAWAutoTiro`, `-EAWBotDist=N`.

## Pistola An IX (2026-11-25)
- Modelo próprio: pistola de cavalaria francesa An IX (1801) — 35 cm, cano de 20 cm, cal. 17 mm, 1,27 kg; ponteira e soleira de latão; vareta presa sob o cano (usa o osso `vareta` nas animações).
- Osso próprio **`pistola`** (filho da raiz, como o `arma` do mosquete) + `pistola_boca` (clarão/fumaça). Ver bug #52.
- Tiro: bala de 22 g / 16,5 mm a **220 m/s**, desvio 0,9° (cano curto), coice de 9°, mira mais leve (anda 75%, gira 170°/s). Teste: acerto na cabeça a 7,4 m (215 m/s, 508 J).
- Animações (3ª e 1ª pessoa): guarda, erguer, **mira de braço esticado com o corpo de lado** (câmera vai ao olho: 5,7 cm à frente, 1,9 à direita, 2,7 acima), tiro (cano sobe 18°), baixar, **recarga de 11 s em 5 etapas com a mão esquerda** (meio-armar, cartucho na boca, escorva, cano para cima, pólvora e bala, vareta para dentro, socar 2x, vareta de volta, armar, guarda).
- Ocupa a vaga 1 (arma de fogo) no lugar do mosquete; pode ser pega do chão (F, 3 s). `-EAWArma=Pistola` começa com ela.

## Multiplayer de teste, sem bot (2026-11-25) ✔ testado com 2 janelas
- **Sem bot automático** (`bSpawnBotOnStart = false`). A tecla **B** ainda cria um bot de treino se quiser.
- Cada jogador nasce **2,5 m ao lado** do anterior (direita, esquerda, direita…), porque o mapa de teste tem um só PlayerStart (`SpawnDefaultPawnAtTransform` no `EAWGameMode`).
- O chão de 600 m (`-EAWCampo`) agora é criado **em cada máquina** (servidor e cliente), no mesmo lugar (embaixo do PlayerStart), marcado com a tag `EAWCampo` para não duplicar.
- Atalhos na pasta `EmpiresAtWar`:
  - `Multiplayer_Servidor.bat` — abre o jogo como servidor (você joga junto). Porta **7777 UDP**.
  - `Multiplayer_Cliente.bat [IP]` — entra no servidor (sem IP = o próprio PC, 127.0.0.1). Em outro PC: `Multiplayer_Cliente.bat 192.168.x.x` e liberar a porta 7777 UDP no firewall do servidor.
  - `Multiplayer_2Janelas.bat` — servidor + cliente no mesmo PC.
- No editor: Play ▸ ⋮ ▸ *Number of Players* = 2, *Net Mode* = **Play As Listen Server**.
- Teste: o cliente entrou ("Join succeeded"), carregou os dois soldados e recebeu mosquete + sabre.

## Morte por tiro no pescoço + sangue e fumaça novos (2026-11-25) ✔ testado (side cam, 1ª pessoa e multiplayer)
- **Morte por tiro no pescoço:** animação "Dying" da Mixamo (trazida pelo usuário em `Modelos\ex\Dying.fbx`, fora do Git) transferida no Blender para o nosso esqueleto → `AS_Morte_Pescoco` (174 quadros, 5,8 s: cambaleia, cai de joelhos e de bruços). O esqueleto Mixamo tem a mesma pose de referência do soldado, então a transferência é direta osso a osso.
  - Bala no pescoço (dano mortal) → em vez do ragdoll toca a animação e a **artéria esguicha**: 2 jatos (entrada e saída) presos ao `neck_01`, pulsando no ritmo do coração (~95 → 60 por minuto), enfraquecendo em 10 s e 7 s.
  - Multiplayer: o servidor marca `bNeckDeath` (replicado) antes do dano; chega no mesmo pacote do "fora de combate", então todas as máquinas tocam a mesma morte. Testado com 2 janelas.
- **Sangue novo (sprites com textura, sem Niagara):** filetes esticados na direção do voo (contas e fiapos, como nas imagens de referência do usuário), gotas, névoa vermelha no impacto, jato arterial e **manchas de respingo** no chão (4 formatos, até 120, somem em 60 s). Saída da bala: leque radial de filetes.
- **Fumaça nova (referência: vídeo do Holdfast 2:09 e 3:39):** flocos com sombra própria que olham para a câmera; nuvem densa ~1,5–3 m à frente do cano, rolos em volta da boca, sopro da caçoleta; fica 5–7 s e deriva com o vento (35 cm/s).
- Texturas geradas por ruído (do zero): `EmpiresAtWar\Texturas\T_Fumaca.png`, `T_Sangue_Jato.png`, `T_Sangue_Mancha.png` (atlas 2×2; R = transparência, G/B = sombreado).
- Modos de teste: `-EAWBot` (cria o bot de treino), `-EAWMirarPescoco` (tiro automático no pescoço do mais próximo), `-EAWTestePescoco` (toda bala conta como pescoço), `-EAWCamBot=N` (câmera fixa ao lado do bot).

## Golpe especial carregado (2026-11-26) ✔ testado (câmera lateral, 1ª pessoa; sabre, faca e baioneta)
- **Como usar:** andando para a frente (W), **segure o botão esquerdo**: o soldado puxa o braço para trás e aparece a barra "Golpe especial...". Com **3 s** de carga e **W apertado**, o golpe sai sozinho. Soltar o botão antes (ou sem W) = corte normal. Sem W, o botão esquerdo continua sendo o corte de sempre.
- **Animações novas (Blender, feitas do zero):** `AS_Sabre_Especial` (+`_FP`) — braço esquerdo aponta o alvo, mão direita atrás do quadril, depois passo de 30 cm e estocada com o corpo (alcance 1,83 m); serve para sabre, faca e machado. `AS_Mosquete_Especial` (+`_FP`) — mosquete puxado para trás do quadril, depois investida funda com as duas mãos e a arma na horizontal (Fig. 9). As versões `_FP` têm o corpo quase parado (senão o ombro tampa a câmera).
- **Investida:** mira o inimigo à frente (até 4 m, 40°), vira o corpo para ele e para na distância de acerto da arma (faca 0,85 m, machado 1,3 m, sabre 1,4 m, baioneta 1,75 m). Alcance prático: faca ~2 m, sabre ~3 m, baioneta ~3,2 m.
- **Dano:** 150 (baioneta 160) = **fatal no tronco**; atravessa o braço que guarda o peito. Custa 22–28 de stamina. Se for **aparado**, o atacante fica atordoado 1,8× mais tempo. Quem leva o golpe fica atordoado 0,9 s.
- **Efeito:** respingo de volta na entrada + leque de filetes na saída + **jorro contínuo pelas costas** (1,6 s, perdendo força), trava de impacto e tremor de câmera.
- Multiplayer: a carga é um estado replicado (os outros veem o braço puxado = aviso para aparar); o servidor só aceita o golpe com a carga completa.
- Modo de teste: `-EAWAutoEspecial` (carrega e solta sozinho, em ciclo). Ex.: `-EAWBot -EAWBotPassivo -EAWAutoEspecial -EAWArma=Sabre -EAWBotDist=280 -EAWCampo -EAWCamBot=330`.

## Morte pelo golpe especial + sangue no chão até renascer (2026-11-26) ✔ testado (câmera do alto e multiplayer)
- **Animação:** `AS_Morte_Especial` — Mixamo trazida pelo usuário (`Modelos\ex\dycomboFacaEspadaBaioneta.fbx`, fora do Git), transferida no Blender para o nosso esqueleto (mesma pose de referência): cambaleia, cai de joelhos e tomba de costas (3,3 s).
- Quando o **golpe especial** (faca, espada, baioneta, machado) mata, a vítima toca essa animação em vez do ragdoll. O servidor marca a morte antes do dano e desmarca se a vítima sobreviver; os clientes recebem no mesmo pacote (`DeathAnimKind`: 1 = tiro no pescoço, 2 = golpe especial).
- **Mais sangue no chão:** a ferida do peito sangra pela frente e pelas costas no ritmo do coração (9 s e 7 s) enquanto ele cai; mais gotas viram mancha (45% dos filetes, 20% das gotas); **poça** que nasce quando o corpo termina de cair e cresce até ~1,7 m (tronco) + ~1 m (quadril) em 16 s. O tiro no pescoço também ganhou poça (~1,4 m).
- **O sangue fica no chão até o soldado renascer** (manchas sem tempo de vida; as do morto somem quando ele renasce/sai do jogo; as sem dono ficam até passar de 300 no mundo).
- Mancha no chão agora em modo **DBuffer cor + rugosidade** (`r.DBuffer=True`): antes ficava lilás (o decal regravava a normal do chão).
- Textura `T_Sangue_Mancha`: o 4º quadro virou uma **poça** (borda lisa, lóbulos escorridos, centro escuro).
- Teste: `-EAWCamAlto=N` sobe a câmera lateral e olha para o chão. Os modos `-EAWAuto*` agora valem só para o 1º jogador (no teste em rede o cliente "roubava" o modo automático).

## Baioneta como no vídeo x1, agachar e fumaça nova (2026-11-26) ✔ testado (duelo de bots, câmera lateral, 1ª pessoa, rede)
Referência: `Modelos\ex\x1.mp4` (vídeo do Holdfast em 3ª pessoa, fora do Git) — só estudo; animações feitas do zero no Blender.
- **Guarda da baioneta nova** (`AS_Mosquete_GuardaBaioneta` + `_FP`): mosquete **na diagonal do peito** (coronha no quadril direito, baioneta subindo para o ombro esquerdo), como no vídeo. Todos os golpes do mosquete começam e terminam nela; **o golpe especial não mudou**.
- **Ataques:** **botão esquerdo = estocada da cintura** (`AS_Mosquete_Estocada` refeita: baixa o mosquete na horizontal ao quadril e empurra com os dois braços; dano 55), **E = estocada longa** (62), **C = coronhada** (38, atordoa). Antes o botão esquerdo era a coronhada.
- **Defesas (botão direito), como no vídeo:** **alta** = mosquete na horizontal sobre a cabeça, pés afastados (`AS_Mosquete_AparoAlto`); **esquerda** = mosquete em pé do lado esquerdo, braço esquerdo alto (`AparoEsq`); **direita** = mosquete em pé do lado direito (`AparoDir`). Escolha automática pelo golpe do inimigo mais perto: golpe de cima → alta; corte da esquerda → esquerda; corte da direita/estocadas/especial → direita (desvia a lâmina); sem golpe vindo → alta.
- **Agachar (C ou Ctrl), só com arma de fogo** (mosquete no modo tiro ou pistola) — decisão do usuário; arma branca agachada fica para depois (estudar o Chivalry). Correr, pular (espaço) ou trocar para arma branca/baioneta levanta.
  - Cápsula 30 cm mais baixa (`CrouchedHalfHeight` 58) → câmera desce 30 cm; quadril da animação desce os mesmos 30 cm.
  - Animações `AS_Agachado_Parado` e `AS_Agachado_Frente/Tras/Esq/Dir` (pernas; 1,0 / 0,8 / 0,7 m/s, ciclo 0,9 s). Velocidade agachado 1,1 m/s (mirando/recarregando cai junto).
  - O tronco continua tocando a arma: **mira, tiro e recarga agachados** usam as mesmas animações de pé (o animador troca só pernas e altura do quadril; a rotação do quadril continua a da arma, então a mira não muda).
  - O mosquete e a pistola (ossos filhos da raiz) agora **acompanham o quadril final** quando ele difere do da animação da arma (agachado ou andando) — as mãos não saem mais da arma.
  - Rede: agachar replica pelo movimento do personagem da Unreal (testado: cliente agachou, servidor recebeu).
- **Fumaça (vídeo x1):** 30 jatos + 10 rolos + **9 flocos grandes de "corpo" da nuvem** (nascem 0,6–3 m à frente e incham até 1,3–1,8 m de raio); cresce sempre (difusão), fica **densa ~1/3 da vida e some devagar**: dura **9–15 s** (antes 5–7 s); sobe 14 cm/s com o vento. Textura `T_Fumaca` mais lisa (menos chuvisco) e borda suave no chão de 2,2 m no `M_Fumaca_Sprite` (bug #62).
- Testes novos: `-EAWAgachado` (seu soldado fica agachado; com `-EAWAutoAndar` anda agachado, com `-EAWAutoTiro` atira e recarrega agachado), `-EAWCamJogador` (com `-EAWCamBot=N`: a câmera lateral olha para o seu soldado).

## Combate — fase 1: movimento suave (2026-11-26) ✔ medido com o roteiro de teste
Referência: vídeos `Modelos\ex\baseMove/ataqueOnehand/thohamds.mp4` (pacote pago de animações — só estudo; tudo feito do zero). Ver [[Referência - movimento e combate (primeira fase)]].
- **8 direções:** trote novo nas diagonais (`AS_Trote_FrenteDir/FrenteEsq/TrasDir/TrasEsq`, 19 quadros, 2,55 e 2,08 m/s). O animador mistura só as **duas direções vizinhas pelo ângulo** (antes somava frente + lado e o pé escorregava ~1,5 m/s na diagonal).
- **Velocidade na diagonal certa:** as teclas viram uma direção só com a "elipse" de velocidades (frente 3,0 · lado 2,25 · trás 1,95 m/s; diagonal frente 2,55, diagonal trás 2,08). Antes a diagonal para trás andava ~3,0 m/s (mais que para trás).
- **Molas críticas** (começam e terminam com velocidade zero) no peso do andar, na troca de direção e na torção do tronco (o tronco segue a mira ~0,13 s atrás, 6° a 100°/s). Antes era "aproxima exponencial", que começa no máximo → tranco ao partir/parar.
- **Partir:** o passo começa com o pé direito à frente, como na guarda, já na direção certa.
- **Parar:** freada ~0,15 s (antes 0,07 s, "de estalo").
- **Virar:** o giro do corpo acelera e freia (1.800°/s²) em vez de começar já a 320–540°/s.
- **Medição (roteiro de 14 s, 60 qps, aceleração da ponta da arma, p95):** média 2.854 → 1.922; inverter para trás 8.345 → 1.553; mão 1.711 → 1.303; cabeça 1.231 → 974. Escorregão do pé na diagonal: 166/137 → ~59/57 cm/s.
- Testes: `-EAWRoteiroMov` (roteiro fixo: partir, parar, diagonais, inverter, virar no lugar, andar virando) + `-EAWDiagMov` (grava `Saved\Logs\diagmov.csv`). Análise: scripts `mov_test.sh` / `mov_analyze.py` (pasta temporária da sessão; a receita está aqui).

## Combate — fase 2: ataques novos (2026-11-26) ✔ testado contra bot (câmera lateral e 1ª pessoa)
Sem escudo (decisão do usuário). **O golpe especial não mudou.** Animações feitas do zero (`Soldado.blend` → texto `eaw_ataques`).
- **Golpe correndo — Shift + botão esquerdo** (já correndo, > 3,8 m/s): sabre armado atrás do ombro → corte diagonal de cima, corpo inclinado; **não freia a corrida** (as pernas continuam correndo). Dano 55, stamina 16. `AS_Sabre_Correndo` (+`_FP`). Mosquete: estocada da cintura. Correndo, o clique vira este golpe (o especial continua com W sem Shift).
- **Golpe de salto — botão esquerdo no ar:** sabre bem alto com o corpo arqueado → desce com tudo e aterrissa agachado (as pernas seguem o golpe). Dano 60. `AS_Sabre_Salto` (+`_FP`). Mosquete: estocada longa.
- **Chute — V** (sabre, faca, machado e baioneta): joelho sobe → pé estica na altura da barriga. **Não fere**; atravessa a defesa (quem defendia perde a guarda, 0,9 s), tira 20 de stamina, atordoa 0,6 s e **empurra ~1 m**. Quem está no meio do próprio golpe só é empurrado. Acerta com a perna (do joelho à ponta do pé), alcance ~1,5 m (testado: acerta a 1,5 m, não a 1,65 m). `AS_Chute` e `AS_Mosquete_Chute` (+`_FP`, mosquete na guarda da baioneta).
- Animador: trechos de "corpo inteiro" (chute e golpe de salto) — as pernas seguem o golpe mesmo andando, com troca suave (mola).
- Versões `_FP`: mão esquerda baixa e quadril quase parado (na 3ª pessoa a mão esquerda vai à frente para equilibrar; na 1ª pessoa ela tapava a visão).
- Teste: `-EAWAutoGolpe=Chute|Salto|Correndo` (o seu soldado repete o golpe contra o bot).

## Combate — fase 3: defesa segurada e trocas suaves (2026-11-26) ✔ testado contra bot atacando (sabre e baioneta)
- **Defesa segurada (botão direito apertado):** o clique continua sendo o parry na hora certa (janela de 0,3 s, contra-ataque rápido se defender). **Segurando** depois da janela, o soldado **fica em defesa** (estado novo `Block`) até soltar, **andando devagar (60%) em qualquer direção**, sem correr — como o "Block Walk/Run" do vídeo de referência de duas mãos.
  - Bloqueia golpes de frente (mesmo ângulo do parry), **sem contra-ataque**, custando **20 de stamina** por golpe (parry custa 12). Sem stamina, a guarda quebra (atordoado 0,9 s).
  - O atacante bloqueado volta à guarda (sem ficar atordoado, como no parry certo).
  - O **chute quebra a defesa segurada**. Golpe de costas/de lado passa.
  - Atacar a partir da defesa segurada sai direto para o golpe.
  - Soltar o botão: abaixa a guarda em 0,2 s.
  - Baioneta: a pose da defesa **acompanha o golpe que vem** (alta para golpe de cima/salto; em pé à esquerda/direita para cortes, estocadas, especial e golpe correndo).
- **Trocas entre golpes:** mistura de 0,12 s com curva em S e o trecho anterior continua tocando durante a mistura (antes congelava). **Medição:** já não era o problema — nas 43 trocas do teste a ponta da arma acelera menos (p95 ~18.000) que no meio dos golpes (p95 ~42.000); depois da mudança ficou igual dentro da variação (p95 ~19.000). Os maiores "trancos" são o próprio impacto do golpe (lâmina mais rápida de propósito).
- Testes: `-EAWAutoGolpe=Defesa` (segura a defesa e anda para frente/trás na frente do bot); registro por quadro agora grava o estado do combate (`-EAWDiagMov`).

## Combate — fase 4: combos por cliques (2026-11-26) ✔ testado contra bot
**O golpe especial não mudou** (conferido: sabre 150, baioneta 160 a 2,8–3 m, igual a antes da fase 1).
| Sequência | O que acontece |
|---|---|
| 1 clique | corte (alterna esquerda/direita a cada golpe, como antes) |
| **2 cliques seguidos** | 2º corte do outro lado com preparação rápida ("Combo 2" na tela) |
| **3 cliques seguidos** | o 3º vira **golpe final de cima**: dano ×1,35, preparação ×1,1 ("GOLPE FINAL!") — mosquete: estocada longa |
| **2 cliques + E** | fecha com **estocada final** (dano ×1,35 e passo 40% mais longo — alcança quem recua) — mosquete: estocada longa |
- O clique "pega" a partir de 35% do golpe (antes 50%): encadear ficou mais fácil. Clicar também na volta à guarda continua o combo.
- Depois do golpe final o combo recomeça. Defender, fintar, ser atordoado ou voltar à guarda zera o combo. Chute, golpe correndo/de salto e especial não contam como passo.
- Andando para a frente (W), um clique na volta à guarda começa a carga do especial (como sempre); **soltando rápido, o combo continua** (antes recomeçava).
- Teste: `-EAWAutoGolpe=Combo3` / `Combo2E`.

### Regressão encontrada e corrigida (bug #70)
O golpe especial **parou de acertar** depois da fase 1 (a investida parava ~20 cm antes). Achado comparando versões compiladas lado a lado (antes/depois da fase 1, desfazendo uma mudança por vez): a **mola do peso do andar** mudava a mistura das pernas no instante da investida, e o alcance de todos os golpes (inclusive o especial) foi medido com a mistura antiga. Agora: mola só andando/parando em guarda; nos golpes, a mistura antiga. Também a freada nos golpes voltou a ser exatamente a antiga (a macia fica para andar/parar).

## Bots novos: chegam andando, atiram e lutam com os golpes novos (2026-11-26) ✔ testado
- **Nascem longe:** com o campo de teste (`-EAWCampo`), o bot nasce a **35 m** de frente para você (tecla **B** ou `-EAWBot`; vários bots ficam espalhados de lado, 4 m entre eles). Sem o campo, 3 m (o chão acaba a ~5 m). `-EAWBotDist=N` escolhe.
- **Fases do bot** (log `bot: ...`):
  1. **Aproximando** — anda (sem correr) até ~25 m: dá para ver a caminhada.
  2. **Atirando** — troca para o mosquete (modo tiro), mira 0,9–1,8 s e atira. **Desvio:** o do cano liso (0,35°) + erro de mira de uma pessoa (0,25°) → a 25 m, ~metade dos tiros acerta o corpo. Fumaça, poeira onde a bala cai, ferimento se acertar.
  3. **Recarregando** — se você continuar a mais de ~14 m, recarrega (15 s; `-EAWRecargaRapida` = 3 s) e atira de novo (até 2 tiros).
  4. **Carregando** — troca para a arma branca (bot de baioneta: põe a baioneta) e vem **correndo**; metade das vezes faz o **golpe correndo** a ~3,3 m.
  5. **Lutando** — anda de lado em volta de você; defende (55%: parry, e 35% dessas viram **defesa segurada** por 0,8–1,8 s); ataca com golpe simples, de cima, estocada, **combos** (2 cliques, 3 cliques = golpe final, 2 cliques + E), **chute** (sempre que você segura a defesa, 60%) e **golpe de salto** (12%, quando está a ~2–2,7 m). Se você fugir, corre atrás.
- Chegando a menos de 9 m em qualquer fase de tiro, o bot larga o mosquete e vai para a luta.
- Testado: a 35 m → andou, atirou a 24,8 m (errou por ~45 cm, a bala caiu 15 m atrás), recarregou, atirou de novo, veio correndo e lutou; golpe correndo acertou (55), golpe de salto (120 na cabeça), combo de 2, chutes em quem defende. Golpe especial do jogador continua 150. Duelo de bots (`-EAWDuelo`) continua direto na luta.

## Correções pedidas pelo usuário: mosquete na perna e ar de robô (2026-11-26) ✔ testado (câmera seguindo de lado)
- **Mosquete atravessando a coxa:** medido no Blender (eixo da coronha × eixo das pernas em todos os quadros do trote, 8 direções): só **8,9 cm** no modo de tiro e **8,4 cm** na guarda da baioneta (a coxa tem ~9 cm de raio → atravessava). Procurada a menor mudança de pose com folga ≥ 15 cm e as mãos ainda na arma:
  - modo de tiro: punho 8 cm para fora e 5 cm acima, boca um pouco para dentro → **19,4 cm**;
  - guarda da baioneta: punho 4 cm para fora e 10 cm acima → **17 cm**.
  Refeitas (só 3ª pessoa; 1ª pessoa não mostra a perna): guarda, erguer, mira, tiro, baixar, recarga do mosquete e todos os golpes/defesas da baioneta. **O golpe especial não foi refeito** (conferido: sabre 150, baioneta 160).
- **Baioneta ao apertar X:** tocava a pose do tiro até o 1º golpe (a guarda diagonal só aparecia depois) → agora a guarda diagonal entra na hora.
- **Ar de robô ao andar:** o tronco ficava parado em cima das pernas. Agora, a cada passo, **os ombros giram contra o quadril (5°), o tronco pende para o pé que apoia (2°) e inclina a cada passo (1,3°), e a cabeça compensa** (fica estável). Cheio andando em guarda; 35% atacando/defendendo; 60% recarregando; 15% mirando; menor agachado e no golpe de corpo inteiro.
- O mosquete e a pistola agora **seguem o peito** (antes o quadril): andar, agachar, torcer e balançar não tiram a arma das mãos.
- Teste novo: `-EAWCamSegue=N` (câmera de lado que acompanha o seu soldado a N cm).
- Ainda falta: o usuário dizer quais golpes parecem robóticos no combate (o balanço ajuda ao andar; nos golpes em si ainda são poses-chave feitas à mão).

## Fardas: soldado francês e soldado britânico (2026-11-26) ✔ testado em jogo (duelo, câmera que segue, 1ª pessoa, de longe)
Referências que o usuário deixou em `Modelos\ex\Textures` e `Modelos\ex\Meshes`: **extraídas do Holdfast** (Unity: "AWI", "Frontlines", `hui-banner-nation-french`, LODs `Uniform_LOD0..4` de ~15 mil faces). Só estudo — **nada foi copiado**: malhas e texturas feitas do zero por script.
- **França — fuzileiro de linha 1812 (habit-veste Bardin):** casaca azul com lapelas brancas de vivo vermelho e 7 botões por lado, gola vermelha com vivo azul, punhos vermelhos com pala branca e 3 botões, abas curtas com forro branco e losango vermelho, ombreiras azuis com vivo vermelho; calça branca; **polainas pretas até o joelho** com fileira de botões; **barretina 1810** (feltro preto, faixas de couro, placa de latão em losango com águia e o número 45, cocar tricolor, pompom verde, rosetas e escamas); correame branco cruzado; cartucheira preta com águia de latão (quadril direito, atrás); bainha da baioneta (quadril esquerdo); **mochila de couro de vaca malhado** com capote enrolado.
- **Grã-Bretanha — infantaria de linha 1812:** casaca ("coatee") vermelha com **10 laços de renda branca por lado** (ponta de baluarte, listra azul, botão na ponta), gola e punhos azuis com renda, abas curtas com forro branco; calça cinza com polaina curta; **barretina "Belgic"** (frente falsa alta, placa com coroa e GR, cordões brancos em festão, pluma branca sobre vermelho do lado esquerdo, roseta preta); correame branco cruzado; cartucheira; bainha; **mochila preta (Trotter)** com o número 30, cobertor cinza e marmita azul; sacola de lona e cantil azul no lado esquerdo.
- **Como é feito (`Modelos\fardas\gerar_fardas.py`, roda com `blender -b`):** cada peça de roupa é uma malha em **anéis em volta dos ossos** (tronco, mangas, pernas, gola, punhos, polainas, abas, lapelas), com o raio medido no corpo do Y Bot por raios (BVH) + folga e suavização (fecha as frestas entre as peças do manequim). Correias dão a volta no tronco num plano inclinado; equipamento (barretinas, mochilas, cartucheira, bainha, sapatos) são peças rígidas. Cabeça e mãos vêm do manequim (pele pintada).
- **Pesos:** copiados do corpo (ponto mais próximo, interpolado) e suavizados; peças rígidas presas a um osso; **abas** = quadril → 40% da coxa na ponta; **bainha** = quadril → 75% da coxa esquerda na ponta; **cartucheira** 80% quadril + 20% coxa direita; **sacola/cantil** 70% quadril + 30% coxa esquerda; **sapatos** rígidos (pé / ponta do pé, cano mistura a canela). Máx. 4 ossos por vértice.
- **Triângulos:** LOD0 23,2 mil (FR) / 24,0 mil (GB); LOD1 ~10,5 mil; LOD2 ~4,7 mil; LOD3 ~1,9 mil (entram a 42%, 20% e 8% da tela). Antes o manequim tinha 55 mil.
- **Texturas (2048², `Modelos\fardas\gerar_texturas_fardas.py`, Python 3.11 com numpy/Pillow):** um atlas por nação — cor, normal (padrão DirectX) e ORM (oclusão, rugosidade, metal). Cada texel sabe a sua posição 3D, normal, material e peça; tudo pintado por regras: sarja da lã, grão do couro, feltro, latão, couro de vaca, lona, madeira, botões com relevo, vivos, renda, placas, números, costuras, sujeira nos pés/joelhos, suor nas axilas, desbote nos ombros e oclusão por vértice (raios). Uso do atlas ~49% (FR) / ~40% (GB); ~2 mm por texel.
- **Unreal:** `/Game/Personagem/Fardas/SK_Soldado_FR` e `SK_Soldado_GB` (esqueleto do soldado, física `SK_Soldado_PhysicsAsset`, LOD1–3 importados dos FBX próprios), material `M_Farda` + instâncias `MI_Farda_FR/GB`, peças do corte por nação em `/Game/Personagem/Fardas/Pecas`. Script: `Scripts\import_fardas.py` (já está no `reimportar_tudo.py`, depois das peças e antes da física).
- **Nação no jogo:** propriedade replicada `Nacao` no `AEAWCharacter` (0 = França, 1 = Grã-Bretanha) troca a malha do corpo, dos braços de 1ª pessoa e as peças do desmembramento. O GameMode cria os soldados com **spawn adiado** (a farda entra antes do `BeginPlay`): jogadores alternam pela ordem de entrada (1º França, 2º Grã-Bretanha...), **bots do lado oposto ao 1º jogador**, duelo de bots = um de cada. Teste: `-EAWNacao=FR|GB` (1º jogador) e `-EAWNacaoBot=FR|GB`.
- **Conferido:** golpe especial igual (sabre **150**, baioneta **160**). Duelo FR × GB, câmera seguindo o jogador, 1ª pessoa (mangas e punhos da nação), de longe (LODs).
- **Colisão medida em TODAS as 126 animações + mosquete na guarda/mira com as 13 animações de andar/agachar** (`Modelos\fardas\teste_colisao.py`, cápsulas nos ossos das pernas com o raio real da roupa):
  - **Mosquete:** em nenhum caso a roupa faz o mosquete atravessar onde o corpo nu já não atravessasse. Os casos que sobram já vinham das animações (contra o corpo nu): aparo à direita/esquerda (5–7 cm), chute do mosquete (8,5 cm), recarga (3,7 cm).
  - **Roupa com roupa:** andando/trote ≤ 1,1 cm (FR) e 0 (GB); correndo 4,4 cm (abas FR) / 2,8 cm (sacola GB); chute 4–6 cm (abas FR); mortes de joelhos: ponta da bainha na canela (5–8 cm, corpo caído).
