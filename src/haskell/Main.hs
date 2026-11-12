module Main where

import Control.Exception (IOException, try)
import Data.Char (isSpace, toLower)
import Data.List (dropWhileEnd, intercalate, minimumBy, nub, sortBy)
import Data.Maybe (listToMaybe, mapMaybe)
import Data.Ord (comparing)
import Text.Printf (printf)

-- ============================================================================
-- Tipos de datos del sistema (compatibles con los CSV de Surco)
-- ============================================================================

data Conexion = Conexion
    { origen      :: String
    , destino     :: String
    , distanciaKm :: Double
    , tiempoMin   :: Int
    , estado      :: String
    } deriving (Show, Eq)

data Unidad = Unidad
    { codigoUnidad :: String
    , tipoUnidad   :: String
    , capacidadKg  :: Double
    , costoKm      :: Double
    , estadoUnidad :: String
    } deriving (Show, Eq)

data Pedido = Pedido
    { codigoPedido   :: String
    , origenPedido   :: String
    , destinoPedido  :: String
    , cargaKg        :: Double
    , prioridad      :: String
    , estadoPedido   :: String
    , unidadAsignada :: String
    } deriving (Show, Eq)

-- ============================================================================
-- Utilidades puras de texto y parsing CSV
-- ============================================================================

recortar :: String -> String
recortar = dropWhileEnd isSpace . dropWhile isSpace

normalizar :: String -> String
normalizar = map toLower . recortar

quitarBOM :: String -> String
quitarBOM ('\xFEFF' : resto) = resto
quitarBOM texto              = texto

-- Divide una linea CSV conservando campos vacios intermedios o finales.
dividirCSV :: String -> [String]
dividirCSV = map recortar . foldr paso [""]
  where
    paso ',' acc    = "" : acc
    paso '\r' acc   = acc
    paso c (x : xs) = (c : x) : xs
    paso c []       = [[c]]

lineasDeDatos :: String -> [String]
lineasDeDatos contenido =
    let sinCR   = filter (/= '\r') (quitarBOM contenido)
        limpias = filter (not . null . recortar) (lines sinCR)
    in case limpias of
        (_ : resto) -> resto
        []          -> []

leerDouble :: String -> Maybe Double
leerDouble texto = case reads (recortar texto) of
    [(valor, resto)] | all isSpace resto -> Just valor
    _                                    -> Nothing

leerInt :: String -> Maybe Int
leerInt texto = case reads (recortar texto) of
    [(valor, resto)] | all isSpace resto -> Just valor
    _                                    -> Nothing

-- ============================================================================
-- [4.2.1 y 4.2.2 - Carlos] Parsing de CSV a listas de tipos propios
-- (Implementacion base compatible con columnas adicionales de data/*.csv)
-- ============================================================================

parsearLineaConexion :: String -> Maybe Conexion
parsearLineaConexion linea = case dividirCSV linea of
    (orig : dest : distStr : tiempoStr : est : _) -> do
        dist   <- leerDouble distStr
        tiempo <- leerInt tiempoStr
        return (Conexion orig dest dist tiempo est)
    _ -> Nothing

parsearLineaUnidad :: String -> Maybe Unidad
parsearLineaUnidad linea = case dividirCSV linea of
    (cod : tipo : capStr : costoStr : est : _) -> do
        cap   <- leerDouble capStr
        costo <- leerDouble costoStr
        return (Unidad cod tipo cap costo est)
    _ -> Nothing

parsearLineaPedido :: String -> Maybe Pedido
parsearLineaPedido linea = case dividirCSV linea of
    (cod : orig : dest : cargaStr : prio : est : unid : _) -> do
        carga <- leerDouble cargaStr
        return (Pedido cod orig dest carga prio est unid)
    _ -> Nothing

parsearConexiones :: String -> [Conexion]
parsearConexiones = mapMaybe parsearLineaConexion . lineasDeDatos

parsearUnidades :: String -> [Unidad]
parsearUnidades = mapMaybe parsearLineaUnidad . lineasDeDatos

parsearPedidos :: String -> [Pedido]
parsearPedidos = mapMaybe parsearLineaPedido . lineasDeDatos

-- ============================================================================
-- [4.2.3, 4.2.4 y 4.2.5 - Jason] Grafo de conexiones, rutas y costos
-- (Implementacion base pura para integracion)
-- ============================================================================

esHabilitada :: Conexion -> Bool
esHabilitada c = normalizar (estado c) == "habilitada"

-- [4.2.3 - Jason] Vecinos alcanzables mediante conexiones habilitadas (doble sentido)
vecinos :: String -> [Conexion] -> [String]
vecinos nodo conexiones =
    nub [ otro
        | c <- conexiones
        , esHabilitada c
        , Just otro <- [extremoOpuesto nodo c]
        , otro /= nodo
        ]
  where
    extremoOpuesto actual c
        | origen c == actual  = Just (destino c)
        | destino c == actual = Just (origen c)
        | otherwise           = Nothing

-- [4.2.4 - Jason] Busqueda recursiva de todas las rutas simples sin repetir nodos
rutasPosibles :: String -> String -> [Conexion] -> [[String]]
rutasPosibles inicio objetivo conexiones = explorar inicio [inicio]
  where
    explorar actual visitados
        | actual == objetivo = [reverse visitados]
        | otherwise =
            concatMap
                (\sig -> explorar sig (sig : visitados))
                [ sig
                | sig <- vecinos actual conexiones
                , sig `notElem` visitados
                ]

-- [4.2.5 - Jason] Calculo de distancia total y tiempo total de una ruta
costoRuta :: [String] -> [Conexion] -> Either String (Double, Int)
costoRuta [] _          = Left "La ruta esta vacia."
costoRuta [_] _         = Right (0.0, 0)
costoRuta (a : b : xs) conexiones = do
    tramo <- buscarTramoHabilitado a b conexiones
    (distResto, tiempoResto) <- costoRuta (b : xs) conexiones
    return (distanciaKm tramo + distResto, tiempoMin tramo + tiempoResto)
  where
    buscarTramoHabilitado u v cs =
        let coinciden =
                [ c
                | c <- cs
                , (origen c == u && destino c == v)
                    || (origen c == v && destino c == u)
                ]
        in case coinciden of
            [] -> Left ("No existe conexion entre " ++ u ++ " y " ++ v ++ ".")
            _  -> case filter esHabilitada coinciden of
                    (activa : _) -> Right activa
                    []           -> Left ("La conexion entre " ++ u ++ " y " ++ v ++ " esta cerrada.")

-- ============================================================================
-- [4.2.6 - Carlos] Pedidos pendientes ordenados por prioridad
-- ============================================================================

pedidosPendientesOrdenados :: [Pedido] -> [Pedido]
pedidosPendientesOrdenados =
    sortBy (comparing (rangoPrioridad . prioridad))
        . filter (\p -> normalizar (estadoPedido p) == "pendiente")
  where
    rangoPrioridad prio = case normalizar prio of
        "alta"  -> 1 :: Int
        "media" -> 2
        "baja"  -> 3
        _       -> 4

-- ============================================================================
-- [4.2.7 - Juan David] Logica pura de resumen y orquestacion IO
-- ============================================================================

seleccionarPedidoEjemplo :: [Pedido] -> Maybe Pedido
seleccionarPedidoEjemplo pedidos =
    case pedidosPendientesOrdenados pedidos of
        (primeroPendiente : _) -> Just primeroPendiente
        []                     -> listToMaybe pedidos

seleccionarUnidadEjemplo :: Pedido -> [Unidad] -> Maybe Unidad
seleccionarUnidadEjemplo pedido unidades =
    case buscarPorCodigo (unidadAsignada pedido) of
        Just u  -> Just u
        Nothing -> case buscarPorCodigo "U01" of
            Just u01 -> Just u01
            Nothing  -> listToMaybe unidades
  where
    buscarPorCodigo cod
        | null cod  = Nothing
        | otherwise = listToMaybe [u | u <- unidades, codigoUnidad u == cod]

mejorRutaPorDistancia :: [[String]] -> [Conexion] -> Maybe ([String], (Double, Int))
mejorRutaPorDistancia rutas conexiones =
    case mapMaybe evaluar rutas of
        []         -> Nothing
        evaluadas  -> Just (minimumBy (comparing (fst . snd)) evaluadas)
  where
    evaluar r = case costoRuta r conexiones of
        Right metricas -> Just (r, metricas)
        Left _         -> Nothing

formatearPendientes :: [Pedido] -> String
formatearPendientes [] = "Ninguno"
formatearPendientes ps =
    intercalate ", " [codigoPedido p ++ " (" ++ prioridad p ++ ")" | p <- ps]

generarReporteConsola :: [Conexion] -> [Unidad] -> [Pedido] -> Either String String
generarReporteConsola conexiones unidades pedidos = do
    pedido <- maybe (Left "No se encontraron pedidos en pedidos.csv.") Right (seleccionarPedidoEjemplo pedidos)
    unidad <- maybe (Left "No se encontraron unidades en unidades.csv.") Right (seleccionarUnidadEjemplo pedido unidades)
    let rutas      = rutasPosibles (origenPedido pedido) (destinoPedido pedido) conexiones
        pendientes = pedidosPendientesOrdenados pedidos
        linea1 = "Pedido: " ++ codigoPedido pedido
            ++ " (" ++ origenPedido pedido ++ " -> " ++ destinoPedido pedido ++ ")"
        linea2 = "Rutas posibles: " ++ show (length rutas)
        linea3 = case mejorRutaPorDistancia rutas conexiones of
            Nothing ->
                "Costo de ruta: No hay rutas habilitadas entre "
                    ++ origenPedido pedido ++ " y " ++ destinoPedido pedido
            Just (rutaOptima, (distTotal, _)) ->
                let costoTotal :: Double
                    costoTotal = distTotal * costoKm unidad
                in "Costo de ruta " ++ intercalate " -> " rutaOptima
                    ++ " (" ++ tipoUnidad unidad ++ "): S/ "
                    ++ printf "%.2f" costoTotal
        linea4 = "Pedidos pendientes por prioridad: " ++ formatearPendientes pendientes
    return (unlines [linea1, linea2, linea3, linea4])

-- Resuelve rutas relativas de data/ de manera portable (Windows/Linux/macOS).
leerCSVConFallback :: FilePath -> IO (Either String String)
leerCSVConFallback nombreArchivo = intentarRutas ["data/", "../../data/", "../data/", ""]
  where
    intentarRutas [] =
        return (Left ("No se encontro el archivo " ++ nombreArchivo ++ " en data/, ../../data/, ../data/ ni en el directorio actual."))
    intentarRutas (prefijo : resto) = do
        let ruta = prefijo ++ nombreArchivo
        resultado <- try (leerEstricto ruta) :: IO (Either IOException String)
        case resultado of
            Right contenido -> return (Right contenido)
            Left _          -> intentarRutas resto

    leerEstricto ruta = do
        contenido <- readFile ruta
        length contenido `seq` return contenido

main :: IO ()
main = do
    resConexiones <- leerCSVConFallback "conexiones.csv"
    resUnidades   <- leerCSVConFallback "unidades.csv"
    resPedidos    <- leerCSVConFallback "pedidos.csv"

    case (resConexiones, resUnidades, resPedidos) of
        (Right txtCon, Right txtUni, Right txtPed) ->
            let conexiones = parsearConexiones txtCon
                unidades   = parsearUnidades txtUni
                pedidos    = parsearPedidos txtPed
            in case generarReporteConsola conexiones unidades pedidos of
                Right salida -> putStr salida
                Left err     -> putStrLn ("Error al generar reporte: " ++ err)
        (Left e, _, _) -> putStrLn ("Error de lectura: " ++ e)
        (_, Left e, _) -> putStrLn ("Error de lectura: " ++ e)
        (_, _, Left e) -> putStrLn ("Error de lectura: " ++ e)
