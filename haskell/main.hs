module Main (main) where

-- ============================================================================
-- 1. IMPORTACIONES Y UTILIDADES GENERALES
-- ============================================================================

import Control.Monad (filterM, unless, when)
import Data.Char (isSpace, toLower)
import Data.List (find, intercalate, minimumBy, nub, sort, sortBy)
import Data.Maybe (listToMaybe, mapMaybe)
import Data.Ord (comparing)
import System.Directory (doesFileExist)
import System.Exit (exitFailure)
import System.IO
import Text.Printf (printf)

-- Quita los espacios al inicio y al final de un texto.
recortar :: String -> String
recortar = quitar . quitar
  where
    -- Quita los espacios del inicio y da vuelta el texto (se aplica dos veces).
    quitar :: String -> String
    quitar = reverse . dropWhile isSpace

-- Pasa un texto a minusculas (para comparar estados y tipos).
minusculas :: String -> String
minusculas = map toLower

-- Convierte un texto en numero decimal; Nothing si no es un numero valido.
leerDecimal :: String -> Maybe Double
leerDecimal texto =
    case reads (recortar texto) :: [(Double, String)] of
        [(valor, "")] -> Just valor
        _             -> Nothing

-- Convierte un texto en numero entero; Nothing si no es un entero valido.
leerEntero :: String -> Maybe Int
leerEntero texto =
    case reads (recortar texto) :: [(Int, String)] of
        [(valor, "")] -> Just valor
        _             -> Nothing

-- Da formato de dos decimales a un numero (ejemplo: 5.2 -> "5.20").
formato2 :: Double -> String
formato2 x = printf "%.2f" x

-- Muestra un numero sin decimales si es entero (1200.0 -> "1200").
mostrarNumero :: Double -> String
mostrarNumero x
    | x == fromIntegral (round x :: Integer) = show (round x :: Integer)
    | otherwise                              = printf "%.2f" x

-- Rellena un texto con espacios a la derecha hasta alcanzar un ancho.
columna :: Int -> String -> String
columna ancho texto = texto ++ replicate (ancho - length texto) ' '


-- ============================================================================
-- 2. TIPOS DE DATOS
-- ============================================================================

-- ---------------------------------------------------------------------------
-- [4.2.2] DEFINICION DE TIPOS - Autor: Carlos
-- ---------------------------------------------------------------------------

-- Una ruta es una lista de codigos de puntos (ejemplo: ["P01","P03","P06"]).
type Ruta = [String]

-- Tramo entre dos puntos (doble sentido, registrado una sola vez).
data Conexion = Conexion
    { cOrigen    :: String
    , cDestino   :: String
    , cDistancia :: Double   -- kilometros
    , cTiempo    :: Int      -- minutos
    , cEstado    :: String   -- "habilitada" o "cerrada"
    } deriving (Show, Eq)

-- Camion recolector.
data Unidad = Unidad
    { uCodigo       :: String
    , uTipo         :: String
    , uCapacidad    :: Double   -- kilogramos
    , uCostoKm      :: Double   -- soles por kilometro
    , uEstado       :: String   -- "disponible" u "ocupada"
    , uEspecialidad :: String   -- FA - Autor: Juan
    } deriving (Show, Eq)

-- Solicitud de traslado de residuos.
data Pedido = Pedido
    { pCodigo       :: String
    , pOrigen       :: String
    , pDestino      :: String
    , pCarga        :: Double   -- kilogramos
    , pPrioridad    :: String   -- "alta", "media" o "baja"
    , pEstado       :: String   -- "pendiente", "asignado" o "entregado"
    , pUnidad       :: String   -- unidad asignada ("" si no tiene)
    , pTipoResiduo  :: String   -- FA - Autor: Juan
    } deriving (Show, Eq)

-- FA - Autor: Jason
-- Control de mantenimiento preventivo de un camion.
data Mantenimiento = Mantenimiento
    { mUnidad    :: String
    , mKmActual  :: Double
    , mKmUltimo  :: Double   -- kilometraje del ultimo servicio
    , mIntervalo :: Double   -- km permitidos entre servicios
    } deriving (Show, Eq)

-- FA - Autor: Carlos
-- Control de combustible de un camion.
data Combustible = Combustible
    { cbUnidad    :: String
    , cbCapacidad :: Double   -- litros del tanque
    , cbActual    :: Double   -- litros disponibles
    , cbConsumo   :: Double   -- litros por kilometro
    } deriving (Show, Eq)

-- Conjunto de datos leidos de los CSV.
data Datos = Datos
    { dConexiones     :: [Conexion]
    , dUnidades       :: [Unidad]
    , dPedidos        :: [Pedido]
    , dMantenimientos :: [Mantenimiento]
    , dCombustibles   :: [Combustible]
    }


-- ============================================================================
-- 3. LECTURA DE ARCHIVOS CSV
-- ============================================================================

-- ---------------------------------------------------------------------------
-- [4.2.1] LECTURA DE ARCHIVOS CSV - Autor: Carlos
-- ---------------------------------------------------------------------------

-- Quita la marca BOM que algunos editores agregan al inicio del archivo.
quitarBOM :: String -> String
quitarBOM ('\xFEFF' : resto) = resto
quitarBOM texto              = texto

-- Divide una linea CSV en campos respetando las comillas.
-- Ejemplo: a,"b,c",d  ->  ["a","b,c","d"]. Los campos vacios se conservan.
dividirCSV :: String -> [String]
dividirCSV = recorrer [] False
  where
    -- Recorre la linea: acumulado (en orden inverso), indicador de comillas
    -- y texto restante.
    recorrer :: String -> Bool -> String -> [String]
    recorrer acc _ [] = [reverse acc]
    recorrer acc True ('"' : '"' : resto) = recorrer ('"' : acc) True resto
    recorrer acc enComillas ('"' : resto) = recorrer acc (not enComillas) resto
    recorrer acc False (',' : resto) = reverse acc : recorrer [] False resto
    recorrer acc enComillas (c : resto) = recorrer (c : acc) enComillas resto

-- Lee un archivo completo en UTF-8 (lectura estricta).
leerArchivo :: FilePath -> IO String
leerArchivo ruta = do
    manejador <- openFile ruta ReadMode
    hSetEncoding manejador utf8
    contenido <- hGetContents manejador
    length contenido `seq` hClose manejador
    return contenido

-- Lee un CSV y devuelve sus registros (sin la cabecera ni lineas vacias).
-- Si el archivo no existe devuelve una lista vacia.
leerRegistros :: FilePath -> IO [[String]]
leerRegistros ruta = do
    existe <- doesFileExist ruta
    if not existe
        then return []
        else do
            contenido <- leerArchivo ruta
            let lineasUtiles = filter (not . all isSpace)
                                   (lines (filter (/= '\r') (quitarBOM contenido)))
            return (map dividirCSV (drop 1 lineasUtiles))

-- Convierte un registro en una conexion (Nothing si esta mal formado).
parsearConexion :: [String] -> Maybe Conexion
parsearConexion (o : d : dist : t : e : _) = do
    distancia <- leerDecimal dist
    tiempo    <- leerEntero t
    return (Conexion (recortar o) (recortar d) distancia tiempo
                     (minusculas (recortar e)))
parsearConexion _ = Nothing

-- Convierte un registro en una unidad (Nothing si esta mal formado).
parsearUnidad :: [String] -> Maybe Unidad
parsearUnidad (cod : tipo : cap : costo : est : esp : _) = do
    capacidad <- leerDecimal cap
    costoKm   <- leerDecimal costo
    return (Unidad (recortar cod) (recortar tipo) capacidad costoKm
                   (minusculas (recortar est)) (minusculas (recortar esp)))
parsearUnidad _ = Nothing

-- Convierte un registro en un pedido (Nothing si esta mal formado).
parsearPedido :: [String] -> Maybe Pedido
parsearPedido (cod : ori : des : carga : prio : est : uni : resto) = do
    cargaKg <- leerDecimal carga
    let tipoResiduo = case resto of
            (t : _) -> minusculas (recortar t)
            []      -> ""
    return (Pedido (recortar cod) (recortar ori) (recortar des) cargaKg
                   (minusculas (recortar prio)) (minusculas (recortar est))
                   (recortar uni) tipoResiduo)
parsearPedido _ = Nothing

-- FA - Autor: Jason
-- Convierte un registro en un control de mantenimiento.
parsearMantenimiento :: [String] -> Maybe Mantenimiento
parsearMantenimiento (u : actual : ultimo : intervalo : _) = do
    kmActual <- leerDecimal actual
    kmUltimo <- leerDecimal ultimo
    kmInterv <- leerDecimal intervalo
    return (Mantenimiento (recortar u) kmActual kmUltimo kmInterv)
parsearMantenimiento _ = Nothing

-- FA - Autor: Carlos
-- Convierte un registro en un control de combustible.
parsearCombustible :: [String] -> Maybe Combustible
parsearCombustible (u : cap : actual : consumo : _) = do
    capacidad <- leerDecimal cap
    litros    <- leerDecimal actual
    consumoKm <- leerDecimal consumo
    return (Combustible (recortar u) capacidad litros consumoKm)
parsearCombustible _ = Nothing

-- Carga todos los CSV de la carpeta indicada.
cargarDatos :: FilePath -> IO Datos
cargarDatos dir = do
    regConexiones <- leerRegistros (dir ++ "conexiones.csv")
    regUnidades   <- leerRegistros (dir ++ "unidades.csv")
    regPedidos    <- leerRegistros (dir ++ "pedidos.csv")
    regMantto     <- leerRegistros (dir ++ "mantenimiento.csv")
    regCombust    <- leerRegistros (dir ++ "combustible.csv")
    return Datos
        { dConexiones     = mapMaybe parsearConexion regConexiones
        , dUnidades       = mapMaybe parsearUnidad regUnidades
        , dPedidos        = mapMaybe parsearPedido regPedidos
        , dMantenimientos = mapMaybe parsearMantenimiento regMantto
        , dCombustibles   = mapMaybe parsearCombustible regCombust
        }

-- Busca la carpeta data/ probando varias rutas relativas.
-- Funciona al ejecutar desde la raiz del proyecto o desde codigo/haskell.
buscarDirectorioData :: IO FilePath
buscarDirectorioData = buscar candidatos
  where
    -- Carpetas donde se intenta encontrar los CSV.
    candidatos :: [FilePath]
    candidatos = [ "data/", "../data/", "../../data/", "../../../data/"
                 , "../cpp/data/" ]

    -- Devuelve la primera carpeta que contiene conexiones.csv.
    buscar :: [FilePath] -> IO FilePath
    buscar []       = return "data/"
    buscar (d : ds) = do
        existe <- doesFileExist (d ++ "conexiones.csv")
        if existe then return d else buscar ds

-- Verifica que existan los tres CSV obligatorios; si no, termina el programa.
verificarArchivos :: FilePath -> IO ()
verificarArchivos dir = do
    faltantes <- filterM (\n -> fmap not (doesFileExist (dir ++ n)))
                         ["conexiones.csv", "unidades.csv", "pedidos.csv"]
    unless (null faltantes) $ do
        putStrLn "[ERROR] No se encontraron los archivos CSV obligatorios:"
        mapM_ (\n -> putStrLn ("  - " ++ dir ++ n)) faltantes
        putStrLn "Ejecute el programa desde la carpeta del proyecto o desde"
        putStrLn "codigo/haskell, y verifique que exista la carpeta data/."
        exitFailure


-- ============================================================================
-- 4. VECINOS DE UN PUNTO
-- ============================================================================

-- Indica si una conexion esta habilitada.
estaHabilitada :: Conexion -> Bool
estaHabilitada c = cEstado c == "habilitada"

-- Comprueba si un punto participa en al menos una conexion de la red.
existePuntoEnRed :: [Conexion] -> String -> Bool
existePuntoEnRed conexiones punto =
    any (\c -> cOrigen c == punto || cDestino c == punto) conexiones

-- ---------------------------------------------------------------------------
-- [4.2.3] FUNCION DE VECINOS - Autor: Jason
-- Devuelve los puntos conectados directamente a un punto. Considera el doble
-- sentido de las conexiones y solo las conexiones habilitadas.
-- ---------------------------------------------------------------------------

vecinos :: [Conexion] -> String -> [String]
vecinos conexiones punto =
    sort (nub (concatMap extremoOpuesto (filter estaHabilitada conexiones)))
  where
    -- Devuelve el otro extremo de la conexion si toca al punto, o nada.
    extremoOpuesto :: Conexion -> [String]
    extremoOpuesto c
        | cOrigen c == punto  = [cDestino c]
        | cDestino c == punto = [cOrigen c]
        | otherwise           = []


-- ============================================================================
-- 5. RUTAS POSIBLES
-- ============================================================================

-- ---------------------------------------------------------------------------
-- [4.2.4] RUTAS POSIBLES - Autor: Jason
-- Funcion recursiva que genera todas las rutas entre un origen y un destino
-- sin repetir puntos, usando solo conexiones habilitadas.
-- ---------------------------------------------------------------------------

rutasPosibles :: [Conexion] -> String -> String -> [Ruta]
rutasPosibles conexiones origen destino = explorar [origen]
  where
    -- Explora recursivamente; el camino se acumula en orden inverso
    -- (el punto actual va primero) y se evita repetir puntos.
    explorar :: [String] -> [Ruta]
    explorar [] = []
    explorar camino@(actual : _)
        | actual == destino = [reverse camino]
        | otherwise         = concatMap (\v -> explorar (v : camino)) siguientes
      where
        siguientes = filter (`notElem` camino) (vecinos conexiones actual)


-- ============================================================================
-- 6. COSTO DE UNA RUTA
-- ============================================================================

-- Busca el tramo entre dos puntos en cualquier sentido.
buscarTramo :: [Conexion] -> String -> String -> Maybe Conexion
buscarTramo conexiones a b = find coincide conexiones
  where
    -- Verifica si la conexion une a con b (en cualquier sentido).
    coincide :: Conexion -> Bool
    coincide c = (cOrigen c == a && cDestino c == b)
              || (cOrigen c == b && cDestino c == a)

-- Texto para mostrar una ruta: P01 -> P03 -> P06
formatearRuta :: Ruta -> String
formatearRuta = intercalate " -> "

-- ---------------------------------------------------------------------------
-- [4.2.5] COSTO DE UNA RUTA - Autor: Jason
-- Dada una ruta (lista de puntos) calcula su distancia y tiempo total.
-- Si algun tramo no existe o esta cerrado devuelve Left con el motivo.
-- ---------------------------------------------------------------------------

costoRuta :: [Conexion] -> Ruta -> Either String (Double, Int)
costoRuta _ []  = Left "La ruta esta vacia."
costoRuta _ [_] = Left "La ruta necesita al menos dos puntos."
costoRuta conexiones (inicio : siguiente : resto) =
    foldr combinar (Right (0, 0))
        ((inicio, siguiente) : tramos (siguiente : resto))
  where
    tramos :: Ruta -> [(String, String)]
    tramos (a : b : xs) = (a, b) : tramos (b : xs)
    tramos _ = []

    -- Suma un tramo al acumulado; falla si el tramo no existe o esta cerrado.
    combinar :: (String, String) -> Either String (Double, Int)
             -> Either String (Double, Int)
    combinar (a, b) acumulado =
        case buscarTramo conexiones a b of
            Nothing ->
                Left ("El tramo " ++ a ++ " -> " ++ b ++ " no existe.")
            Just c
                | not (estaHabilitada c) ->
                    Left ("El tramo " ++ a ++ " -> " ++ b ++ " esta cerrado.")
                | otherwise ->
                    fmap (\(km, mins) -> (km + cDistancia c, mins + cTiempo c))
                         acumulado

-- Costo en soles de recorrer una distancia con una unidad.
costoEnSoles :: Unidad -> Double -> Double
costoEnSoles unidad km = km * uCostoKm unidad


-- ============================================================================
-- 7. REPORTE DE PEDIDOS
-- ============================================================================

-- Peso numerico de una prioridad (menor numero = mas urgente).
pesoPrioridad :: String -> Int
pesoPrioridad "alta"  = 0
pesoPrioridad "media" = 1
pesoPrioridad "baja"  = 2
pesoPrioridad _       = 3

-- Suma las cargas de una lista de pedidos (usa foldr).
sumarCargas :: [Pedido] -> Double
sumarCargas = foldr (\p acumulado -> pCarga p + acumulado) 0

-- ---------------------------------------------------------------------------
-- [4.2.6] REPORTE DE PEDIDOS - Autor: Carlos
-- Cuenta los pedidos por estado, calcula la carga total por unidad y lista
-- los pedidos pendientes ordenados por prioridad.
-- ---------------------------------------------------------------------------

-- Cantidad de pedidos por cada estado.
contarPorEstado :: [Pedido] -> [(String, Int)]
contarPorEstado pedidos =
    map (\estado -> (estado, length (filter (\p -> pEstado p == estado) pedidos)))
        ["pendiente", "asignado", "entregado"]

-- Carga total de una unidad en un estado de pedido determinado.
cargaDeUnidad :: String -> String -> [Pedido] -> Double
cargaDeUnidad codigo estado pedidos = sumarCargas (filter coincide pedidos)
  where
    -- Verifica que el pedido sea de la unidad y este en el estado buscado.
    coincide :: Pedido -> Bool
    coincide p = pUnidad p == codigo && pEstado p == estado

-- Carga por unidad: (unidad, carga asignada en curso, carga ya entregada).
cargaPorUnidad :: [Unidad] -> [Pedido] -> [(String, Double, Double)]
cargaPorUnidad unidades pedidos =
    map (\u -> ( uCodigo u
               , cargaDeUnidad (uCodigo u) "asignado" pedidos
               , cargaDeUnidad (uCodigo u) "entregado" pedidos ))
        unidades

-- Pedidos pendientes ordenados por prioridad (alta, media, baja).
pendientesPorPrioridad :: [Pedido] -> [Pedido]
pendientesPorPrioridad pedidos =
    sortBy (comparing (\p -> (pesoPrioridad (pPrioridad p), pCodigo p)))
           (filter (\p -> pEstado p == "pendiente") pedidos)


-- ============================================================================
-- 8. RESUMEN DE UN PEDIDO
-- ============================================================================

-- Elige la unidad con que se calcula el costo de un pedido: la asignada o,
-- si no tiene, una compatible (misma especialidad y capacidad suficiente),
-- prefiriendo las disponibles.
unidadParaPedido :: [Unidad] -> Pedido -> Maybe Unidad
unidadParaPedido unidades pedido
    | pUnidad pedido /= "" = find (\u -> uCodigo u == pUnidad pedido) unidades
    | otherwise            = listToMaybe (disponibles ++ compatibles)
  where
    -- Unidades que pueden llevar el pedido.
    compatibles :: [Unidad]
    compatibles = filter compatible unidades

    -- Verifica especialidad y capacidad suficiente.
    compatible :: Unidad -> Bool
    compatible u = uEspecialidad u == pTipoResiduo pedido
                && uCapacidad u >= pCarga pedido

    -- Unidades compatibles que ademas estan disponibles.
    disponibles :: [Unidad]
    disponibles = filter (\u -> uEstado u == "disponible") compatibles

-- ---------------------------------------------------------------------------
-- [4.2.7] RESULTADO ESPERADO - Autor: Juan
-- Arma el resumen de un pedido: cantidad de rutas posibles, costo de la ruta
-- mas corta con la unidad elegida y pedidos pendientes por prioridad.
-- ---------------------------------------------------------------------------

resumenPedido :: Datos -> Pedido -> [String]
resumenPedido datos pedido =
    [ "Pedido: " ++ pCodigo pedido
        ++ " (" ++ pOrigen pedido ++ " -> " ++ pDestino pedido ++ ")"
    , "Rutas posibles: " ++ show (length rutas)
    ] ++ lineasCosto ++
    [ "Pedidos pendientes por prioridad: " ++ textoPendientes ]
  where
    conexiones = dConexiones datos
    rutas      = rutasPosibles conexiones (pOrigen pedido) (pDestino pedido)

    -- Rutas con su distancia y tiempo (solo las que se pueden costear).
    conCosto :: [(Ruta, Double, Int)]
    conCosto = [ (r, km, mins)
               | r <- rutas
               , Right (km, mins) <- [costoRuta conexiones r] ]

    -- Ruta de menor distancia (Nothing si no hay ninguna habilitada).
    masCorta :: Maybe (Ruta, Double, Int)
    masCorta
        | null conCosto = Nothing
        | otherwise     = Just (minimumBy (comparing (\(_, km, _) -> km)) conCosto)

    -- Unidad con la que se calcula el costo en soles.
    unidad :: Maybe Unidad
    unidad = unidadParaPedido (dUnidades datos) pedido

    -- Todos los pedidos pendientes, ordenados por prioridad.
    pendientes :: [Pedido]
    pendientes = pendientesPorPrioridad (dPedidos datos)

    -- Texto con la lista de pendientes: D004 (alta), D002 (media), ...
    textoPendientes :: String
    textoPendientes
        | null pendientes = "ninguno"
        | otherwise       = intercalate ", " (map etiqueta pendientes)

    -- Etiqueta de un pedido: codigo y prioridad.
    etiqueta :: Pedido -> String
    etiqueta p = pCodigo p ++ " (" ++ pPrioridad p ++ ")"

    -- Lineas con el costo de la ruta mas corta (o el aviso si no hay ruta).
    lineasCosto :: [String]
    lineasCosto =
        case masCorta of
            Nothing ->
                [ "No existe una ruta habilitada entre "
                    ++ pOrigen pedido ++ " y " ++ pDestino pedido ++ "." ]
            Just (ruta, km, mins) ->
                case unidad of
                    Just u ->
                        [ "Costo de ruta " ++ formatearRuta ruta
                            ++ " (" ++ uTipo u ++ " " ++ uCodigo u ++ "): S/ "
                            ++ formato2 (costoEnSoles u km)
                        , "Distancia: " ++ formato2 km ++ " km | Tiempo: "
                            ++ show mins ++ " min"
                        ]
                    Nothing ->
                        [ "Ruta mas corta " ++ formatearRuta ruta
                            ++ ": " ++ formato2 km ++ " km, "
                            ++ show mins ++ " min"
                        , "Sin unidad compatible para calcular el costo en soles."
                        ]


-- ============================================================================
-- 9. FA - MANTENIMIENTO PREVENTIVO (Autor: Jason)
-- ============================================================================

-- Resultado de evaluar el mantenimiento de un camion.
data ResultadoMant = ResultadoMant
    { rmUnidad    :: String
    , rmRecorrido :: Double   -- km recorridos desde el ultimo servicio
    , rmRestante  :: Double   -- km restantes (<= 0 significa pendiente)
    , rmIntervalo :: Double
    }

-- FA - Autor: Jason
-- Transforma un registro de mantenimiento en su resultado.
evaluarMantenimiento :: Mantenimiento -> ResultadoMant
evaluarMantenimiento m =
    ResultadoMant (mUnidad m) recorrido (mIntervalo m - recorrido) (mIntervalo m)
  where
    -- Km recorridos desde el ultimo servicio.
    recorrido = mKmActual m - mKmUltimo m

-- FA - Autor: Jason
-- Indica si el camion requiere mantenimiento.
estaPendiente :: ResultadoMant -> Bool
estaPendiente r = rmRestante r <= 0

-- FA - Autor: Jason
-- Usa map para evaluar toda la flota.
resultadosMantenimiento :: [Mantenimiento] -> [ResultadoMant]
resultadosMantenimiento = map evaluarMantenimiento

-- FA - Autor: Jason
-- Usa filter para seleccionar los camiones con mantenimiento pendiente.
mantenimientosPendientes :: [ResultadoMant] -> [ResultadoMant]
mantenimientosPendientes = filter (\r -> estaPendiente r)

-- FA - Autor: Jason
-- Usa foldr para obtener el resumen de la flota:
-- (camiones evaluados, pendientes, al dia).
resumenFlota :: [ResultadoMant] -> (Int, Int, Int)
resumenFlota = foldr acumular (0, 0, 0)
  where
    -- Suma un camion al conteo segun este pendiente o al dia.
    acumular :: ResultadoMant -> (Int, Int, Int) -> (Int, Int, Int)
    acumular r (total, pendientes, alDia)
        | estaPendiente r = (total + 1, pendientes + 1, alDia)
        | otherwise       = (total + 1, pendientes, alDia + 1)

-- FA - Autor: Jason
-- Texto descriptivo del resultado de un camion.
describirMantenimiento :: ResultadoMant -> String
describirMantenimiento r
    | rmRestante r > 0 =
        rmUnidad r ++ ": le faltan " ++ mostrarNumero (rmRestante r)
            ++ " km (recorrio " ++ mostrarNumero (rmRecorrido r)
            ++ " de " ++ mostrarNumero (rmIntervalo r) ++ ")"
    | rmRestante r == 0 =
        rmUnidad r ++ ": llego a su intervalo, mantenimiento pendiente"
    | otherwise =
        rmUnidad r ++ ": excedio su intervalo en "
            ++ mostrarNumero (negate (rmRestante r)) ++ " km"


-- ============================================================================
-- 10. FA - COMBUSTIBLE Y AUTONOMIA (Autor: Carlos)
-- ============================================================================

-- FA - Autor: Carlos
-- Autonomia en kilometros con el combustible disponible.
autonomia :: Combustible -> Double
autonomia c = cbActual c / cbConsumo c

-- FA - Autor: Carlos
-- Litros necesarios para recorrer una distancia.
litrosNecesarios :: Double -> Combustible -> Double
litrosNecesarios distancia c = distancia * cbConsumo c

-- FA - Autor: Carlos
-- Litros que faltan antes de iniciar un traslado (0 si alcanza).
litrosFaltantes :: Double -> Combustible -> Double
litrosFaltantes distancia c = max 0 (litrosNecesarios distancia c - cbActual c)

-- FA - Autor: Carlos
-- Usa filter para identificar las unidades sin combustible suficiente.
unidadesInsuficientes :: Double -> [Combustible] -> [Combustible]
unidadesInsuficientes distancia = filter (\c -> autonomia c < distancia)

-- FA - Autor: Carlos
-- Usa foldr para obtener el combustible total disponible de la flota.
combustibleTotal :: [Combustible] -> Double
combustibleTotal = foldr (\c acumulado -> cbActual c + acumulado) 0


-- ============================================================================
-- 11. FA - VALORIZACION DE RECICLAJE (Autor: Juan)
-- ============================================================================

-- FA - Autor: Juan
-- Precio estimado de valorizacion por kilogramo de residuo reciclable (S/).
precioPorKgReciclable :: Double
precioPorKgReciclable = 0.50

-- FA - Autor: Juan
-- Usa filter para seleccionar los pedidos reciclables ya entregados.
reciclablesEntregados :: [Pedido] -> [Pedido]
reciclablesEntregados =
    filter (\p -> pTipoResiduo p == "reciclable" && pEstado p == "entregado")

-- FA - Autor: Juan
-- Usa map para calcular la valorizacion de cada pedido.
valorizaciones :: [Pedido] -> [Double]
valorizaciones = map (\p -> pCarga p * precioPorKgReciclable)

-- FA - Autor: Juan
-- Usa foldr para acumular la valorizacion total.
valorizacionTotal :: [Pedido] -> Double
valorizacionTotal pedidos = foldr (+) 0 (valorizaciones pedidos)

-- FA - Autor: Juan
-- Pedidos entregados de una categoria de residuo.
entregadosDe :: String -> [Pedido] -> [Pedido]
entregadosDe categoria =
    filter (\p -> pTipoResiduo p == categoria && pEstado p == "entregado")

-- FA - Autor: Juan
-- Peso total entregado por categoria: (categoria, kg, cantidad de pedidos).
pesoPorCategoria :: [Pedido] -> [(String, Double, Int)]
pesoPorCategoria pedidos =
    map (\categoria ->
            let entregados = entregadosDe categoria pedidos
            in (categoria, sumarCargas entregados, length entregados))
        ["domiciliario", "reciclable", "desmonte"]


-- ============================================================================
-- 12. UTILIDADES DE MENU
-- ============================================================================

-- Pide un texto por consola. Si se cierra la entrada devuelve "0" para que
-- el programa pueda terminar.
pedirTexto :: String -> IO String
pedirTexto mensaje = do
    putStr mensaje
    hFlush stdout
    fin <- isEOF
    if fin
        then return "0"
        else fmap recortar getLine


-- Muestra el menu principal.
mostrarMenu :: IO ()
mostrarMenu = do
    putStrLn ""
    putStrLn "=========================================="
    putStrLn "  PLANIFICADOR DE RUTAS - RESIDUOS SURCO"
    putStrLn "=========================================="
    putStrLn "1. Ver datos cargados"
    putStrLn "2. Consultas de rutas"
    putStrLn "3. Reporte de pedidos"
    putStrLn "4. Resumen de un pedido"
    putStrLn "5. Funcionalidades adicionales"
    putStrLn "0. Salir"
    putStrLn "=========================================="

-- Muestra el submenu de consultas de rutas.
mostrarMenuConsultas :: IO ()
mostrarMenuConsultas = do
    putStrLn ""
    putStrLn "===================================="
    putStrLn "        CONSULTAS DE RUTAS"
    putStrLn "===================================="
    putStrLn "1. Vecinos de un punto"
    putStrLn "2. Rutas posibles entre 2 puntos"
    putStrLn "3. Costo de una ruta"
    putStrLn "0. Volver al menu principal"
    putStrLn "===================================="

-- Muestra el submenu de funcionalidades adicionales.
mostrarMenuAdicionales :: IO ()
mostrarMenuAdicionales = do
    putStrLn ""
    putStrLn "=========================================="
    putStrLn "        FUNCIONALIDADES ADICIONALES"
    putStrLn "=========================================="
    putStrLn " 1. Consultar mantenimiento de la flota (Jason)"
    putStrLn " 2. Consultar autonomia y combustible (Carlos)"
    putStrLn " 3. Consultar valorizacion de reciclaje (Juan)"
    putStrLn " 0. Volver al menu principal"
    putStrLn "=========================================="

-- Bucle generico de un submenu: muestra el menu, lee la opcion y ejecuta la
-- accion elegida hasta que el usuario escribe 0. Relee los CSV en cada vuelta.
submenu :: FilePath -> IO () -> (Datos -> String -> Maybe (IO ())) -> IO ()
submenu dir mostrar elegir = do
    datos <- cargarDatos dir
    mostrar
    opcion <- pedirTexto "Seleccione una opcion: "
    unless (opcion == "0") $ do
        case elegir datos opcion of
            Just operacion -> operacion
            Nothing        -> putStrLn "[ERROR] Opcion no valida."
        submenu dir mostrar elegir


-- ============================================================================
-- 13. OPCIONES DEL MENU
-- ============================================================================

-- [4.2.1 / 4.2.2] Muestra los datos leidos de los CSV.
opcionMostrarDatos :: Datos -> IO ()
opcionMostrarDatos datos = do
    putStrLn "\n--- DATOS CARGADOS DESDE LOS CSV ---"

    putStrLn ("\nConexiones: " ++ show (length (dConexiones datos)))
    putStrLn (columna 10 "ORIGEN" ++ columna 10 "DESTINO"
              ++ columna 12 "DIST. (KM)" ++ columna 10 "TIEMPO" ++ "ESTADO")
    mapM_ (\c -> putStrLn (columna 10 (cOrigen c) ++ columna 10 (cDestino c)
                           ++ columna 12 (formato2 (cDistancia c))
                           ++ columna 10 (show (cTiempo c)) ++ cEstado c))
          (dConexiones datos)

    putStrLn ("\nUnidades: " ++ show (length (dUnidades datos)))
    putStrLn (columna 8 "CODIGO" ++ columna 28 "TIPO" ++ columna 12 "CAP. (KG)"
              ++ columna 10 "S/ POR KM" ++ columna 12 "ESTADO" ++ "ESPECIALIDAD")
    mapM_ (\u -> putStrLn (columna 8 (uCodigo u) ++ columna 28 (uTipo u)
                           ++ columna 12 (mostrarNumero (uCapacidad u))
                           ++ columna 10 (formato2 (uCostoKm u))
                           ++ columna 12 (uEstado u) ++ uEspecialidad u))
          (dUnidades datos)

    putStrLn ("\nPedidos: " ++ show (length (dPedidos datos)))
    putStrLn (columna 8 "CODIGO" ++ columna 8 "ORIGEN" ++ columna 8 "DESTINO"
              ++ columna 11 "CARGA (KG)" ++ columna 11 "PRIORIDAD"
              ++ columna 12 "ESTADO" ++ columna 9 "UNIDAD" ++ "RESIDUO")
    mapM_ (\p -> putStrLn (columna 8 (pCodigo p) ++ columna 8 (pOrigen p)
                           ++ columna 8 (pDestino p)
                           ++ columna 11 (mostrarNumero (pCarga p))
                           ++ columna 11 (pPrioridad p) ++ columna 12 (pEstado p)
                           ++ columna 9 (if null (pUnidad p) then "-" else pUnidad p)
                           ++ pTipoResiduo p))
          (dPedidos datos)

-- [4.2.3] Pide un punto y muestra sus vecinos (verifica que exista en la red).
opcionVecinos :: Datos -> IO ()
opcionVecinos datos = do
    putStrLn "\n--- VECINOS DE UN PUNTO ---"
    punto <- pedirTexto "Codigo del punto: "
    let conexiones = dConexiones datos
        lista      = vecinos conexiones punto
    if not (existePuntoEnRed conexiones punto)
        then putStrLn "[ERROR] El punto no aparece en ninguna conexion."
        else if null lista
            then putStrLn ("El punto " ++ punto
                           ++ " no tiene vecinos por conexiones habilitadas.")
            else putStrLn ("Vecinos de " ++ punto ++": " ++ intercalate ", " lista)

-- [4.2.4] Pide origen y destino (los verifica) y lista las rutas posibles.
opcionRutas :: Datos -> IO ()
opcionRutas datos = do
    putStrLn "\n--- RUTAS POSIBLES ---"
    origen  <- pedirTexto "Punto de origen: "
    destino <- pedirTexto "Punto de destino: "
    let conexiones = dConexiones datos
    if not (existePuntoEnRed conexiones origen)
        then putStrLn ("[ERROR] El punto de origen " ++ origen
                       ++ " no existe en la red de conexiones.")
        else if not (existePuntoEnRed conexiones destino)
            then putStrLn ("[ERROR] El punto de destino " ++ destino
                           ++ " no existe en la red de conexiones.")
            else if origen == destino
                then putStrLn "[ERROR] El origen y el destino deben ser distintos."
                else do
                    let rutas = rutasPosibles conexiones origen destino
                    putStrLn ("Rutas posibles de " ++ origen ++ " a " ++ destino
                              ++ ": " ++ show (length rutas))
                    mapM_ (mostrarRuta conexiones) (zip [1 :: Int ..] rutas)
  where
    -- Muestra una ruta numerada con su distancia y tiempo (o el motivo de error).
    mostrarRuta :: [Conexion] -> (Int, Ruta) -> IO ()
    mostrarRuta conexiones (numero, ruta) =
        case costoRuta conexiones ruta of
            Right (km, mins) ->
                putStrLn ("  " ++ show numero ++ ") " ++ formatearRuta ruta
                          ++ " | " ++ formato2 km ++ " km | "
                          ++ show mins ++ " min")
            Left motivo ->
                putStrLn ("  " ++ show numero ++ ") " ++ formatearRuta ruta
                          ++ " | " ++ motivo)

-- [4.2.5] Pide una ruta al usuario y muestra su distancia y tiempo total.
opcionCosto :: Datos -> IO ()
opcionCosto datos = do
    putStrLn "\n--- COSTO DE UNA RUTA ---"
    texto <- pedirTexto "Ruta (separados por coma): "
    let ruta = words (map (\c -> if c == ',' then ' ' else c) texto)
    case costoRuta (dConexiones datos) ruta of
        Left motivo ->
            putStrLn ("[ERROR] " ++ motivo)
        Right (km, mins) -> do
            putStrLn ("Ruta: " ++ formatearRuta ruta)
            putStrLn ("Distancia total: " ++ formato2 km ++ " km")
            putStrLn ("Tiempo total: " ++ show mins ++ " min")

-- [4.2.6] Muestra el reporte de pedidos (por estado, por unidad y pendientes).
opcionReporte :: Datos -> IO ()
opcionReporte datos = do
    putStrLn "\n--- REPORTE DE PEDIDOS ---"

    putStrLn "\nPedidos por estado:"
    mapM_ (\(estado, cantidad) ->
              putStrLn ("  " ++ columna 12 estado ++ show cantidad))
          (contarPorEstado (dPedidos datos))

    putStrLn "\nCarga por unidad (kg):"
    putStrLn ("  " ++ columna 8 "UNIDAD" ++ columna 20 "ASIGNADA (EN CURSO)"
              ++ "ENTREGADA")
    mapM_ (\(unidad, asignada, entregada) ->
              putStrLn ("  " ++ columna 8 unidad
                        ++ columna 20 (mostrarNumero asignada)
                        ++ mostrarNumero entregada))
          (cargaPorUnidad (dUnidades datos) (dPedidos datos))

    putStrLn "\nPedidos pendientes por prioridad:"
    let pendientes = pendientesPorPrioridad (dPedidos datos)
    if null pendientes
        then putStrLn "  No hay pedidos pendientes."
        else mapM_ (\p -> putStrLn ("  " ++ pCodigo p ++ " (" ++ pPrioridad p
                                    ++ ") " ++ pOrigen p ++ " -> " ++ pDestino p
                                    ++ " | " ++ mostrarNumero (pCarga p) ++ " kg"))
                   pendientes

-- [4.2.7] Muestra el resumen de un pedido (o de todos los pendientes).
opcionResumen :: Datos -> IO ()
opcionResumen datos = do
    putStrLn "\n--- RESUMEN DE UN PEDIDO ---"
    codigo <- pedirTexto "Codigo del pedido (o TODOS los pendientes): "
    if minusculas codigo == "todos"
        then let pendientes = pendientesPorPrioridad (dPedidos datos)
             in if null pendientes
                    then putStrLn "No hay pedidos pendientes."
                    else mapM_ (\p -> do
                                    putStrLn ""
                                    mapM_ putStrLn (resumenPedido datos p))
                               pendientes
        else case find (\p -> pCodigo p == codigo) (dPedidos datos) of
            Nothing -> putStrLn "[ERROR] El pedido no existe."
            Just p  -> do
                putStrLn ""
                mapM_ putStrLn (resumenPedido datos p)

-- FA - Autor: Jason
-- Muestra el reporte de mantenimiento preventivo de la flota.
opcionMantenimiento :: Datos -> IO ()
opcionMantenimiento datos = do
    putStrLn "\n--- MANTENIMIENTO PREVENTIVO ---"
    let resultados = resultadosMantenimiento (dMantenimientos datos)
        pendientes = mantenimientosPendientes resultados
        (total, nPendientes, nAlDia) = resumenFlota resultados
    if null resultados
        then putStrLn "No hay registros en mantenimiento.csv."
        else do
            mapM_ (putStrLn . describirMantenimiento) resultados
            putStrLn ("\nUnidades con mantenimiento pendiente: "
                      ++ (if null pendientes
                              then "ninguna"
                              else intercalate ", " (map rmUnidad pendientes)))
            putStrLn ("Resumen de flota: " ++ show total ++ " camiones evaluados, "
                      ++ show nPendientes ++ " pendientes y "
                      ++ show nAlDia ++ " al dia.")

-- FA - Autor: Carlos
-- Muestra autonomia y combustible suficiente para una distancia ingresada.
opcionCombustible :: Datos -> IO ()
opcionCombustible datos = do
    putStrLn "\n--- COMBUSTIBLE Y AUTONOMIA ---"
    texto <- pedirTexto "Distancia del recorrido en km: "
    case leerDecimal texto of
        Nothing ->
            putStrLn "[ERROR] Ingrese un numero valido."
        Just distancia
            | distancia <= 0 ->
                putStrLn "[ERROR] La distancia debe ser mayor que cero."
            | null (dCombustibles datos) ->
                putStrLn "No hay registros en combustible.csv."
            | otherwise -> do
                let combustibles = dCombustibles datos
                    faltan = unidadesInsuficientes distancia combustibles
                mapM_ (\c -> putStrLn (cbUnidad c
                         ++ ": " ++ mostrarNumero (cbActual c) ++ " L"
                         ++ " | autonomia " ++ mostrarNumero (autonomia c) ++ " km"
                         ++ " | necesita " ++ mostrarNumero (litrosNecesarios distancia c)
                         ++ " L | faltan " ++ mostrarNumero (litrosFaltantes distancia c)
                         ++ " L"))
                      combustibles
                putStrLn ("\nUnidades sin combustible suficiente para "
                          ++ mostrarNumero distancia ++ " km: "
                          ++ (if null faltan
                                  then "ninguna"
                                  else intercalate ", " (map cbUnidad faltan)))
                putStrLn ("Combustible total de la flota: "
                          ++ mostrarNumero (combustibleTotal combustibles) ++ " L")

-- FA - Autor: Juan
-- Muestra la valorizacion del reciclaje entregado.
opcionReciclaje :: Datos -> IO ()
opcionReciclaje datos = do
    putStrLn "\n--- VALORIZACION DE RECICLAJE ---"
    putStrLn "Peso entregado por categoria:"
    mapM_ (\(categoria, kg, cantidad) ->
              putStrLn ("  " ++ columna 14 categoria ++ mostrarNumero kg
                        ++ " kg (" ++ show cantidad ++ " pedidos)"))
          (pesoPorCategoria (dPedidos datos))
    let reciclables = reciclablesEntregados (dPedidos datos)
    putStrLn ("\nTotal reciclable recolectado: "
              ++ mostrarNumero (sumarCargas reciclables) ++ " kg ("
              ++ show (length reciclables) ++ " pedidos)")
    putStrLn ("Valorizacion estimada: S/ "
              ++ formato2 (valorizacionTotal reciclables)
              ++ " (a S/ " ++ formato2 precioPorKgReciclable ++ " por kg)")


-- ============================================================================
-- 14. SUBMENUS Y PROGRAMA PRINCIPAL
-- ============================================================================

-- Asocia cada opcion del submenu de consultas con su accion.
eleccionConsultas :: Datos -> String -> Maybe (IO ())
eleccionConsultas datos opcion =
    case opcion of
        "1" -> Just (opcionVecinos datos)
        "2" -> Just (opcionRutas datos)
        "3" -> Just (opcionCosto datos)
        _   -> Nothing

-- Asocia cada opcion del submenu de funcionalidades adicionales con su accion.
eleccionAdicionales :: Datos -> String -> Maybe (IO ())
eleccionAdicionales datos opcion =
    case opcion of
        "1" -> Just (opcionMantenimiento datos)
        "2" -> Just (opcionCombustible datos)
        "3" -> Just (opcionReciclaje datos)
        _   -> Nothing

-- Ejecuta la opcion elegida en el menu principal. Devuelve False para salir.
ejecutarOpcion :: FilePath -> Datos -> String -> IO Bool
ejecutarOpcion dir datos opcion =
    case opcion of
        "0" -> do
            putStrLn "\nHasta pronto."
            return False
        "1" -> accion (opcionMostrarDatos datos)
        "2" -> accion (submenu dir mostrarMenuConsultas eleccionConsultas)
        "3" -> accion (opcionReporte datos)
        "4" -> accion (opcionResumen datos)
        "5" -> accion (submenu dir mostrarMenuAdicionales eleccionAdicionales)
        _   -> accion (putStrLn "[ERROR] Opcion no valida.")
  where
    -- Ejecuta una operacion y indica que el programa debe continuar.
    accion :: IO () -> IO Bool
    accion operacion = operacion >> return True

-- Bucle del menu principal. Los CSV se vuelven a leer en cada vuelta, asi que
-- los cambios hechos desde C++ se ven sin reiniciar el programa.
bucleMenu :: FilePath -> IO ()
bucleMenu dir = do
    datos <- cargarDatos dir
    mostrarMenu
    opcion <- pedirTexto "Seleccione una opcion: "
    continuar <- ejecutarOpcion dir datos opcion
    when continuar (bucleMenu dir)

-- Punto de entrada: ubica la carpeta data/, verifica los CSV e inicia el menu.
main :: IO ()
main = do
    hSetEncoding stdout utf8
    hSetEncoding stdin utf8
    dir <- buscarDirectorioData
    verificarArchivos dir
    putStrLn ("Datos cargados desde: " ++ dir)
    bucleMenu dir