module Main where

import Data.Char (isSpace, toLower)
import Data.List (find, intercalate, isSuffixOf, minimumBy, nub, sortOn)
import Data.Maybe (mapMaybe)
import Data.Ord (comparing)
import System.Directory (doesFileExist)
import System.IO (hFlush, isEOF, stdout)
import Text.Printf (printf)
import Text.Read (readMaybe)

-- ============================================================================
-- Tipos de datos requeridos (compatibles con los CSV generados por C++)
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
-- Resolucion segura de rutas hacia data/*.csv
-- ============================================================================

normalizarNombreCSV :: String -> String
normalizarNombreCSV nombre
  | ".csv" `isSuffixOf` map toLower nombre = nombre
  | otherwise = nombre ++ ".csv"

resolverRutaCSV :: String -> IO FilePath
resolverRutaCSV nombreArchivo = buscarPrimero candidatos
  where
    archivo = normalizarNombreCSV nombreArchivo
    candidatos =
      [ "data/" ++ archivo
      , "../data/" ++ archivo
      , "./" ++ archivo
      ]
    buscarPrimero [] = return ("data/" ++ archivo)
    buscarPrimero (ruta : resto) = do
      existe <- doesFileExist ruta
      if existe
        then return ruta
        else buscarPrimero resto

-- ============================================================================
-- [4.2.1 y 4.2.2 - Carlos] Parsing limpio de CSV a tipos propios
-- ============================================================================

recortar :: String -> String
recortar = reverse . dropWhile isSpace . reverse . dropWhile isSpace

aMinusculas :: String -> String
aMinusculas = map toLower

quitarBOM :: String -> String
quitarBOM ('\xFEFF' : xs) = xs
quitarBOM xs = xs

dividirCSV :: String -> [String]
dividirCSV s = case break (== ',') s of
  (campo, [])     -> [recortar campo]
  (campo, _ : xs) -> recortar campo : dividirCSV xs

lineasDeDatos :: String -> [String]
lineasDeDatos contenido =
  drop 1 . filter (not . null) . map (recortar . quitarBOM) . lines $ contenido

parsearConexion :: String -> Maybe Conexion
parsearConexion linea =
  case dividirCSV linea of
    (orig : dest : distStr : tiempoStr : est : _) -> do
      dist <- readMaybe distStr
      tiempo <- readMaybe tiempoStr
      return (Conexion orig dest dist tiempo est)
    _ -> Nothing

parsearUnidad :: String -> Maybe Unidad
parsearUnidad linea =
  case dividirCSV linea of
    -- Tolera columnas adicionales al final (ej. especialidad)
    (cod : tipo : capStr : costoStr : est : _) -> do
      cap <- readMaybe capStr
      costo <- readMaybe costoStr
      return (Unidad cod tipo cap costo est)
    _ -> Nothing

parsearPedido :: String -> Maybe Pedido
parsearPedido linea =
  case dividirCSV linea of
    -- Maneja unidad_asignada vacia y tolera columnas adicionales (ej. tipo_residuo)
    (cod : orig : dest : cargaStr : prio : est : unidad : _) -> do
      carga <- readMaybe cargaStr
      return (Pedido cod orig dest carga prio est unidad)
    _ -> Nothing

parsearConexiones :: String -> [Conexion]
parsearConexiones = mapMaybe parsearConexion . lineasDeDatos

parsearUnidades :: String -> [Unidad]
parsearUnidades = mapMaybe parsearUnidad . lineasDeDatos

parsearPedidos :: String -> [Pedido]
parsearPedidos = mapMaybe parsearPedido . lineasDeDatos

-- ============================================================================
-- [4.2.3, 4.2.4 y 4.2.5 - Jason] Grafo de conexiones, rutas y costos
-- ============================================================================

esHabilitada :: Conexion -> Bool
esHabilitada c = aMinusculas (estado c) == "habilitada"

vecinos :: [Conexion] -> String -> [String]
vecinos conexiones punto =
  nub
    [ if aMinusculas (origen c) == p then destino c else origen c
    | c <- conexiones
    , esHabilitada c
    , aMinusculas (origen c) == p || aMinusculas (destino c) == p
    ]
  where
    p = aMinusculas punto

rutasPosibles :: [Conexion] -> String -> String -> [[String]]
rutasPosibles conexiones inicio fin = buscar [inicio] inicio
  where
    finNorm = aMinusculas fin
    buscar visitados actual
      | aMinusculas actual == finNorm = [reverse visitados]
      | otherwise =
          [ ruta
          | siguiente <- vecinos conexiones actual
          , aMinusculas siguiente `notElem` map aMinusculas visitados
          , ruta <- buscar (siguiente : visitados) siguiente
          ]

buscarTramo :: [Conexion] -> String -> String -> Either String (Double, Int)
buscarTramo conexiones a b =
  case coincidencias of
    [] -> Left ("No existe conexion entre " ++ a ++ " y " ++ b)
    cs -> case filter esHabilitada cs of
      [] -> Left ("El tramo entre " ++ a ++ " y " ++ b ++ " esta cerrado")
      habilitadas ->
        let mejor = minimumBy (comparing distanciaKm) habilitadas
         in Right (distanciaKm mejor, tiempoMin mejor)
  where
    aNorm = aMinusculas a
    bNorm = aMinusculas b
    coincidencias =
      [ c
      | c <- conexiones
      , (aMinusculas (origen c) == aNorm && aMinusculas (destino c) == bNorm)
          || (aMinusculas (origen c) == bNorm && aMinusculas (destino c) == aNorm)
      ]

costoRuta :: [Conexion] -> [String] -> Either String (Double, Int)
costoRuta _ [] = Left "La ruta esta vacia"
costoRuta _ [_] = Right (0.0, 0)
costoRuta conexiones (a : b : resto) = do
  (d1, t1) <- buscarTramo conexiones a b
  (dResto, tResto) <- costoRuta conexiones (b : resto)
  Right (d1 + dResto, t1 + tResto)

-- ============================================================================
-- [4.2.6 - Carlos] Pedidos pendientes ordenados por prioridad
-- ============================================================================

valorPrioridad :: String -> Int
valorPrioridad p =
  case aMinusculas (recortar p) of
    "alta"  -> 0
    "media" -> 1
    "baja"  -> 2
    _       -> 3

pedidosPendientesOrdenados :: [Pedido] -> [Pedido]
pedidosPendientesOrdenados =
  sortOn (valorPrioridad . prioridad)
    . filter (\p -> aMinusculas (recortar (estadoPedido p)) == "pendiente")

-- ============================================================================
-- [4.2.7 - Juan David Morales Rabelly] Orquestacion main y formateo de salida
-- ============================================================================

evaluarRutas :: [Conexion] -> [[String]] -> [([String], (Double, Int))]
evaluarRutas conexiones =
  mapMaybe (\r -> case costoRuta conexiones r of
                    Right ct -> Just (r, ct)
                    Left _   -> Nothing)

seleccionarRutaOptima :: [([String], (Double, Int))] -> Maybe ([String], (Double, Int))
seleccionarRutaOptima [] = Nothing
seleccionarRutaOptima xs = Just (minimumBy (comparing snd) xs)

formatearListaPendientes :: [Pedido] -> String
formatearListaPendientes =
  intercalate ", " . map (\p -> codigoPedido p ++ " (" ++ prioridad p ++ ")")

formatearListaUnidades :: [Unidad] -> String
formatearListaUnidades =
  intercalate ", " . map (\u -> codigoUnidad u ++ " (" ++ tipoUnidad u ++ ")")

leerEntrada :: IO String
leerEntrada = do
  fin <- isEOF
  if fin
    then return ""
    else recortar <$> getLine

seleccionarPedido :: Pedido -> [Pedido] -> IO Pedido
seleccionarPedido defecto pendientes = loop
  where
    loop = do
      putStrLn ("Pedidos pendientes: " ++ formatearListaPendientes pendientes)
      putStr ("Seleccione codigo de pedido [Enter para " ++ codigoPedido defecto ++ "]: ")
      hFlush stdout
      entrada <- leerEntrada
      if null entrada
        then return defecto
        else case find (\p -> aMinusculas (codigoPedido p) == aMinusculas entrada) pendientes of
          Just elegido -> return elegido
          Nothing -> do
            putStrLn ("Codigo '" ++ entrada ++ "' no encontrado entre pedidos pendientes. Intente nuevamente.")
            loop

seleccionarUnidad :: Unidad -> [Unidad] -> IO Unidad
seleccionarUnidad defecto disponibles = loop
  where
    loop = do
      putStrLn ("Unidades disponibles: " ++ formatearListaUnidades disponibles)
      putStr ("Seleccione codigo de unidad [Enter para " ++ codigoUnidad defecto ++ "]: ")
      hFlush stdout
      entrada <- leerEntrada
      if null entrada
        then return defecto
        else case find (\u -> aMinusculas (codigoUnidad u) == aMinusculas entrada) disponibles of
          Just elegida -> return elegida
          Nothing -> do
            putStrLn ("Codigo '" ++ entrada ++ "' no encontrado entre unidades disponibles. Intente nuevamente.")
            loop

cargarDatosSiExisten :: IO (Maybe ([Conexion], [Unidad], [Pedido]))
cargarDatosSiExisten = do
  rutaConexiones <- resolverRutaCSV "conexiones.csv"
  rutaUnidades <- resolverRutaCSV "unidades.csv"
  rutaPedidos <- resolverRutaCSV "pedidos.csv"

  okCon <- doesFileExist rutaConexiones
  okUni <- doesFileExist rutaUnidades
  okPed <- doesFileExist rutaPedidos

  if not (okCon && okUni && okPed)
    then do
      putStrLn "[ERROR] No se encontraron todos los archivos CSV requeridos en data/."
      return Nothing
    else do
      rawCon <- readFile rutaConexiones
      rawUni <- readFile rutaUnidades
      rawPed <- readFile rutaPedidos
      return (Just (parsearConexiones rawCon, parsearUnidades rawUni, parsearPedidos rawPed))

main :: IO ()
main = do
  datos <- cargarDatosSiExisten
  case datos of
    Nothing -> return ()
    Just (conexiones, unidades, pedidos) -> do
      let pendientes = pedidosPendientesOrdenados pedidos
          disponibles = filter (\u -> aMinusculas (recortar (estadoUnidad u)) == "disponible") unidades

      case (pendientes, disponibles) of
        ([], _) -> putStrLn "No hay pedidos pendientes registrados."
        (_, []) -> putStrLn "No hay unidades disponibles registradas."
        (pedidoDefecto : _, unidadDefecto : _) -> do
          pedidoSel <- seleccionarPedido pedidoDefecto pendientes
          unidadSel <- seleccionarUnidad unidadDefecto disponibles

          let rutas = rutasPosibles conexiones (origenPedido pedidoSel) (destinoPedido pedidoSel)
              rutasValidas = evaluarRutas conexiones rutas

          putStrLn ""
          putStrLn $
            "Pedido: "
              ++ codigoPedido pedidoSel
              ++ " ("
              ++ origenPedido pedidoSel
              ++ " -> "
              ++ destinoPedido pedidoSel
              ++ ")"
          putStrLn $ "Rutas posibles: " ++ show (length rutasValidas)

          case seleccionarRutaOptima rutasValidas of
            Nothing ->
              putStrLn $
                "No existe ruta habilitada entre "
                  ++ origenPedido pedidoSel
                  ++ " y "
                  ++ destinoPedido pedidoSel
                  ++ "."
            Just (rutaOptima, (distTotal, _tiempoTotal)) -> do
              let costoTotal = distTotal * costoKm unidadSel
                  rutaTexto = intercalate " -> " rutaOptima
              putStrLn $
                "Costo de ruta "
                  ++ rutaTexto
                  ++ " ("
                  ++ tipoUnidad unidadSel
                  ++ "): S/ "
                  ++ printf "%.2f" costoTotal

          putStrLn $ "Pedidos pendientes por prioridad: " ++ formatearListaPendientes pendientes
