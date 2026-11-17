# Sistema de Planificacion de Rutas y Logistica - Recoleccion de Residuos Solidos (Santiago de Surco)

Sistema multiparadigma dividido en tres programas **independientes** que comparten una carpeta comun de archivos CSV (`data/`):

- **Modulo C++ (`cpp/main.cpp`)**: Gestion imperativa estructurada en consola, carga inicial, validacion y registro de entidades, gestion de estados de red y flota, persistencia en CSV, reporte general e integracion de las cuatro funcionalidades adicionales.
- **Modulo Haskell (`haskell/Main.hs`)**: Analisis funcional puro de grafos de vias, calculo de rutas posibles (`DFS`), costos de traslado (`foldr`), priorizacion de pedidos, control de combustible, mantenimiento preventivo y valorizacion de residuos reciclables.
- **Modulo Prolog (`prolog/main.pl`)**: Base de conocimiento logica cargada desde CSV para inferir conexiones directas (`conectado/3`), alcanzabilidad (`alcanzable/2`), rutas con desvios (`ruta_evitando/4`), pedidos inaccesibles (`pedidos_inaccesibles/1`), compatibilidad de camiones, riesgo sanitario, autonomia de combustible y mantenimiento.

---

## Estructura del Proyecto

```text
ProyectoRecoleccionResiduos/
├── data/
│   ├── puntos.csv
│   ├── conexiones.csv
│   ├── unidades.csv
│   ├── pedidos.csv
│   ├── combustible.csv
│   ├── mantenimiento.csv
│   └── historial_mantenimiento.csv
├── cpp/
│   └── main.cpp
├── haskell/
│   └── Main.hs
├── prolog/
│   └── main.pl
└── README.md
```

> **Compatibilidad y flujo con `data/`:** C++ actua como gestor transaccional (lee y escribe los 7 archivos CSV), mientras que Haskell y Prolog operan exclusivamente en modo lectura. Los tres modulos resuelven automaticamente la ruta hacia `data/` (`data/<archivo>.csv`, `../data/<archivo>.csv`, `./<archivo>.csv`).

---

## Esquema de Archivos CSV (`data/`)

| Archivo | Campos | Descripcion |
| :--- | :--- | :--- |
| `puntos.csv` | `codigo,nombre,tipo_punto,zona,dias_sin_recolectar` | Puntos de acopio, mercados, centros de acopio y plantas de transferencia, incluyendo dias acumulados sin recojo *(FA)*. |
| `conexiones.csv` | `origen,destino,distancia_km,tiempo_min,estado` | Tramos viales bidireccionales con distancia, tiempo estimado y estado (`habilitada` o `cerrada`). |
| `unidades.csv` | `codigo,tipo,capacidad_kg,costo_km,estado,especialidad` | Flota de camiones recolectores con capacidad, costo operativo, disponibilidad y especialidad *(FA)*. |
| `pedidos.csv` | `codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada,tipo_residuo` | Solicitudes de recoleccion con carga, nivel de urgencia, estado operativo, unidad asignada y categoria de residuo *(FA)*. |
| `combustible.csv` *(FA)* | `unidad,capacidad_tanque_l,combustible_actual_l,consumo_l_km` | Nivel actual del tanque, capacidad maxima y tasa de consumo por kilometro de cada unidad. |
| `mantenimiento.csv` *(FA)* | `unidad,km_actual,km_ultimo_mantenimiento,intervalo_km` | Odometro acumulado y umbral de kilometraje para mantenimiento preventivo. |
| `historial_mantenimiento.csv` *(FA)* | `codigo_servicio,unidad,fecha,kilometraje,descripcion` | Bitacora historica de servicios preventivos realizados a la flota. |

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

#### Modulo Prolog (SWI-Prolog)
```powershell
# Sesion interactiva desde la raiz (carga automaticamente data/*.csv)
swipl prolog\main.pl

# Consultas de ejemplo dentro de swipl:
# ?- conectado(p01, p08, D).
# ?- alcanzable(p04, p07).
# ?- ruta_evitando(p01, p07, p02, Ruta).
# ?- puede_atender(u02, r02).
# ?- pedidos_inaccesibles(Pedidos).
# ?- camion_compatible(r02, Unidad).
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

#### Modulo Prolog (SWI-Prolog)
```bash
# Desde la raiz del proyecto
swipl prolog/main.pl

# O desde la subcarpeta prolog/
cd prolog
swipl main.pl
```

---

## Alcance Implementado por Integrante (Grupo 5)

### 1. Juan David Morales Rabelly — Clasificacion de residuos y camiones especializados
- **Nucleo base**: Menu principal en C++ (`main.cpp`), carga inicial y persistencia de los 7 archivos CSV (`cargarDatos`, `guardarTodo`), reporte general (`reporteGeneral`) y resumen integral de pedidos en Haskell (`resumenPedido`).
- **Funcionalidad adicional**: Gestion de `especialidad` en unidades y `tipo_residuo` (`domiciliario`, `reciclable`, `desmonte`) en pedidos; validacion estricta de compatibilidad al asignar unidades en C++; calculo funcional de peso y valorizacion economica del reciclaje (`S/ 0.50/kg`) en Haskell mediante `filter`, `map` y `foldr`; y regla logica `camion_compatible/2` en Prolog.

### 2. Jason Ayosa Quiroz — Mantenimiento preventivo de flota
- **Nucleo base**: Gestion de estados en C++ (habilitar/cerrar conexiones con `gestionarConexion`, asignar pedidos con `asignarPedido` y registrar entregas con `marcarPedidoEntregado`); busqueda de nodos adyacentes (`vecinos`), generacion recursiva de caminos simples (`rutasPosibles`) y calculo de costos de ruta con `foldr` (`costoRuta`) en Haskell; reglas `alcanzable/2` y `pedidos_inaccesibles/1` en Prolog.
- **Funcionalidad adicional**: Control de kilometraje acumulado vs. intervalo preventivo (`mantenimiento.csv` e `historial_mantenimiento.csv`), bloqueo de unidades vencidas y registro de servicios en C++; evaluacion funcional y resumen preventivo de flota en Haskell; y predicados `km_hasta_mantenimiento` y `requiere_mantenimiento` en Prolog.

### 3. Diana Condor Vilchez — Riesgo sanitario y priorizacion dinamica
- **Nucleo base**: Consultas operativas en C++ (`consultarPuntos`, `consultarConexionesPunto`, `consultarPedidosPorEstado`, `consultarPedidosPorUnidad`); lectura de CSV mediante `library(csv)` y poblamiento de hechos dinamicos en Prolog (`punto/1`, `detalle_punto/4`, `conexion/5`, `unidad/6`, `pedido/8`, `combustible/4`, `mantenimiento/4`, `historial_mantenimiento/5`), junto con las reglas `ruta_evitando/4` y `puede_atender/2`.
- **Funcionalidad adicional**: Evaluacion de `dias_sin_recolectar` segun tipo de punto, simulacion de avance diario, escalamiento automatico de prioridad de pedidos y sugerencia de unidades en C++; y reglas `nivel_riesgo_sanitario` y `evaluar_riesgo_sanitario` en Prolog.

### 4. Carlos Nicolas Robles Mosqueira — Control de combustible y autonomia
- **Nucleo base**: Registro validado de puntos, conexiones, unidades y pedidos en C++ (`registrarPunto`, `registrarConexion`, `registrarUnidad`, `registrarPedido`), lectura limpia de CSV y estadisticas de pedidos/carga en Haskell (`cargarDatos`, `contarPorEstado`, `cargaPorUnidad`, `pendientesPorPrioridad`), y regla de conexion directa bidireccional `conectado/3` en Prolog.
- **Funcionalidad adicional**: Gestion de `combustible.csv` (abastecimiento, calculo de autonomia en km, validacion previa a la asignacion y descuento por distancia recorrida) en C++; calculo funcional de autonomia y reportes energeticos con `map`, `filter` y `foldr` en Haskell; y reglas logicas de disponibilidad de combustible para completar rutas en Prolog.