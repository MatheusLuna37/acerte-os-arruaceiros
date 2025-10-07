Acerte os Arruaceiros
=====================

Pequeno jogo/visualizador OpenGL legado (Windows). Este repositório contém o jogo "Acerte os Arruaceiros" escrito em C.

Como compilar (MSYS2 / MinGW64)

Usando a task já fornecida no workspace (recomendado): abra o VS Code e execute a task "Build OpenGL com GCC".

Ou compile manualmente (exemplo):

```powershell
cd C:\Users\W10\Desktop\acerte-os-arruaceiros
C:/msys64/mingw64/bin/gcc.exe -g main.c src/glad.c -o main.exe -Iinclude -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lfreeglut -lopengl32 -lglu32 -lassimp -static-libgcc
```

Como rodar

```
./main.exe <modelo_da_sala.obj>
```

Controles

- B: iniciar / parar jogo
- P: pausa / resume
- V: alterna modo visual (bonecos / quadrados)
- M ou ESC: abre/fecha menu
- Mouse esquerdo: apontar / bater

Arquivos de dados

- `spots.txt`: define as posições dos slots (linhas com: x y z [tipo]) — até 8 linhas.
- `scores.txt`: histórico de partidas (gravado automaticamente).

Notas técnicas

- Texto do HUD e menu usa fontes GLUT (bitmap). O som do martelo usa `Beep()` na plataforma Windows.
