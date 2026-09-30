# Verificación de herramientas del curso

Este documento te guiará paso a paso para verificar que todas las herramientas necesarias para el curso estén correctamente instaladas y funcionando en tu máquina Linux. Es importante que escribas cada comando manualmente en la terminal — no los copies y pegues. Escribirlos te ayudará a familiarizarte con la línea de comandos, que será tu entorno de trabajo durante todo el semestre.

Para cada herramienta encontrarás: qué es y para qué se usa, cómo verificar si está instalada, y cómo instalarla si hace falta.

---

## 1. bash — El intérprete de comandos

Bash es el programa que interpreta y ejecuta los comandos que escribes en la terminal. Es el lenguaje del sistema operativo desde tu punto de vista como usuario. En prácticamente cualquier sistema Linux ya está instalado, pero vale la pena verificar la versión.

```bash
which bash && bash --version
```

Si el comando devuelve algo como `GNU bash, version 5.x`, está correctamente instalado. Si por alguna razón no estuviera disponible:

```bash
sudo apt install bash
```

---

## 2. git — El sistema de control de versiones

Git es la herramienta central del curso para gestionar el código fuente. Con git podrás guardar el historial de tu trabajo, avanzar de semana en semana y colaborar con otros desarrolladores — exactamente como se hace en la industria.

```bash
which git && git --version
```

Una respuesta como `git version 2.x.x` confirma que está instalado. Si no lo está:

```bash
sudo apt install git
```

Después de instalarlo, es importante configurar tu identidad, porque git la registra en cada confirmación que hagas:

```bash
git config --global user.name "Tu Nombre"
git config --global user.email "tu@correo.com"
```

Puedes verificar que la configuración quedó guardada con:

```bash
git config --list
```

---

## 3. ssh — El cliente de conexión segura

SSH (Secure Shell) es el protocolo que usarás para comunicarte de forma segura con GitHub. Sin SSH, no podrás enviar ni recibir código desde el repositorio remoto. El comando `ssh` es el cliente — el programa que inicia la conexión.

```bash
which ssh && ssh -V
```

Una respuesta como `OpenSSH_8.x` confirma que está instalado. Si no lo está:

```bash
sudo apt install openssh-client
```

---

## 4. Verificación de la llave SSH

Para que GitHub reconozca tu máquina, necesitas una llave SSH personal registrada en tu cuenta. Primero verifica si ya tienes una:

```bash
ls ~/.ssh/
```

Si ves archivos como `id_ed25519` e `id_ed25519.pub`, ya tienes una llave. Si la carpeta está vacía o no existe, debes generar una nueva llave — reemplaza el correo con el tuyo:

```bash
ssh-keygen -t ed25519 -C "tu@correo.com"
```

El sistema te pedirá una frase de contraseña (passphrase). Puedes asignar una o dejarla vacía presionando Enter — para el curso se recomienda asignar una para mayor seguridad.

Una vez generada, muestra tu llave pública para registrarla en GitHub:

```bash
cat ~/.ssh/id_ed25519.pub
```

Copia el texto completo que aparece y agrégalo en GitHub en: **Settings → SSH and GPG keys → New SSH key**.

---

## 5. Verificación de la comunicación con GitHub

Este es el comando más importante de toda la verificación. Prueba la cadena completa de comunicación: red, DNS, GitHub, SSH, llave y cuenta — todo en un solo paso.

```bash
ssh -T git@github.com
```

Si todo está correcto verás exactamente esto (con tu nombre de usuario):

```
Hi tuusuario! You've successfully authenticated, but GitHub does not provide shell access.
```

Si aparece un error de `Permission denied`, revisa que tu llave pública esté correctamente registrada en GitHub y que hayas generado la llave en esta máquina.

---

## 6. curl — La herramienta de transferencia de datos

Curl es una herramienta de línea de comandos para transferir datos a través de protocolos de red como HTTP y HTTPS. Se usa para verificar conectividad, descargar archivos y comunicarse con servicios web. En el contexto del curso, también la usará el sistema de IA en algunos casos.

```bash
which curl && curl --version
```

Una respuesta que comienza con `curl 7.x.x` o superior confirma que está instalado. Si no lo está:

```bash
sudo apt install curl
```

Puedes verificar que tienes acceso a internet y que GitHub es alcanzable con:

```bash
curl -Is https://github.com | head -n 1
```

Si ves `HTTP/2 200`, GitHub está perfectamente alcanzable desde tu máquina.

---

## 7. make — La herramienta de compilación

Make es el programa que coordina el proceso de compilación de un proyecto. Lee un archivo llamado `Makefile` que describe cómo construir el programa, qué archivos dependen de qué otros, y qué comandos ejecutar. Tu entorno de desarrollo lo usa internamente cada vez que compilas tu proyecto.

```bash
which make && make --version
```

Una respuesta como `GNU Make 4.x` confirma que está instalado. Si no lo está:

```bash
sudo apt install make
```

---

## 8. Entorno de desarrollo y compilador ARM

El entorno de desarrollo principal del curso es **Visual Studio Code (VS Code)**, junto con la extensión oficial de STMicroelectronics para desarrollo STM32. A diferencia de las herramientas anteriores, ni VS Code ni esta extensión **se instalan a través de `apt`** — se descargan directamente e instalan el compilador ARM (`arm-none-eabi-gcc`) de forma privada dentro de su propia carpeta, por lo que comandos como `which arm-none-eabi-gcc` no lo encontrarán. Esto es completamente normal.

Primero verifica que VS Code esté instalado:

```bash
which code && code --version
```

Si no aparece nada, descárgalo desde:

```
https://code.visualstudio.com/download
```

Luego, dentro de VS Code, instala la extensión **"STM32CubeIDE for Visual Studio Code"** desde el Marketplace (ícono de extensiones en la barra lateral, o `Ctrl+Shift+X`). Puedes confirmar que quedó instalada desde la terminal:

```bash
code --list-extensions | grep -i stmicroelectronics
```

Si ves una línea como `stmicroelectronics.stm32-vscode-extension`, la extensión está correctamente instalada.

Esta extensión instala también **STM32CubeCLT**, el conjunto de herramientas de línea de comandos que incluye el compilador ARM. Verifica que esté presente buscando su carpeta:

```bash
find /opt/st -maxdepth 1 -iname "stm32cubeclt*" 2>/dev/null
```

Si ves una ruta como `/opt/st/stm32cubeclt_1.17.0`, la instalación está presente. Puedes confirmar que el compilador ARM también está disponible dentro de esa carpeta:

```bash
find /opt/st -name "arm-none-eabi-gcc" 2>/dev/null
```

Deberías ver la ruta completa al compilador, dentro de una subcarpeta llamada `GNU-tools-for-STM32/bin/`.

---

## 9. Codex CLI — El asistente de IA

Codex CLI es el asistente de IA que usarás durante todo el curso. Es el programa que lee el archivo `AGENTS.md` en tu proyecto y te acompaña como asistente de aprendizaje, respetando las reglas de cada semana definidas en `ai-config/`.

Codex CLI se instala a través de Node.js, así que primero verifica que `node` y `npm` estén disponibles:

```bash
which node && node --version
which npm && npm --version
```

Si no están instalados:

```bash
sudo apt install nodejs npm
```

Luego instala Codex CLI de forma global:

```bash
npm install -g @openai/codex
```

Verifica que quedó instalado correctamente:

```bash
which codex && codex --version
```

La primera vez que lo ejecutes, `codex` te pedirá autenticarte (con tu cuenta o con una llave de API). Sigue las instrucciones que aparecen en pantalla.

---

## Resumen de verificación rápida

Una vez que hayas seguido todos los pasos anteriores, puedes ejecutar estos comandos en secuencia como verificación final. Si todos responden correctamente, tu máquina está lista para el curso:

```bash
bash --version
git --version
ssh -V
curl --version
make --version
ssh -T git@github.com
code --version
find /opt/st -maxdepth 1 -iname "stm32cubeclt*" 2>/dev/null
codex --version
```

Si alguno falla, regresa a la sección correspondiente de este documento y sigue los pasos de instalación. Si el problema persiste, consulta con tu profesor antes de continuar.
