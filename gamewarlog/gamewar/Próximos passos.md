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
- [ ] Faca, machado, lança, baioneta, pistola, rifle (cada um com empunhadura própria)
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
- [ ] Virar no lugar (hoje o corpo gira sem mexer os pés)
- [ ] Braços de 1ª pessoa também balançarem com o passo
- [ ] Corrida: reduzir os 2,4 cm de alcance da perna nos extremos da passada
- [ ] Ferimentos (mancar) e reações de impacto
- [ ] Farda napoleônica no personagem e LODs
- [ ] Teste multiplayer
- [ ] Atualizar os PDFs para 150 x 150
