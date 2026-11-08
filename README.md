# Codificador-Aritmetico
Compactador e Descompactador aritmetico que utiliza um modelo fixo e um adaptativo para a compactação

=====================================================================
               COMPACTADOR POR CODIFICAÇÃO ARITMÉTICA
=====================================================================

Compacta e descompacta qualquer arquivo usando codificação aritmética,
com dois modelos de probabilidade à escolha:

  [F] Fixo ........ usa uma tabela de frequências do português.
                    Melhor para textos curtos em português.
  [A] Adaptativo .. aprende as frequências enquanto lê o arquivo.
                    Melhor para qualquer outro tipo de arquivo.


---------------------------------------------------------------------
 REQUISITOS
---------------------------------------------------------------------

  - Windows, Linux ou macOS
  - Compilador g++ (no Windows: MinGW)
  - Os arquivos COMPACTADOR.cpp e TABELA.hpp na mesma pasta


---------------------------------------------------------------------
 COMPILAÇÃO
---------------------------------------------------------------------

  Pelo terminal, dentro da pasta do compactador:

        g++ COMPACTADOR.cpp -o compactador

  Ou pelo VS Code: abra a pasta e compile o COMPACTADOR.cpp. Com a
  extensão de compilação do VS Code, o executável é gerado dentro da
  pasta "output".


---------------------------------------------------------------------
 COMO USAR
---------------------------------------------------------------------

  1. Coloque os arquivos que deseja compactar ou descompactar na
     MESMA PASTA do executável (a pasta do compactador se compilou
     pelo terminal, ou a pasta "output" se compilou pelo VS Code).

  2. Execute o programa:

        Windows:        compactador.exe
        Linux/macOS:    ./compactador

  3. Escolha a operação:

        [C] Compactar
        [D] Descompactar

  4. Digite o nome do arquivo com a extensão.
        Exemplos:  teste.txt   foto.bmp   teste.txt.ari

  5. Ao compactar, escolha também o modelo: [F] Fixo ou [A] Adaptativo.
     Ao descompactar, o modelo é identificado automaticamente.


---------------------------------------------------------------------
 ARQUIVOS GERADOS
---------------------------------------------------------------------

  Compactar     teste.txt       →  teste.txt.ari
  Descompactar  teste.txt.ari   →  teste_descompactado.txt

  O arquivo original nunca é sobrescrito.

  Observações:
  - O modelo fixo só funciona bem com texto em português; em outros
    tipos de arquivo ele aumenta o tamanho.
  - JPG, PNG e MP3 já são formatos comprimidos, por isso quase não
    diminuem.
