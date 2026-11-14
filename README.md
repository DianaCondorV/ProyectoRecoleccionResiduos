# Sistema de Planificacion de Rutas y Logistica - Recoleccion de Residuos Solidos (Santiago de Surco)

Sistema universitario dividido en dos programas **independientes** que comparten una carpeta comun de archivos CSV (`data/`):

- **Modulo C++ (`cpp/main.cpp`)**: Gestion imperativa estructurada en consola, carga inicial, persistencia en CSV, reporte general y puntos de extension para el equipo.
- **Modulo Haskell (`haskell/Main.hs`)**: Analisis funcional puro de grafos de vias, calculo de rutas posibles, costos de traslado y priorizacion de pedidos pendientes.

---

## Estructura del Proyecto

```text
ProyectoRecoleccionResiduos/
├── data/
│   ├── puntos.csv
│   ├── conexiones.csv
│   ├── unidades.csv
│   └── pedidos.csv
├── cpp/
│   └── main.cpp
├── haskell/
│   └── Main.hs
└── README.md
```

> **Compatibilidad con `data/`:** Ambos modulos utilizan la funcion `resolverRutaCSV`, que busca primero `data/<archivo>.csv`, luego `../data/<archivo>.csv` y finalmente `./<archivo>.csv`. Ademas, los parsers leen las columnas base y toleran columnas adicionales presentes en los CSV actuales del repositorio (`dias_sin_recolectar`, `especialidad`, `tipo_residuo`).

---

## Compilacion y Ejecucion Independiente

### 1. Windows (PowerShell)

#### Modulo C++ (desde la raiz del proyecto)

Con `g++` (MinGW / MSYS2):
```powershell
g++ -std=c++17 -Wall -Wextra cpp\main.cpp -o cpp\main.exe
.\cpp\main.exe
```

Si no tienes `g++` moderno (GCC 8+), usa el `clang++` incluido en GHCup (`C:\ghcup\ghc\9.10.3\mingw\bin\clang++.exe`) con `-static` para evitar dependencias de DLLs:
```powershell
C:\ghcup\ghc\9.10.3\mingw\bin\clang++.exe -std=c++17 -static -Wall -Wextra cpp\main.cpp -o cpp\main.exe
.\cpp\main.exe
```

Con Visual Studio 2022 (`cl.exe` desde Developer PowerShell / Developer Command Prompt, pasando **`/std:c++17`** obligatoriamente):
```powershell
cl /std:c++17 /EHsc /W4 /nologo /Fe:cpp\main.exe /Fo:cpp\main.obj cpp\main.cpp
.\cpp\main.exe
```

#### Modulo C++ (desde la subcarpeta `cpp\`)
```powershell
cd cpp
g++ -std=c++17 -Wall -Wextra main.cpp -o main.exe
.\main.exe
```

#### Modulo Haskell (desde la raiz del proyecto)

Con interprete (`runghc`):
```powershell
runghc haskell\Main.hs
```

Compilado (`ghc`):
```powershell
ghc -Wall haskell\Main.hs -o haskell\Main.exe
.\haskell\Main.exe
```

#### Modulo Haskell (desde la subcarpeta `haskell\`)
```powershell
cd haskell
runghc Main.hs
```
O compilado:
```powershell
cd haskell
ghc -Wall Main.hs -o Main.exe
.\Main.exe
```

---

### 2. Windows (CMD / Simbolo del sistema)

#### Modulo C++
```bat
REM Desde la raiz del proyecto
g++ -std=c++17 -Wall -Wextra cpp\main.cpp -o cpp\main.exe
cpp\main.exe

REM O usando clang++ de GHCup:
C:\ghcup\ghc\9.10.3\mingw\bin\clang++.exe -std=c++17 -static -Wall -Wextra cpp\main.cpp -o cpp\main.exe
cpp\main.exe

REM Desde la subcarpeta cpp\
cd cpp
g++ -std=c++17 -Wall -Wextra main.cpp -o main.exe
main.exe
```

#### Modulo Haskell
```bat
REM Desde la raiz del proyecto
runghc haskell\Main.hs

REM O compilado:
ghc -Wall haskell\Main.hs -o haskell\Main.exe
haskell\Main.exe

REM Desde la subcarpeta haskell\
cd haskell
runghc Main.hs
```

---

### 3. Linux / macOS (Bash / Zsh)

#### Modulo C++
```bash
# Desde la raiz del proyecto
g++ -std=c++17 -Wall -Wextra cpp/main.cpp -o cpp/main
./cpp/main

# Desde la subcarpeta cpp/
cd cpp
g++ -std=c++17 -Wall -Wextra main.cpp -o main
./main
```

#### Modulo Haskell
```bash
# Desde la raiz del proyecto
runghc haskell/Main.hs

# O compilado:
ghc -Wall haskell/Main.hs -o haskell/Main
./haskell/Main

# Desde la subcarpeta haskell/
cd haskell
runghc Main.hs
```

---

## Alcance Implementado y Puntos de Extension

### Implementacion base (Juan David Morales Rabelly)
- **C++ `[4.1.1]`**: Menu principal interactivo adaptado a recoleccion de residuos en Surco (invoca `guardarTodo()` al volver de las opciones 1 y 2).
- **C++ `[4.1.2]`**: Carga inicial de `puntos.csv`, `conexiones.csv`, `unidades.csv` y `pedidos.csv` (creacion automatica con cabecera si faltan, soporte `\r\n` y campos vacios al final).
- **C++ `[4.1.5]`**: Persistencia CSV (`guardarPuntos`, `guardarConexiones`, `guardarUnidades`, `guardarPedidos`, `guardarTodo`).
- **C++ `[4.1.7]`**: Reporte general del sistema en memoria.
- **Haskell `[4.2.7]`**: Orquestacion `main :: IO ()`, seleccion interactiva con valor por defecto (Enter) de pedido pendiente y unidad disponible, calculo de ruta optima y salida formateada.

### Puntos de integracion del equipo
- **C++**: Stubs comentados con `// TODO [4.1.3 - Carlos]`, `// TODO [4.1.4 - Jason]` y `// TODO [4.1.6 - Diana]`.
- **Haskell**: Implementaciones base funcionales delimitadas por comentarios para `[4.2.1 y 4.2.2 - Carlos]`, `[4.2.3, 4.2.4 y 4.2.5 - Jason]` y `[4.2.6 - Carlos]`.
