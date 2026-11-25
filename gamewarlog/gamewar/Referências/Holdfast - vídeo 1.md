---
tags: [referencia, holdfast, estudo]
data: 2026-11-24
arquivo: Desktop\Projeto\game\Modelos\holdfast.mp4
---
# Holdfast — análise do vídeo 1 (só referência)

Vídeo de 4 min 2 s, 30 fps, 1280×720. Cenário da Revolução Americana. Tem **1ª e 3ª pessoa**.
**Uso:** referência de tempo, sensação e organização. Não copiar poses, modelos nem animações. Ver [[Combinados]].

## Trechos do vídeo
| Tempo | O que acontece |
|---|---|
| 0:02–1:59 | Menus: escolha de facção, classe e farda (infantaria de linha, sapador, artilharia, cavalaria, porta-bandeira, músicos...) |
| 2:00–2:02 | Mapa para escolher onde nascer |
| 2:03–2:20 | 1ª pessoa: mira pelo cano, disparo, nuvem de fumaça, **círculo de recarga** na tela |
| 2:24–2:57 | Corrida em 1ª pessoa; morte de inimigo na ponte (2:45); recarga com vareta (2:46); gestos de comando com a mão (2:47–2:52) |
| 2:59–3:26 | 3ª pessoa: corrida em trilha |
| 3:26–3:30 | **Morte do jogador**, câmera da morte, mapa de renascimento |
| 3:31–3:57 | 3ª pessoa no acampamento; disparo de mosquete visto de trás com fumaça |

## Mortes e corpos
- **Queda instantânea (ragdoll):** o inimigo na ponte é atingido, abre os braços e está no chão em **0,2–0,4 s**. Não há animação de queda dedicada.
- **Corpos ficam no chão** (pelo menos 4 s visíveis neste vídeo).
- **Câmera da morte:** ao morrer, a tela corta para **3ª pessoa mostrando o próprio corpo** caído (de bruços, braços abertos) por ~0,4 s; depois abre o **mapa de renascimento**; renasce em ~4 s.
- **Neste vídeo não aparecem** corpos partidos, membros arrancados nem tiros de canhão.

## Mosquete (1ª e 3ª pessoa)
- Mira alinhando o cano (sem mira óptica).
- Disparo com **nuvem de fumaça grande que fica no ar ~4 s** (2:09–2:13).
- **Círculo de recarga** no centro da tela durante a recarga (2:14–2:16).
- Recarga com **vareta** visível em 1ª pessoa (2:46).
- Em 3ª pessoa o disparo também gera fumaça na frente do soldado (3:39–3:46).

## O que aproveitar no nosso jogo
- **Morte com ragdoll imediato** + câmera da morte em 3ª pessoa + mapa de renascimento — combina com a seção 8-A do documento.
- **Corpos persistentes** no campo (com o limite de 20 por área que decidimos).
- **Fumaça persistente** do mosquete afetando a visão (seção 6 do documento).
- **Gestos de comando** com a mão (útil para oficiais/cabos na hierarquia).
- **Tela de classe e farda** como referência de organização das classes.

## Falta para os cortes de corpo
Este vídeo não mostra corpos partidos. Para a referência de **canhão e golpes fortes**, gravar:
mortes por artilharia (de perto e de longe), golpes de arma branca na cabeça/tronco e, se o jogo tiver, perda de membro.
