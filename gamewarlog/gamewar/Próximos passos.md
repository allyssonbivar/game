---
tags: [planejamento]
atualizado: 2026-11-25
---
# Próximos passos

- [x] Sabre: corte para a direita (2026-11-24)
- [x] Sabre: golpe de cima, estocada, parry, golpe bloqueado, finta (2026-11-24) — ver [[Testes de animação]]
- [x] Exportar o conjunto de teste do sabre para o Unreal e ligar ao `MeleeCombatComponent` (2026-11-24) — duelo de bots testado
- [x] Testar em 1ª pessoa (2026-11-24): mãos fora da tela → precisa de braços próprios para 1ª pessoa
- [x] **Braços de 1ª pessoa** (2026-11-24): cópia do soldado presa à câmera, só para o dono, mesmas animações
- [x] Poses de 1ª pessoa para as preparações e o parry aparecerem na tela (2026-11-24) — versões `_FP`
- [ ] Suavizar o antebraço passando perto da lente (fim do corte esq., preparação do corte dir.)
- [ ] Reduzir a torção do pulso no golpe de cima em 1ª pessoa (79°)
- [ ] Misturar animações (blend) para combos e golpe bloqueado sem "pulo" de pose — AnimBlueprint/montagens
- [ ] Mira vertical (aim offset): o golpe acompanhar o olhar para cima/baixo
- [~] Armas (2026-11-25): mosquete (tiro + baioneta/coronha), faca e machado feitos; faltam lança e pistola
- [ ] Sistema de pegar/largar/trocar armas (pickup) no Unreal + animações de pegar e trocar (referência: troca ~0,5 s, "desce/sobe", aviso no centro — ver [[Chivalry - vídeo 1]])
- [ ] Animações de pulo (impulso, no ar com braços abertos, aterrissagem) e defesa (lâmina na frente do rosto)
- [ ] **Decidido (2026-11-24) — corpos divididos:** partem o corpo só **canhão e golpes muito fortes**; os cortes são **por ponto de impacto** (mais trabalho de arte, cada região tem seus cortes); limite de **20 partes de corpo por área**, depois somem os mais antigos. Próximo passo: definir as zonas de corte (cabeça/pescoço, peito, cintura, ombro, quadril, joelho) e começar com cintura e peito.
- [x] Referência Holdfast vídeo 1 analisada → [[Holdfast - vídeo 1]] (não mostrou corpos partidos)
- [ ] Desmembramento estilo Chivalry (decapitação e membros) para **arma branca e tiros**: pontos de corte no `Soldado.blend` (pescoço, ombros, quadril/joelhos), peças separadas com farda, tampas, jato de sangue (Niagara) — ver [[Visão do jogo]]
- [ ] Referência de violência: gravar o Chivalry 2 em 1ª pessoa com decapitações e membros arrancados (o Holdfast não tem)
- [ ] Gravar mais referência de mortes: artilharia (de perto e de longe), golpes de arma branca fortes, perda de membro (se houver)
- [ ] Morte no Unreal: ragdoll imediato + câmera da morte em 3ª pessoa + mapa de renascimento- [ ] Gravar clipes curtos de referência em 1ª pessoa: pulo no plano, parry, golpe bloqueado, troca, pickup (o Chivalry 2 não tem 3ª pessoa)
- [x] Definir baioneta: **só presa no mosquete** (2026-11-24)
- [ ] Animação de encaixar/tirar a baioneta do mosquete
- [x] Andar (frente, trás, esq., dir.) e correr no Blender (2026-11-24)
- [x] **Movimento no Unreal** (2026-11-24): animador em camadas, velocidades iguais às animações, queda do ragdoll
- [ ] ⏸ **Aguardando o usuário (2026-11-25):** vai comprar (Chivalry: Medieval Warfare e/ou pacote de animações) para usar
      **parâmetros** de jogabilidade como referência (tempos, janelas de defesa, alcance, dano, stamina, velocidades,
      limite de giro) — sem copiar animações, modelos, sons ou código. Parâmetros não resolvem o visual sozinhos.
- [x] Peso e física (2026-11-25): inércia, giro limitado, passo no golpe, trava no impacto, reação física, tremor, ragdoll
- [x] **Sangue e desmembramento** (2026-11-25): gotas, manchas, decapitação, braço, canela, jato, tampas, tela vermelha, `-EAWSemSangue` — ver [[Estado atual]]
      Falta: tampas com carne/osso e farda rasgada (arte), sangue nos tiros (quando houver armas de fogo), ver a tela vermelha em 1ª pessoa,
      cortar também no antebraço/coxa conforme o ponto exato, peças e manchas por área (hoje limite global)
- [ ] **Qualidade "profissional"** (ver [[Chivalry - vídeo 2]]): câmera viva, inércia da arma, desfoque de movimento,
      efeitos de impacto (código) + decidir a fonte das animações (pacote profissional ou captura de movimento)
- [ ] Testar correr (sabre no ombro) e andar de lado no jogo
- [x] Virar no lugar (2026-11-25): tronco torce até 70°, depois os pés viram com passinhos
- [x] Braços de 1ª pessoa balançam com o passo e atrasam o giro (2026-11-25)
- [ ] Corrida: reduzir os 2,4 cm de alcance da perna nos extremos da passada
- [ ] Ferimentos (mancar) e reações de impacto
- [ ] Farda napoleônica no personagem e LODs
- [x] Teste multiplayer — 2 janelas no mesmo PC (2026-11-25). Falta: testar entre 2 PCs pela rede e com mais jogadores
- [ ] Atualizar os PDFs para 150 x 150
- [ ] Ver a recarga completa em jogo e a ferida de bala de perto
- [ ] Animações próprias para faca (empunhadura curta) e machado (golpe de cima pesado) — hoje usam as do sabre
- [x] Pegar armas do chão (F, 3 s, barra) e troca "desce/sobe" (2026-11-25); 2 armas por soldado
- [ ] Desligar o desenho de depuração da lâmina (linha amarela) fora dos testes
- [x] **Pistola** An IX (2026-11-25): modelo, osso próprio, animações de uma mão, tiro e recarga — ver [[Estado atual]]
- [ ] Arma no chão: brilho/contorno quando estiver na mira (hoje só o aviso)
- [x] Conferido (2026-11-25): sabre/faca/machado chegam certos na Unreal; só a pistola girava (bug #52)
- [ ] Recarga da pistola em 1ª pessoa: a mão esquerda passa embaixo no centro ao pôr a pólvora (8,8 cm de folga no alcance da vareta)
- [x] Morte por tiro no pescoço com jato arterial (2026-11-25) — animação Mixamo "Dying"
- [x] Sangue e fumaça com sprites e texturas (2026-11-25)
- [ ] Usuário avaliar o visual do sangue/fumaça no jogo e dizer o que ajustar (quantidade, cor, tamanho)
- [ ] Mixamo: confirmar a licença antes do lançamento (a animação de morte também vem de lá)
- [ ] Cabeça (não pescoço): animação/efeito próprio de tiro na cabeça (referência "brain burst")
- [x] Golpe especial carregado (segurar 3 s + W) para sabre, faca, machado e baioneta (2026-11-26)
- [ ] Bots usarem o golpe especial (e aparar quando virem o braço puxado)
- [ ] Animação própria do especial para o machado (hoje usa a estocada do sabre)
- [x] Morte própria do golpe especial + poça de sangue + sangue no chão até renascer (2026-11-26)
- [ ] Renascimento de verdade (hoje só o reset de teste): ao renascer, apagar o sangue do morto (já ligado em `EndPlay`/reset)
- [ ] Ver com o usuário: `DyingWithouthead.fbx` (morte sem cabeça → decapitação?) e `esfacela.mp4`
