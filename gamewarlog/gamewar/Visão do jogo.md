---
tags: [visao, design]
atualizado: 2026-11-24
---
# Visão do jogo

- FPS multiplayer napoleônico, 100% em primeira pessoa, visual realista.
- **150 x 150 jogadores** (300 no total). ⚠️ Os PDFs de concepção ainda dizem 300 x 300.
- Batalha por cidade de até 24 h: invasão (0–6 h), cerco (6–16 h), conquista final (16–24 h).
- Bandeiras começam com o defensor; captura em 10 min, proporção 2:1, mínimo fixo por objetivo.
- Combate corpo a corpo **no estilo Chivalry** (mecânicas), tiro com sensação de Call of Duty adaptada
  a armas de pederneira, organização de exército inspirada em Holdfast.
- Dano por região (cabeça, pescoço, tronco, braços, pernas); até 2 ferimentos não vitais; médicos estabilizam.
- Engine: **Unreal Engine 4.27** (Chaos Destruction é experimental nesta versão).
- Nações: França, Reino Unido, Rússia, Áustria, Prússia, Espanha, Portugal, Ducado de Varsóvia.
- Documentos de concepção: `documentacao_jogo_napoleonico_v2_2.pdf` (mais completo) e `v2 (1).pdf`.

## Violência no combate — decidido em 2026-11-24
**Referência:** a violência do corpo a corpo do Chivalry (cabeça decepada voando, membro arrancado, jato de sangue do coto).
O Holdfast **não** tem isso. Efeitos, modelos e animações serão **feitos por nós** (só referência de estilo).

| Causa | Resultado |
|---|---|
| Arma branca (golpe comum) | Ferimento, sangue, reação por direção |
| Arma branca (golpe forte) | **Decapitação** (pescoço) ou **membro arrancado** (braço/perna), como no Chivalry |
| **Tiro** (mosquete/pistola) | **O mesmo sistema**: em impacto de alta energia (cabeça, perto), pode arrancar membro ou partir a cabeça |
| Bala de canhão / golpe muito forte | **Nosso plus: corpo partido** no ponto de impacto (ver abaixo) |

Como fazer (Unreal 4.27, igual à seção 8-A do documento):
- Esconder o osso atingido no personagem e soltar uma **peça separada** (cabeça, braço, perna) com física, lançada na direção do golpe/tiro.
- **Tampa no coto** (malha de carne) + **jato de sangue** a partir do ponto de corte (Niagara), que diminui em poucos segundos.
- Partes separadas levam a **farda da nação** (manga, bota, chapéu), não pele nua.
- O **servidor decide** o resultado; o cliente mostra. Opção de reduzir/desligar sangue e desmembramento, sem mudar o jogo.
- ⚠️ Esse nível de violência leva a **classificação +18**; avaliar regras das lojas antes do lançamento.

## Corpos partidos e mortes — decidido em 2026-11-24
- **Quem parte o corpo:** só **bala de canhão** e **golpes muito fortes** (machado pesado, espada em golpe muito forte).
  Mosquete e golpes comuns só ferem ou, nos casos previstos na seção 8-A, arrancam membros.
- **Onde parte:** **pelo ponto de impacto**. Zonas de corte planejadas: cabeça/pescoço, peito, cintura, ombro, quadril, joelho.
  Começar por **cintura e peito**.
- **Como fazer:** metades **pré-cortadas** no Blender (com tampa e material de carne) trocadas na hora do impacto;
  não fatiar malha em tempo real (pesado demais para 150 x 150).
- **Limite:** **20 partes de corpo por área**; as mais antigas somem.
- **Morte (referência Holdfast):** ragdoll imediato, câmera da morte em 3ª pessoa mostrando o próprio corpo,
  depois mapa de renascimento. Ver [[Holdfast - vídeo 1]].

## Pegar armas (pickup) — decidido em 2026-11-24
O soldado **pode pegar vários tipos de arma** do chão/cenário (de aliados e inimigos caídos, depósitos etc.),
não só a da classe. Por isso cada arma é um item independente, com seus próprios dados e animações.

## Armas que o personagem poderá usar
| Arma             | Mãos            | Notas para animação                                                                              | Status                    |
| ---------------- | --------------- | ------------------------------------------------------------------------------------------------ | ------------------------- |
| Espada / sabre   | 1               | Guarda, cortes, de cima, estocada, parry                                                         | 🟡 guarda + corte prontos |
| Faca             | 1               | Golpes curtos e rápidos, pouco alcance                                                           | ⚪                         |
| Machado          | 1 ou 2          | Peso na ponta: preparação lenta, golpe pesado                                                    | ⚪                         |
| Lança            | 2               | Estocadas, alcance longo; fraca em espaço fechado                                                | ⚪                         |
| Baioneta         | 2 (no mosquete) | **Só presa no mosquete** (decidido 2026-11-24); pegar baioneta = encaixar no mosquete; estocadas | ⚪                         |
| Pistola          | 1               | Mirar, atirar, recarregar; coronha opcional                                                      | ⚪                         |
| Rifle / mosquete | 2               | Mirar, atirar, recarga longa (15–25 s), baioneta, coronha                                        | ⚪                         |

Cada arma precisa de: empunhadura (pose dos dedos), guarda/idle, ataques, defesa, **pegar/largar/trocar**
e, nas de fogo, mira e recarga. O esqueleto já tem `weapon_l` e `weapon_r` para encaixar qualquer uma.

### Ideia para não multiplicar trabalho
Agrupar armas em **famílias** que dividem animações e só mudam tempos/alcance/peso:
- **Lâmina de 1 mão:** sabre, espada, faca (faca mais rápida e curta), machado leve.
- **Haste de 2 mãos:** lança, mosquete com baioneta (estocadas), machado pesado.
- **Fogo:** pistola (1 mão), rifle/mosquete (2 mãos).
No Unreal, cada arma vira um Data Asset (`MeleeWeaponData`) com tempos, alcance, dano e malha.

Ver também: [[Combinados]], [[Estado atual]].
