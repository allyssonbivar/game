---
tags: [bugs]
atualizado: 2026-11-24
---
# Bugs resolvidos

| # | Data | Problema | Causa | Solução |
|---|---|---|---|---|
| 1 | 2026-11-24 | Build do REA recusado | Exige Node 24.22.1 (havia 26.13.1) | Node 24.22.1 portátil em `testar\.tools` (Node do sistema intacto) |
| 2 | 2026-11-24 | Build do REA sem addon nativo no Windows | Não é gerado a partir do código | Copiado do pacote oficial `rea-agents@6.10.1` (código nativo igual ao da release) |
| 3 | 2026-11-24 | Caminhos do MCP REA sem barras `\` | Escape do shell | Gravados via variável de ambiente |
| 4 | 2026-11-24 | MCP REA sem Ghidra | Faltavam `GHIDRA_INSTALL_DIR` e `JAVA_HOME` | Adicionados no bloco `env` do `~/.claude.json` |
| 5 | 2026-11-24 | Unreal não compilava | Windows SDK ausente | SDK 10.0.19041 + MSVC 14.29 instalados; versão fixada nos `*.Target.cs` |
| 6 | 2026-11-24 | Log do teste sem linhas | Buffer perdido ao encerrar o processo | Rodar com `-forcelogflush` |
| 7 | 2026-11-24 | Golpes do bot nunca acertavam | Alcance ~125 cm, bot lutava a ~160 cm; estocada 27 cm deslocada | Braço avança no golpe (~1,4 m), estocada centralizada, `EngageDistance` 125 |
| 8 | 2026-11-24 | Bot fintava o próprio contra-ataque | Decisão de finta no 1º quadro | Finta só após 30% da preparação e nunca no contra-ataque |
| 9 | 2026-11-24 | Download do Mixamo sem corpo | Animação selecionada | Aba **Characters**, remover a animação, baixar com **Pose: T-pose** |
| 10 | 2026-11-24 | Cotovelo não dobrava no IK | Braço reto em T-pose | Pré-dobra de 2 cm no cotovelo |
| 11 | 2026-11-24 | Sabre na vertical na guarda | Lâmina sai perpendicular aos dedos | Pulso inclinado: lâmina a 51° para cima |
| 12 | 2026-11-24 | Golpe sem peso | Pico de velocidade antes do impacto | Ease-in 15→20, ease-out 20→24 |
| 13 | 2026-11-24 | Pulso torcia 116° | Antebraço não girava | Antebraço assume 50% (Locked Track) → máx. 63° |
| 14 | 2026-11-24 | Mão aberta, sem segurar o sabre | Sem pose de dedos; cabo no meio da mão | `weapon_r` 2,5 cm para a palma; dedos fechados; polegar sobre o indicador |

| 15 | 2026-11-24 | Teste dizia que a ponta do sabre não se movia | `SM_Sabre` escondido (teste da faca); o Blender não atualiza objeto escondido | Sabre visível, faca de teste escondida |
| 16 | 2026-11-24 | Corte direito com pulso torcido 179° | Rotação da mão em volta do cabo escolhida à mão | Função `best_hand`: testa a mão girada de 30 em 30° e fica com a menor torção |
| 17 | 2026-11-24 | Preparação do corte direito com pulso dobrado 161° | Direção da lâmina ruim para o revés | Busca automática: lâmina 60° para cima sobre o ombro esquerdo (dobra 113°) |
| 18 | 2026-11-24 | Pulso dobrando até 142° (de cima, estocada) | **Cabo perpendicular à mão** (erro de modelagem); no punho real o cabo cruza a palma na diagonal | `weapon_r/l` girados **30°** na palma; dedos anelar/mínimo mais fechados; dobra máx. caiu para 75–98° |
| 19 | 2026-11-24 | Parry com pulso dobrado 114° | Lâmina deitada para a esquerda | Busca: lâmina 55° para cima, cruzando a frente do rosto a 1,68 m; dobra 75° |

| 20 | 2026-11-24 | Personagem chegou no Unreal com 1,8 cm | Esqueleto do Mixamo com **escala 0,01** no Blender + unidade do FBX | Escala aplicada no Blender (esqueleto = 1); posições das animações ×0,01 para continuarem iguais (medições conferidas) |
| 21 | 2026-11-24 | Lâmina a 100–200 m do soldado durante a animação | Escala ×100 aplicada à malha também multiplicou as animações | Ver receita abaixo |
| 22 | 2026-11-24 | Lâmina encolhida nos pés (animação 100× menor) | O Blender põe a conversão m→cm no nó "Armature", e o Unreal descarta esse nó | Exportar com **FBX_SCALE_ALL** (conversão em cada osso) + importar ×100 |
| 23 | 2026-11-24 | Animação extra "mixamo.com" no Unreal | Ação antiga do 1º download esquecida no .blend | Ações que não começam com `AS_` apagadas; script do Unreal apaga sobras "mixamo" |
| 24 | 2026-11-24 | Hitboxes do morto ficavam de pé no ar | Ragdoll não desligava as cápsulas | Ao cair: hitboxes sem colisão e escondidas; o reset religa |
| 25 | 2026-11-24 | Sabres em guarda atravessando o adversário | Bots lutavam a ~90 cm | Bot se aproxima até 150 cm e recua abaixo de 120 cm |
| 26 | 2026-11-24 | Medição antes/depois idêntica em tudo | `SM_Sabre` escondido de novo (exportação) | Medir pela ponta do osso `weapon_r_tip`, sempre atualizado |

| 27 | 2026-11-24 | Plugin ACL não carregava ("not compatible with 4.25.0") | `.uplugin` declarado para UE 4.25 | `EngineVersion` = 4.27.0 na cópia do projeto (compila no 4.27); reabrir o editor |

| 28 | 2026-11-24 | Em 1ª pessoa as mãos ficavam fora da tela | Câmera nos olhos do corpo inteiro: mãos ~55° abaixo da visão | Braços de 1ª pessoa presos à câmera, inclinados e deslocados |
| 29 | 2026-11-24 | Ombro/braço cobrindo a tela nos golpes (1ª pessoa) | Ombro passava na frente da câmera ao girar o tronco | Braços de 1ª pessoa recuados 14 cm e 10 cm para a direita |
| 30 | 2026-11-24 | Ajustes do dono não aconteciam no BeginPlay | O jogador possui o soldado **depois** do BeginPlay | Configuração da 1ª pessoa em `PawnClientRestart` |

| 31 | 2026-11-24 | Importação por script não criou as animações novas (`_FP`) | **Editor do Unreal aberto** com o projeto segura os assets | Fechar o editor antes de rodar `import_soldado.py` (o personagem até atualiza, as animações não) |

| 32 | 2026-11-24 | `GetAnimationPose` não compilava no animador novo | API mudou no UE 4.26+ | Usar `FAnimationPoseData(PoseContext)` |
| 33 | 2026-11-24 | Variável `Layers` dava erro | Todo ator do Unreal já tem um campo `Layers` | Renomeada para `AnimLayers` |

| 34 | 2026-11-24 | Ragdoll com as pernas de pé (só o tronco tombava) | A cápsula do próprio soldado segurava quadril e pernas | Cápsula **sem colisão** ao cair; religada no reset |
| 35 | 2026-11-24 | Corpo "murchava" devagar (2,4 s até o chão) | Ragdoll começava sem nenhuma força | Empurrão no tronco para trás ao cair → ~0,5 s (referência Holdfast: 0,2–0,4 s) |

| 36 | 2026-11-24 | "Assar" do Blender não fazia nada ("Nothing to bake") | No Blender 5.3 a seleção de ossos mudou de lugar | Assar por script próprio (`convert_space` por quadro) |

| 37 | 2026-11-25 | Morto saía **voando** e girando no ar por ~2 s | Trava de tempo + física parcial + ragdoll no mesmo instante → velocidades enormes | Quem cai não recebe trava/empurrão; antes do ragdoll: física parcial desligada, tempo normal, velocidades zeradas |
| 38 | 2026-11-25 | Pernas balançando no ar depois de cair | Ragdoll sem amortecimento | Amortecimento linear 0,6 e angular 4 em todos os segmentos |
| 39 | 2026-11-25 | Corpo caído era **lançado para o alto** quando o outro soldado encostava nele | O movimento do personagem (CharacterMovement) aplica força em objetos com física que toca | Ragdoll e peças soltas **ignoram o canal Pawn** (soldados vivos não empurram corpos) |
| 40 | 2026-11-25 | Na decapitação a cabeça ficava **parada no ar** e o corpo era arremessado | A peça solta tinha os corpos físicos do resto do esqueleto (invisíveis, cinemáticos) presos a ela e encostados no corpo | Na peça, corpos e juntas fora do membro são **removidos**; peça não colide com corpos caídos (canal PhysicsBody) |
| 41 | 2026-11-25 | Peças não caíam; ragdoll duro (pernas esticadas) desde o início | O **PhysicsAsset da importação tinha só 4 corpos** (pelve, peito, clavícula esq., mão dir.): a pose de referência tem **escala 100 na raiz** (FBX em metros ×100) e o gerador automático da Unreal mede os ossos errado | Ferramenta própria `UEAWEditorTools::RebuildPhysicsAsset` + `Scripts\refazer_fisica.py`: **18 corpos** (pelve, 3 coluna, pescoço, cabeça, braço/antebraço/mão, coxa/canela/pé) com cápsulas medidas em cm e limites nas juntas (cotovelo/joelho dobram para um lado) |
| 42 | 2026-11-25 | Erro de compilação nas gotas de sangue | Na 4.27 `AddInstance` não aceita "espaço do mundo" | `AddInstanceWorldSpace` |
| 43 | 2026-11-25 | Erro de link `ShouldInstanceSimulatingPhysics` | Função fica no módulo PhysicsCore | `PhysicsCore` nas dependências do `Build.cs` |
| 44 | 2026-11-25 | Gotas de sangue pareciam **pretas** contra o céu | Gotas pequenas e escuras vistas contra a luz | Brilho próprio vermelho no `M_Sangue` (0,5) e gotas 33% maiores |

### Receita Blender → Unreal (testada em 2026-11-24)
1. Esqueleto `SK_Soldado` com **escala 1** (já aplicado). Renomear para `Armature` só durante a exportação.
2. Exportar `Modelos\Export\SK_Soldado.fbx`: só `Armature`, `Alpha_Surface`, `Alpha_Joints`, `SKM_Sabre_Mao`;
   **apply_scale_options = FBX_SCALE_ALL**, global_scale 1, apply_unit_scale, só ossos de deformação, sem leaf bones,
   animação "todas as ações", passo 1, sem simplificar.
3. Importar com `EmpiresAtWar\Scripts\import_soldado.py` (escala **×100** na malha e nas animações):
   `UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="...\import_soldado.py"`.
4. Conferir: altura 180,5 cm, 8 animações `AS_Sabre_*`, 0 avisos "bone transform".
5. **Sempre depois de reimportar:** rodar `Scripts\refazer_fisica.py` (a importação recria o PhysicsAsset com só 4 corpos — bug #41).
6. Peças do desmembramento: exportar do Blender (`Modelos\Export\SK_Peca_*.fbx`) e importar com `Scripts\import_pecas.py`.

## Em aberto
- Dobra do pulso 92–98° em de cima e estocada (acima do ideal de 80°) — conferir a malha de perto e no Unreal.
- Exportação de animações do Chivalry 2 no FModel sai **vazia** (0 trilhas) — só importa para estudo.
- Licença do Mixamo: confirmar antes do lançamento (o documento pede "assets originais").
- Personagem com ~55 mil triângulos: precisa de LODs para 150 x 150.
- Atraso do próprio golpe no multiplayer (sem predição no cliente).
| 45 | 2026-11-25 | Fumaça/gotas com material quadriculado | Material em instâncias precisa de "Used with Instanced Static Meshes"; e na 4.27 o dado por instância só existe no vertex shader | Flag nos scripts + `VertexInterpolator` no `M_Fumaca` |
| 46 | 2026-11-25 | Mira 1ª pessoa tapada pelo ombro | Braços de 1ª pessoa do sabre ficam deslocados/inclinados | Mosquete usa encaixe próprio (corpo igual ao de 3ª pessoa) e a câmera vai ao olho ao mirar |
| 47 | 2026-11-25 | Bot sumia a 6–15 m | O chão do mapa de teste acaba a ~5 m | `-EAWCampo` cria chão de 600 m |
| 48 | 2026-11-25 | Bala não acertava o bot (passava e caía a 96 m) | O rastreio por tipo de objeto para no 1º que bloqueia: a cápsula de movimento e as hitboxes do corpo a corpo escondiam os corpos do esqueleto | Ao achar cápsula/hitbox, ignora e rastreia de novo (até 8 vezes). Teste: acerto a 9,6 m, 399 m/s, 2.190 J |
| 49 | 2026-11-25 | Imagem tombava até ~45° andando de lado | A base da mira copiava a rotação final da câmera (com balanço/tremor) e somava de novo a cada quadro | Dono usa a rotação do controle (mouse) na base da mira e no giro do tronco |
| 50 | 2026-11-25 | Machado/faca flutuando atrás da cabeça | 1) vértices sem peso no osso da mão; 2) trocar a malha da arma não refazia o mapa de ossos | Pesos no Blender; `SetMasterPoseComponent(..., true)` |
| 51 | 2026-11-25 | Movimento lento e "no gelo"; corpo girando grudado no mouse | Velocidade 1,4 m/s (para casar com o passo) e aceleração baixa; rotação do controle direto no corpo | Trote 3,0 m/s com animações novas, arranque/freada ~0,15 s; corpo segue a mira a 540°/s, parado só vira depois de 70° (tronco torce na cintura) |
