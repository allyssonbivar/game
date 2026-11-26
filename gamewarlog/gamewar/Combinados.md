---
tags: [regras]
atualizado: 2026-11-26
---
# Combinados

- **Chivalry 2 só como estudo**: tempos, estrutura, técnica, vídeo do jogo como referência.
  Nada de copiar curvas, retargetar animações ou usar assets deles. Nossas animações são feitas do zero.
- **Animações primeiro no Blender**, com **uma ida ao Unreal** depois do conjunto de teste do sabre
  (guarda, corte, parry, golpe bloqueado) para validar escala, 1ª pessoa e tempos. Depois o resto no Blender.
- **Economizar tokens**: uma sessão por assunto, logs filtrados, poucas capturas de tela.
- **Registrar tudo neste cofre** ao fim de cada etapa.

## Armas do soldado (2026-11-25)
- Só **2 armas**: uma de fogo (**mosquete ou pistola**) + uma de corpo a corpo (**espada/sabre, faca ou machado**).
- Achou uma arma no chão: chega perto, **segura a tecla (F) e aparece uma barra de 3 s**; a nova substitui a do mesmo tipo e a antiga é largada.

## Golpe especial (decisão do usuário, 2026-11-26)
- **Segurar o botão esquerdo por 3 s + W** = golpe especial. Na carga o soldado **puxa o braço para trás**; ao soltar **vai com força** (passo longo + estocada que atravessa o corpo).
- Sabre, faca e machado: movimento como no vídeo de referência `Modelos\ex\move1.mp4` (braço esquerdo aponta o alvo, lâmina atrás do quadril). Baioneta: investida como a **Fig. 9** do manual de baioneta (mosquete puxado para trás → estocada longa com as duas mãos).
- Efeito como no vídeo: a lâmina atravessa e o sangue jorra **pelas costas** da vítima por ~1,6 s.

## Baioneta e agachar (decisão do usuário, 2026-11-26)
- Combate da baioneta **como no vídeo `Modelos\ex\x1.mp4` em 3ª pessoa**: guarda com o mosquete na diagonal do peito, estocada da cintura, defesas alta (horizontal sobre a cabeça) e laterais (mosquete em pé). **O golpe especial não muda.**
- **Agachar e andar agachado só com pistolas e mosquetes/rifles** (modo tiro). Arma branca agachada: o usuário ainda vai estudar como o Chivalry faz.

## Melhoria do combate (decisão do usuário, 2026-11-26)
- Ordem: **1) movimento → 2) ataques → 3) defesas → 4) combos**. Foco em **suavidade**. Só o combate corpo a corpo.
- **Sem escudo** no jogo.
- **O golpe especial ("combo da força": segurar o botão esquerdo 3 s + W) não muda.**
- Combos por sequência simples de cliques (ex.: 2 cliques, 3 cliques, cliques + uma tecla) — escolher o mais fácil para o jogador.
- **Combos (2026-11-26):** 2 cliques = dois cortes; 3 cliques = golpe final de cima; 2 cliques + E = estocada final.
- Toda mudança de movimento/animação: conferir de novo o golpe especial (`-EAWAutoEspecial`), que não pode mudar (bug #70).

## Fardas (2026-11-26)
- `Modelos\ex\Textures` e `Modelos\ex\Meshes` são arquivos **extraídos do Holdfast** → **só referência visual** (como o Chivalry 2). As fardas do jogo são feitas do zero por script; nenhuma malha ou textura deles entra no projeto.
- Toda mudança nas fardas: rodar de novo `teste_colisao.py` e conferir o golpe especial (150/160).
