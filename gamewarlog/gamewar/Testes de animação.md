---
tags: [testes, animacao]
atualizado: 2026-11-24
---
# Testes de animação

Como testamos cada animação (feito no Blender por script, função `measure` da biblioteca `eaw_lib`):
- **Alcance e altura da ponta** da lâmina em cada quadro; **pico de velocidade** deve cair no quadro do impacto.
- **Torção do pulso** (mão em relação ao antebraço): ✔ até ~60°.
- **Dobra do pulso**: ✔ até ~80°, ⚠ 80–100°, ✖ acima de 100°.
- **IK**: mãos e pés chegam no alvo (erro 0 cm = braço/perna não esticou além do limite).
- **Emenda**: primeiro e último quadro iguais à guarda.
- **Folha visual**: render das poses-chave (prep, impacto, fim).

## Resultado — conjunto de teste do sabre (2026-11-24)
| Animação | Quadros | Alcance máx. | Pico de velocidade | Torção máx. | Dobra máx. | Emenda | Resultado |
|---|---|---|---|---|---|---|---|
| `AS_Sabre_Guarda` | 1–60 loop | — | — | 6° | 25° | ✔ | ✔ |
| `AS_Sabre_Corte_Esq` | 1–41 | 1,47 m (q20) | 29 m/s no impacto (q20) | 8° | 82° | ✔ | ✔ |
| `AS_Sabre_Corte_Dir` | 1–41 | 1,48 m (q20) | 23 m/s no impacto (q20) | 29° | 85° | ✔ | ✔ (⚠ dobra) |
| `AS_Sabre_DeCima` | 1–45 | 1,41 m (q23) | 28 m/s no impacto (q22, ponta a 1,83 m) | 17° | 98° | ✔ | ⚠ dobra |
| `AS_Sabre_Estocada` | 1–35 | 1,53 m (q20) | 7 m/s (q17) | 12° | 92° | ✔ | ⚠ dobra |
| `AS_Sabre_Parry` | 1–20 | lâmina cruza o centro a 1,68 m, 45 cm à frente | — | 6° | 75° | ✔ | ✔ |
| `AS_Sabre_Bloqueado` | 1–17 | (começa no impacto do corte) | 19 m/s no ricochete | 7° | 75° | ✔ fim | ✔ |
| `AS_Sabre_Finta` | 1–19 | — | — | 6° | 36° | ✔ | ✔ |

IK das mãos e pés: erro **0 cm** em todas. Pés não escorregam.

## Teste no Unreal — duelo bot contra bot (2026-11-24)
Comando: `UE4Editor.exe <uproject> -game -windowed -ResX=960 -ResY=540 -EAWDuelo -EAWShots=60 -EAWShotEvery=0.1 -EAWShotDelay=0.4`
| Item | Resultado |
|---|---|
| Altura do personagem / cabeça | 180,5 cm / 160 cm ✔ |
| Orientação (pé aponta para a frente) | 1,00 ✔ |
| Lâmina animada na escala certa | ✔ (depois dos bugs #20–#22) |
| Estocada acerta o tronco e interrompe o golpe do outro | ✔ |
| Corte acerta, 2º ferimento → incapacitado → ragdoll | ✔ |
| Parry bem-sucedido → atacante atordoado | ✔ |
| Hitboxes do caído desligadas | ✔ (bug #24) |
| Distância de luta 125–165 cm, sem lâmina atravessando o corpo | ✔ (bug #25) |

## Teste em 1ª pessoa (2026-11-24)
Comando: `UE4Editor.exe <uproject> -game -windowed -ResX=960 -ResY=540 -EAWShots=30 -EAWShotEvery=0.2 -EAWShotDelay=0.5`
| Item | Resultado |
|---|---|
| Câmera nos olhos, cabeça escondida (não tapa a visão) | ✔ |
| Ponta do nosso sabre aparece na guarda | ✔ |
| **Mãos e braços na tela** | ✖ — na guarda as mãos ficam ~55° abaixo dos olhos; a câmera só vê ~30° para baixo |
| Bot acertou nossa cabeça (golpe de cima, dano 100) → morte + ragdoll | ✔ (comportamento esperado) |
**Conclusão:** o corpo "de verdade" não serve como visão de 1ª pessoa. Precisa de **braços separados para a 1ª pessoa**
(como o documento prevê na seção 5). Ver [[Próximos passos]].

## Braços de 1ª pessoa (2026-11-24)
Comando: `... -game -windowed -ResX=960 -ResY=540 -EAWBotPassivo -EAWAutoAtaque -EAWShots=30 -EAWShotEvery=0.2 -EAWShotDelay=0.5`
| Versão | Resultado |
|---|---|
| 1ª tentativa (X +8, Y 0, Z +12, 12°) | Mão e sabre aparecem ✔; sabre no centro tapando o adversário ✖; **ombro/braço cobrindo a tela** nos golpes ✖ |
| 2ª tentativa (X −14, Y +10, Z +14, 14°) — **padrão** | Sabre à direita, adversário livre ✔; sem ombro na tela ✔; golpe atravessa a tela ✔ |
| Pendente | **Preparações saem da tela** (braço vai para trás) — pedem poses próprias de 1ª pessoa |

### Versões de 1ª pessoa (Blender, 2026-11-24)
Medição com a câmera de 1ª pessoa reproduzida no Blender (`to_cam`/`measure_fp` na `eaw_lib`).
| Animação | Fora da tela (antes → `_FP`) | IK | Torção / dobra do pulso |
|---|---|---|---|
| Corte esq. | quadros 5–18 e 23–28 → **nenhum** | 2 cm | 11° / 85° |
| Corte dir. | 6–17 e 22–31 (+ braço na câmera) → **nenhum** | 0 | 22° / 95° |
| De cima | 4–22 (inclusive impacto) → **nenhum** | 0 | ⚠ 79° / 69° |
| Parry | 4–12 (defesa inteira!) → **nenhum** | 0 | 14° / 76° |
| Finta | 4–15 → **nenhum** | 0 | 6° / 27° |
| Bloqueado | 3–12 → **nenhum** | 0 | 6° / 75° |
| Guarda, estocada | já visíveis (usam a mesma animação) | — | — |
Poses na tela escolhidas por busca automática (alcance do IK + visível + longe da lente); posições em `eaw_fp`.
### Versões de 1ª pessoa no Unreal (capturas, 2026-11-24) ✔
| Golpe | O que se vê na tela |
|---|---|
| Corte esq. | Preparação no canto superior direito, varre para a esquerda ✔ |
| Corte dir. | Preparação no canto superior esquerdo (revés), varre para a direita ✔ |
| De cima | Mão sobe no topo, desce no centro no impacto ✔ |
| Estocada | Lâmina avança da direita para o centro ✔ |
| Parry | Lâmina na diagonal na frente do rosto ✔ |
| ⚠ Menor | Antebraço passa grande perto da lente ~0,2 s (fim do corte esq., preparação do corte dir.) |

## Movimento (Blender, 2026-11-24)
| Animação | Escorregão do pé | Alcance da perna (IK) | Emenda do loop | Pulso (torção/dobra) |
|---|---|---|---|---|
| Andar frente | 0 | 0 cm | 0 cm | 7° / 24° ✔ |
| Andar trás | 0 | 0 cm | 0 cm | 4° / 24° ✔ |
| Andar esq. / dir. | 0 | 0 cm | 0 cm | 5° / 23° ✔ |
| Correr | 0 | ⚠ 2,4 cm (extremos da passada) | 0 cm | 12° / 75° ✔ |
Folha visual (de lado e de frente): passada alternando, cruzamento no meio, corrida inclinada com sabre no ombro ✔.

## Movimento + combate no Unreal (duelo, 2026-11-24)
| Item | Resultado |
|---|---|
| Pernas andando com o tronco em guarda | ✔ |
| Preparar golpe andando (pernas no movimento, tronco no combate) | ✔ |
| Combate continua acertando pelo animador novo | ✔ |
| Queda ao morrer | ✖ pernas de pé (bug #34) → ✖ queda lenta 2,4 s (bug #35) → ✔ ~0,5 s |
| Ainda não visto | Correr com sabre no ombro, andar de lado, transição de 0,1 s entre golpes (em detalhe) |

## Passe fino — movimento secundário por mola (2026-11-24)
Assado fiel (diferença máx. 0,08 mm). Molas: coluna 6/5 Hz, pescoço 4 Hz, cabeça 3,5 Hz, braço livre 3–4 Hz; braço do sabre fora.
| Golpe | Alcance antes → depois | Pico antes → depois | Mão livre | Cabeça | Coluna |
|---|---|---|---|---|---|
| Corte esq. | 1,47 → 1,48 m (q20) | 29,4 → 28,9 m/s (q20) | 7 cm | 0,7 cm | 0,2 cm |
| Corte dir. | 1,48 → 1,49 m (q20) | 22,8 → 22,3 m/s (q20) | 10 cm | 0,7 cm | 0,1 cm |
| De cima | 1,41 → 1,40 m (q23) | 28,5 → 28,0 m/s (q22) | 14 cm | 1,8 cm | 0,4 cm |
| Estocada | 1,53 → 1,53 m (q20) | 6,9 → 6,9 m/s (q17) | 18 cm | 1,5 cm | 0,3 cm |
Emendas com a guarda: 2 mm. Loops fecham (0 cm). No Unreal: 19 animações, 0 avisos, duelo acertando ✔.
**Avaliação honesta:** o ganho é real mas **sutil** (sobretudo o braço livre e a cabeça); a coluna quase não mudou,
porque mexer mais nela altera o braço do sabre. A comparação em imagem no Blender ficou pequena demais para mostrar.

## Peso e física (duelo, 2026-11-25)
| Item | Resultado |
|---|---|
| Tronco do atingido reage com física e volta à guarda | ✔ |
| Queda ao morrer | ✖ corpo voando (bug #37) → ✖ pernas balançando (bug #38) → ✔ cai em ~0,5 s e fica quieto |
| Não visto ainda | Tremor de câmera e giro limitado em 1ª pessoa (só aparecem jogando/capturas em 1ª pessoa) |

## Ainda não testado
- **Emendas sem mistura:** hoje cada animação começa seca (sem blend); combo e golpe bloqueado de outro golpe podem dar "pulo" de pose.
- **Mira vertical:** a lâmina segue a animação, não o olhar para cima/baixo.
- Deformação da malha do pulso de perto em de cima e estocada (dobra 92–98°).
- Visão em 1ª pessoa (só no Unreal).
- Emendas de combo (fim do golpe → preparação do próximo) — o código encurta a preparação no combo.

## Sangue e desmembramento — duelo de bots (2026-11-25)
Capturas a cada 0,2–0,3 s com `-EAWDuelo`; cortes forçados com `-EAWTesteCorte=`.

| Item | Resultado |
|---|---|
| Gotas a cada acerto + manchas no chão | ✔ (gotas pareciam pretas contra o céu → brilho próprio, bug #44) |
| Decapitação | ✖ cabeça parada no ar e corpo arremessado (bug #40) → ✖ cabeça ainda presa (bug #41, física com 4 corpos) → ✔ cabeça voa, corpo cai sem cabeça |
| Braço | ✔ braço esquerdo se solta e voa; corpo cai |
| Perna | ✔ canela se solta; tampa vermelha no joelho; poça embaixo do corpo |
| Duelo normal (sem forçar) | ✔ golpe real na cabeça (dano 100) decapitou; corpo ficou parado na poça |
| Corpo voando ao ser tocado | ✖ (bug #39) → ✔ |
| `-EAWSemSangue` | ✔ morte sem corte nem sangue |
| Não visto ainda | Tela vermelha (só aparece no seu soldado em 1ª pessoa); jato do coto de perto |

## Movimento, tiro e armas (2026-11-25)
| Item | Resultado |
|---|---|
| Trote 1ª pessoa (−EAWAutoAndar) | ✔ 2,85 m/s medido; arma balança no passo; inclina <1° de lado (✖ antes: imagem tombava, bug #49) |
| Bots vindo de 5–8 m | ✔ trotam e chegam em <1 s, de frente um para o outro |
| Bala a 10 m | ✖ passava (bug #48) → ✔ acerto no braço a 399 m/s (2.190 J) |
| Clarão, coice, fumaça | ✔ (fumaça precisava de VertexInterpolator, bug #45) |
| Duelo de baioneta | ✔ coronhada, aparos com mosquetes cruzados, estocada com sangue |
| Faca × machado | ✖ machado flutuando (bug #50) → ✔ na mão |
| Combate do mosquete em 1ª pessoa | ✔ estocadas à frente; coronhada ajustada só para a 1ª pessoa (preparação 25 cm à frente, golpe 18 cm abaixo) |
| Não visto ainda | Recarga completa de 15 s em jogo (só etapas iniciais), ferida de bala de perto |
| Golpe especial (sabre, faca, baioneta) | ✔ carga com braço para trás, investida até o alvo, jorro pelas costas, vítima cai; 1ª pessoa sem tapar a tela |
| Baioneta do vídeo x1 (duelo de bots) | ✔ guarda diagonal; estocada da cintura com sangue; defesa com o mosquete em pé desviando a estocada; mãos na arma. Defesa alta não apareceu no duelo (os bots só estocam) |
| Agachado (câmera lateral) | ✔ anda agachado com o mosquete pronto; mira, tiro e recarga agachado com as mãos na arma |
| Fumaça nova (1ª pessoa) | ✔ nuvem enche a tela no tiro, fica ~9 s e some devagar; sem as linhas no chão (bug #62) |
| Agachar em rede (2 janelas) | ✔ cliente agachou → servidor recebeu |
