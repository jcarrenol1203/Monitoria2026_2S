# Taller de Git — Triqui a punta de commits

Taller introductorio **100% local**, pensado para estudiantes que nunca han usado control de versiones. No necesita GitHub, ni internet, ni cuentas: todo el taller ocurre en una sola carpeta de un solo computador. La dinámica es un único ejercicio — jugar una partida de triqui (tres en raya) en parejas, usando **cada jugada como un commit**.

---

## 0. ¿Qué es el control de versiones?

Es un sistema que guarda "fotos" (versiones) del estado de tus archivos a lo largo del tiempo, para que puedas:

- Ver qué cambió y en qué momento.
- Volver a revisar una versión anterior sin perder la actual.
- Reconstruir, paso a paso, cómo se llegó al estado actual de un proyecto.

**Git** es la herramienta que hace esto en tu computador. En este taller nos quedamos ahí: todo el historial que van a construir vive únicamente en la carpeta de su máquina, en el archivo oculto `.git/` que crea `git init`. No hay ningún servidor de por medio.

---

## 1. Los tres estados de un archivo en Git

Antes de ver comandos, hay que entender por qué `add` y `commit` son dos pasos separados:

```
Working Directory  --git add-->  Staging Area  --git commit-->  Repository (historial)
   (tus archivos          (lo que vas a          (versión guardada
    tal cual los ves)      incluir en el          permanentemente)
                            próximo commit)
```

- **Working directory**: la carpeta del proyecto tal como la ves y editas (el archivo `tablero.txt` mientras lo estás marcando).
- **Staging area** (o "índice"): una zona intermedia donde confirmas *qué cambios* quieres que entren en el próximo commit. En este taller casi siempre será "todo el archivo del tablero".
- **Repository**: el historial permanente de commits ya guardados — la partida completa, jugada por jugada.

---

## 2. Los comandos del taller

Con estos **7 comandos** alcanza para jugar la partida completa y entender lo esencial de Git. No se usa nada de GitHub, remotos, `push`, `pull` ni `merge` — eso queda para una sesión futura.

### `git init`
Convierte la carpeta actual en un repositorio de Git (crea la carpeta oculta `.git/` donde va a vivir todo el historial de la partida). Se corre **una sola vez**, al arrancar.
```bash
git init
```

### `git status`
Muestra qué archivos cambiaron, cuáles están listos para commitear y en qué rama estás. Es el comando que más van a usar — corránlo antes de editar y antes de comitear, en cada turno.
```bash
git status
```

### `git add`
Mueve cambios del *working directory* al *staging area*. Le dice a Git "esto quiero que entre en el próximo commit" — en este taller, casi siempre el tablero recién editado.
```bash
git add tablero.txt
```

### `git commit`
Guarda permanentemente lo que está en staging como una nueva versión del historial, con un mensaje que describe la jugada.
```bash
git commit -m "Jugada 1: X en fila 1, columna 1"
```

### `git log --oneline`
Muestra el historial de commits en una línea por commit: es el **replay de la partida**, jugada por jugada, con su hash y su mensaje. Úsenlo cuando quieran revisar cómo va (o cómo terminó) la partida.
```bash
git log --oneline
```

### `git branch`
Lista las ramas que existen. En este taller van a jugar toda la partida sobre una sola rama (normalmente `master`), así que sirve sobre todo para confirmar dónde están parados.
```bash
git branch
```

### `git checkout`
Sirve para **ver cómo se veía el tablero en un commit anterior**, sin perder ni modificar la partida actual. Es la herramienta para comparar versiones: viajas a un punto pasado, lo revisas, y regresas al presente cuando quieras.
```bash
git checkout <hash-del-commit>    # ir a ver una jugada pasada
git checkout master               # volver al estado actual de la partida
```

---

## 3. Mecánica del taller: parejas, un solo computador

- Se trabaja en **parejas**, compartiendo **un solo computador** — no hace falta que cada quien tenga el suyo, porque no hay que subir nada a ningún lado.
- Los dos jugadores se turnan el teclado: cuando le toca a uno, edita el archivo, hace `add` + `commit`, y le pasa el turno (y el teclado) al otro.
- Como todo pasa en la misma carpeta y el mismo repositorio, no hace falta ni `push` ni `pull` ni configurar nada de red: el turno se "pasa" simplemente soltando el teclado.

---

## 4. Ejercicio — Triqui con historial de versiones (paso a paso)

Objetivo: usar `add`/`commit` como turnos de un juego y `log --oneline` como el "replay" de la partida completa.

### 4.1 Preparación (una sola vez, entre los dos)

1. Abran una terminal y ubíquense en la carpeta donde quieren guardar la partida.
2. Creen una carpeta nueva para el juego y entren en ella:
   ```bash
   mkdir triqui-git
   cd triqui-git
   ```
3. Inicialicen el repositorio:
   ```bash
   git init
   ```
4. Revisen el estado — la carpeta está vacía, así que Git no debería reportar nada para commitear:
   ```bash
   git status
   ```
5. Creen el archivo `tablero.txt` (con su editor de texto favorito, o con el comando que prefieran) y escriban el tablero vacío exactamente así:
   ```
   _ _ _
   _ _ _
   _ _ _
   ```
6. Revisen el estado de nuevo — `tablero.txt` debería aparecer como archivo **sin seguimiento** ("untracked"):
   ```bash
   git status
   ```
7. Agréguenlo al staging area:
   ```bash
   git add tablero.txt
   ```
8. Revisen el estado otra vez — ahora `tablero.txt` debería aparecer listo para el commit:
   ```bash
   git status
   ```
9. Hagan el primer commit, el del tablero vacío:
   ```bash
   git commit -m "Tablero vacio"
   ```
10. Confirmen que quedó guardado:
    ```bash
    git log --oneline
    ```
    Deberían ver un único commit.
11. Decidan quién es **X** y quién es **O**. X siempre empieza.

### 4.2 Cada turno (se repite hasta que termine la partida)

Cuando le toque jugar a alguien, sigue estos pasos en orden:

1. `git status` — confirma que el tablero está limpio (sin cambios pendientes) antes de jugar.
2. Abre `tablero.txt` y reemplaza un `_` por su letra (X u O), en la fila y columna que elija (fila 1 = de arriba, columna 1 = de la izquierda).
3. Guarda el archivo.
4. `git status` — ahora debería mostrar `tablero.txt` como modificado.
5. `git add tablero.txt`
6. `git commit -m "Jugada N: <letra> en fila F, columna C"` (reemplazando N por el número de jugada).
7. Pasa el teclado al rival — ya puede ver el tablero actualizado directamente en el archivo, sin ningún comando extra.

Repitan este ciclo hasta que alguien gane (tres en línea) o se llene el tablero (empate).

### 4.3 Cierre

Al terminar la partida, corran:
```bash
git log --oneline
```
El historial completo es literalmente el replay de la partida, jugada por jugada. Si quieren ver autor y fecha de cada jugada, usen `git log` sin `--oneline`.

---

## 5. Revisando el historial con `checkout`

Objetivo: usar `git checkout` para **comparar cómo se veía el tablero en distintos momentos de la partida**, sin modificar nada.

1. Corran `git log --oneline` y copien el hash de una jugada anterior (por ejemplo, la jugada 2).
2. Viajen a ese punto:
   ```bash
   git checkout <hash>
   ```
   Git va a avisar algo como:
   ```
   You are in 'detached HEAD' state...
   ```
   Esto significa que están viendo el proyecto *tal como estaba* en ese momento — abran `tablero.txt` y compárenlo con el tablero final.
3. Repitan con el hash de otra jugada para comparar dos versiones distintas y ver exactamente qué casilla cambió entre una y otra.
4. Para volver al estado actual de la partida:
   ```bash
   git checkout master
   ```

> **Importante:** mientras están "de visita" en un commit anterior (detached HEAD), no editen ni comiteen nada — es solo para mirar y comparar. Si necesitan volver a jugar, primero regresen con `git checkout master`.
