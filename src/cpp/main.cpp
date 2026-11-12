#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// Estructuras de datos base del sistema (Recoleccion de Residuos - Surco)
// ============================================================================

struct Punto {
    std::string codigo;
    std::string nombre;
    std::string tipo_punto;
    std::string zona;
    int dias_sin_recolectar = 0; // Columna opcional compatible con data/puntos.csv
};

struct Conexion {
    std::string origen;
    std::string destino;
    double distancia_km = 0.0;
    int tiempo_min = 0;
    std::string estado;
};

struct Unidad {
    std::string codigo;
    std::string tipo;
    double capacidad_kg = 0.0;
    double costo_km = 0.0;
    std::string estado;
    std::string especialidad; // Columna opcional compatible con data/unidades.csv
};

struct Pedido {
    std::string codigo;
    std::string origen;
    std::string destino;
    double carga_kg = 0.0;
    std::string prioridad;
    std::string estado;
    std::string unidad_asignada;
    std::string tipo_residuo; // Columna opcional compatible con data/pedidos.csv
};

// Conserva el formato de columnas adicionales si los CSV de entrada las incluyen.
struct EsquemaCSV {
    bool puntosTieneDias = false;
    bool unidadesTieneEspecialidad = false;
    bool pedidosTieneTipoResiduo = false;
};

// ============================================================================
// Utilidades de texto, parsing CSV y resolucion de rutas de archivos
// ============================================================================

std::string limpiarTexto(const std::string& texto) {
    size_t inicio = 0;
    while (inicio < texto.size() &&
           std::isspace(static_cast<unsigned char>(texto[inicio]))) {
        ++inicio;
    }
    size_t fin = texto.size();
    while (fin > inicio &&
           std::isspace(static_cast<unsigned char>(texto[fin - 1]))) {
        --fin;
    }
    return texto.substr(inicio, fin - inicio);
}

void quitarBOM(std::string& linea) {
    if (linea.size() >= 3 &&
        static_cast<unsigned char>(linea[0]) == 0xEF &&
        static_cast<unsigned char>(linea[1]) == 0xBB &&
        static_cast<unsigned char>(linea[2]) == 0xBF) {
        linea.erase(0, 3);
    }
}

// Divide una linea CSV respetando campos vacios intermedios y al final (ej. "a,b,").
std::vector<std::string> dividirCSV(const std::string& linea) {
    std::vector<std::string> campos;
    std::string actual;
    for (char c : linea) {
        if (c == ',') {
            campos.push_back(limpiarTexto(actual));
            actual.clear();
        } else if (c != '\r') {
            actual.push_back(c);
        }
    }
    campos.push_back(limpiarTexto(actual));
    return campos;
}

bool parsearDouble(const std::string& texto, double& resultado) {
    if (texto.empty()) {
        return false;
    }
    try {
        size_t procesados = 0;
        double valor = std::stod(texto, &procesados);
        if (procesados != texto.size()) {
            return false;
        }
        resultado = valor;
        return true;
    } catch (...) {
        return false;
    }
}

bool parsearInt(const std::string& texto, int& resultado) {
    if (texto.empty()) {
        return false;
    }
    try {
        size_t procesados = 0;
        int valor = std::stoi(texto, &procesados);
        if (procesados != texto.size()) {
            return false;
        }
        resultado = valor;
        return true;
    } catch (...) {
        return false;
    }
}

bool archivoExiste(const std::string& ruta) {
    std::ifstream prueba(ruta);
    return prueba.good();
}

// Permite ejecutar el binario desde la raiz del repo, src/cpp/, build/ o una carpeta aislada.
std::string detectarDirectorioDatos() {
    const std::vector<std::string> prefijos = {"data/", "../../data/", "../data/", ""};
    const std::vector<std::string> archivos = {
        "puntos.csv", "conexiones.csv", "unidades.csv", "pedidos.csv"
    };

    for (const std::string& prefijo : prefijos) {
        for (const std::string& nombre : archivos) {
            if (archivoExiste(prefijo + nombre)) {
                return prefijo;
            }
        }
    }
    return "";
}

bool leerEnteroSeguro(const std::string& mensaje, int& valor) {
    std::cout << mensaje;
    std::string linea;
    if (!std::getline(std::cin, linea)) {
        return false;
    }
    return parsearInt(limpiarTexto(linea), valor);
}

// ============================================================================
// [4.1.2] Carga inicial de archivos CSV
// ============================================================================

bool abrirOCrearCSV(const std::string& ruta,
                    const std::string& cabeceraPorDefecto,
                    std::ifstream& entrada) {
    entrada.open(ruta);
    if (entrada.is_open()) {
        return true;
    }

    std::ofstream nuevo(ruta);
    if (!nuevo.is_open()) {
        std::cerr << "[Error] No se pudo crear el archivo: " << ruta << "\n";
        return false;
    }
    nuevo << cabeceraPorDefecto << "\n";
    return false;
}

void cargarPuntos(const std::string& ruta,
                  std::vector<Punto>& puntos,
                  bool& tieneExtra) {
    puntos.clear();
    std::ifstream archivo;
    if (!abrirOCrearCSV(ruta, "codigo,nombre,tipo_punto,zona", archivo)) {
        return;
    }

    std::string linea;
    if (!std::getline(archivo, linea)) {
        return;
    }
    quitarBOM(linea);
    if (dividirCSV(linea).size() >= 5) {
        tieneExtra = true;
    }

    int numeroLinea = 1;
    while (std::getline(archivo, linea)) {
        ++numeroLinea;
        if (limpiarTexto(linea).empty()) {
            continue;
        }
        std::vector<std::string> cols = dividirCSV(linea);
        if (cols.size() < 4) {
            std::cerr << "[Aviso] Linea invalida en " << ruta
                      << " (" << numeroLinea << "): columnas insuficientes.\n";
            continue;
        }
        Punto p;
        p.codigo = cols[0];
        p.nombre = cols[1];
        p.tipo_punto = cols[2];
        p.zona = cols[3];
        if (cols.size() >= 5) {
            tieneExtra = true;
            int dias = 0;
            if (!cols[4].empty() && parsearInt(cols[4], dias)) {
                p.dias_sin_recolectar = dias;
            }
        }
        puntos.push_back(p);
    }
}

void cargarConexiones(const std::string& ruta,
                      std::vector<Conexion>& conexiones) {
    conexiones.clear();
    std::ifstream archivo;
    if (!abrirOCrearCSV(ruta, "origen,destino,distancia_km,tiempo_min,estado", archivo)) {
        return;
    }

    std::string linea;
    if (!std::getline(archivo, linea)) {
        return;
    }

    int numeroLinea = 1;
    while (std::getline(archivo, linea)) {
        ++numeroLinea;
        if (limpiarTexto(linea).empty()) {
            continue;
        }
        std::vector<std::string> cols = dividirCSV(linea);
        if (cols.size() < 5) {
            std::cerr << "[Aviso] Linea invalida en " << ruta
                      << " (" << numeroLinea << "): columnas insuficientes.\n";
            continue;
        }
        Conexion c;
        c.origen = cols[0];
        c.destino = cols[1];
        if (!parsearDouble(cols[2], c.distancia_km) ||
            !parsearInt(cols[3], c.tiempo_min)) {
            std::cerr << "[Aviso] Datos numericos invalidos en " << ruta
                      << " (" << numeroLinea << ").\n";
            continue;
        }
        c.estado = cols[4];
        conexiones.push_back(c);
    }
}

void cargarUnidades(const std::string& ruta,
                    std::vector<Unidad>& unidades,
                    bool& tieneExtra) {
    unidades.clear();
    std::ifstream archivo;
    if (!abrirOCrearCSV(ruta, "codigo,tipo,capacidad_kg,costo_km,estado", archivo)) {
        return;
    }

    std::string linea;
    if (!std::getline(archivo, linea)) {
        return;
    }
    quitarBOM(linea);
    if (dividirCSV(linea).size() >= 6) {
        tieneExtra = true;
    }

    int numeroLinea = 1;
    while (std::getline(archivo, linea)) {
        ++numeroLinea;
        if (limpiarTexto(linea).empty()) {
            continue;
        }
        std::vector<std::string> cols = dividirCSV(linea);
        if (cols.size() < 5) {
            std::cerr << "[Aviso] Linea invalida en " << ruta
                      << " (" << numeroLinea << "): columnas insuficientes.\n";
            continue;
        }
        Unidad u;
        u.codigo = cols[0];
        u.tipo = cols[1];
        if (!parsearDouble(cols[2], u.capacidad_kg) ||
            !parsearDouble(cols[3], u.costo_km)) {
            std::cerr << "[Aviso] Datos numericos invalidos en " << ruta
                      << " (" << numeroLinea << ").\n";
            continue;
        }
        u.estado = cols[4];
        if (cols.size() >= 6) {
            tieneExtra = true;
            u.especialidad = cols[5];
        }
        unidades.push_back(u);
    }
}

void cargarPedidos(const std::string& ruta,
                   std::vector<Pedido>& pedidos,
                   bool& tieneExtra) {
    pedidos.clear();
    std::ifstream archivo;
    if (!abrirOCrearCSV(
            ruta,
            "codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada",
            archivo)) {
        return;
    }

    std::string linea;
    if (!std::getline(archivo, linea)) {
        return;
    }
    quitarBOM(linea);
    if (dividirCSV(linea).size() >= 8) {
        tieneExtra = true;
    }

    int numeroLinea = 1;
    while (std::getline(archivo, linea)) {
        ++numeroLinea;
        if (limpiarTexto(linea).empty()) {
            continue;
        }
        std::vector<std::string> cols = dividirCSV(linea);
        if (cols.size() < 7) {
            std::cerr << "[Aviso] Linea invalida en " << ruta
                      << " (" << numeroLinea << "): columnas insuficientes.\n";
            continue;
        }
        Pedido p;
        p.codigo = cols[0];
        p.origen = cols[1];
        p.destino = cols[2];
        if (!parsearDouble(cols[3], p.carga_kg)) {
            std::cerr << "[Aviso] Carga invalida en " << ruta
                      << " (" << numeroLinea << ").\n";
            continue;
        }
        p.prioridad = cols[4];
        p.estado = cols[5];
        p.unidad_asignada = cols[6];
        if (cols.size() >= 8) {
            tieneExtra = true;
            p.tipo_residuo = cols[7];
        }
        pedidos.push_back(p);
    }
}

void cargarTodo(const std::string& directorioData,
                std::vector<Punto>& puntos,
                std::vector<Conexion>& conexiones,
                std::vector<Unidad>& unidades,
                std::vector<Pedido>& pedidos,
                EsquemaCSV& esquema) {
    cargarPuntos(directorioData + "puntos.csv", puntos, esquema.puntosTieneDias);
    cargarConexiones(directorioData + "conexiones.csv", conexiones);
    cargarUnidades(directorioData + "unidades.csv", unidades, esquema.unidadesTieneEspecialidad);
    cargarPedidos(directorioData + "pedidos.csv", pedidos, esquema.pedidosTieneTipoResiduo);
}

// ============================================================================
// [4.1.5] Generacion y persistencia de archivos CSV
// ============================================================================

bool guardarPuntos(const std::string& ruta,
                   const std::vector<Punto>& puntos,
                   bool incluirExtra = false) {
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[Error] No se pudo guardar " << ruta << "\n";
        return false;
    }
    archivo << (incluirExtra
                    ? "codigo,nombre,tipo_punto,zona,dias_sin_recolectar\n"
                    : "codigo,nombre,tipo_punto,zona\n");
    for (const Punto& p : puntos) {
        archivo << p.codigo << ","
                << p.nombre << ","
                << p.tipo_punto << ","
                << p.zona;
        if (incluirExtra) {
            archivo << "," << p.dias_sin_recolectar;
        }
        archivo << "\n";
    }
    return archivo.good();
}

bool guardarConexiones(const std::string& ruta,
                       const std::vector<Conexion>& conexiones) {
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[Error] No se pudo guardar " << ruta << "\n";
        return false;
    }
    archivo << "origen,destino,distancia_km,tiempo_min,estado\n";
    for (const Conexion& c : conexiones) {
        archivo << c.origen << ","
                << c.destino << ","
                << c.distancia_km << ","
                << c.tiempo_min << ","
                << c.estado << "\n";
    }
    return archivo.good();
}

bool guardarUnidades(const std::string& ruta,
                     const std::vector<Unidad>& unidades,
                     bool incluirExtra = false) {
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[Error] No se pudo guardar " << ruta << "\n";
        return false;
    }
    archivo << (incluirExtra
                    ? "codigo,tipo,capacidad_kg,costo_km,estado,especialidad\n"
                    : "codigo,tipo,capacidad_kg,costo_km,estado\n");
    for (const Unidad& u : unidades) {
        std::ostringstream costoFmt;
        costoFmt << std::fixed << std::setprecision(2) << u.costo_km;
        archivo << u.codigo << ","
                << u.tipo << ","
                << u.capacidad_kg << ","
                << costoFmt.str() << ","
                << u.estado;
        if (incluirExtra) {
            archivo << "," << u.especialidad;
        }
        archivo << "\n";
    }
    return archivo.good();
}

bool guardarPedidos(const std::string& ruta,
                    const std::vector<Pedido>& pedidos,
                    bool incluirExtra = false) {
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[Error] No se pudo guardar " << ruta << "\n";
        return false;
    }
    archivo << (incluirExtra
                    ? "codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada,tipo_residuo\n"
                    : "codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada\n");
    for (const Pedido& p : pedidos) {
        archivo << p.codigo << ","
                << p.origen << ","
                << p.destino << ","
                << p.carga_kg << ","
                << p.prioridad << ","
                << p.estado << ","
                << p.unidad_asignada;
        if (incluirExtra) {
            archivo << "," << p.tipo_residuo;
        }
        archivo << "\n";
    }
    return archivo.good();
}

bool guardarTodo(const std::string& directorioData,
                 const std::vector<Punto>& puntos,
                 const std::vector<Conexion>& conexiones,
                 const std::vector<Unidad>& unidades,
                 const std::vector<Pedido>& pedidos,
                 const EsquemaCSV& esquema) {
    bool okPuntos = guardarPuntos(directorioData + "puntos.csv", puntos, esquema.puntosTieneDias);
    bool okConexiones = guardarConexiones(directorioData + "conexiones.csv", conexiones);
    bool okUnidades = guardarUnidades(directorioData + "unidades.csv", unidades, esquema.unidadesTieneEspecialidad);
    bool okPedidos = guardarPedidos(directorioData + "pedidos.csv", pedidos, esquema.pedidosTieneTipoResiduo);
    return okPuntos && okConexiones && okUnidades && okPedidos;
}

// ============================================================================
// Puntos de extension para modulos de equipo (Stubs)
// ============================================================================

// TODO [4.1.3 - Carlos]: Implementar registro de punto de recojo/acopio en Surco
void registrarPunto(std::vector<Punto>& /*puntos*/) {
    std::cout << "[Pendiente] TODO [4.1.3 - Carlos]: Registrar punto de recojo/acopio.\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de via/conexion entre puntos
void registrarConexion(std::vector<Conexion>& /*conexiones*/,
                       const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Pendiente] TODO [4.1.3 - Carlos]: Registrar via/conexion.\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de camion recolector
void registrarUnidad(std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Pendiente] TODO [4.1.3 - Carlos]: Registrar camion recolector.\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de solicitud de recojo
void registrarPedido(std::vector<Pedido>& /*pedidos*/,
                     const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Pendiente] TODO [4.1.3 - Carlos]: Registrar solicitud de recojo.\n";
}

void menuRegistrarDatos(std::vector<Punto>& puntos,
                        std::vector<Conexion>& conexiones,
                        std::vector<Unidad>& unidades,
                        std::vector<Pedido>& pedidos) {
    // TODO [4.1.3 - Carlos]: Completar flujo de registro y validaciones
    std::cout << "\n--- REGISTRAR DATOS (SURCO) ---\n";
    std::cout << "1. Punto de recojo / acopio\n";
    std::cout << "2. Via / Conexion\n";
    std::cout << "3. Camion recolector\n";
    std::cout << "4. Solicitud de recojo\n";
    std::cout << "0. Volver\n";

    int sub = -1;
    if (!leerEnteroSeguro("Seleccione una subopcion: ", sub)) {
        std::cout << "Entrada invalida.\n";
        return;
    }

    switch (sub) {
        case 1: registrarPunto(puntos); break;
        case 2: registrarConexion(conexiones, puntos); break;
        case 3: registrarUnidad(unidades); break;
        case 4: registrarPedido(pedidos, puntos); break;
        case 0: break;
        default: std::cout << "Subopcion no valida.\n"; break;
    }
}

// TODO [4.1.4 - Jason]: Implementar habilitar o cerrar una via existente
void cambiarEstadoConexion(std::vector<Conexion>& /*conexiones*/) {
    std::cout << "[Pendiente] TODO [4.1.4 - Jason]: Habilitar/Cerrar via.\n";
}

// TODO [4.1.4 - Jason]: Implementar asignacion de solicitud pendiente a camion disponible
void asignarPedidoAUnidad(std::vector<Pedido>& /*pedidos*/,
                          std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Pendiente] TODO [4.1.4 - Jason]: Asignar solicitud a camion.\n";
}

// TODO [4.1.4 - Jason]: Implementar marcado de solicitud como entregada y liberacion de unidad
void marcarPedidoEntregado(std::vector<Pedido>& /*pedidos*/,
                           std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Pendiente] TODO [4.1.4 - Jason]: Marcar solicitud como entregada.\n";
}

void menuGestionarEstados(std::vector<Conexion>& conexiones,
                          std::vector<Unidad>& unidades,
                          std::vector<Pedido>& pedidos) {
    // TODO [4.1.4 - Jason]: Completar flujo de gestion de estados
    std::cout << "\n--- GESTIONAR ESTADOS (SURCO) ---\n";
    std::cout << "1. Habilitar / Cerrar via\n";
    std::cout << "2. Asignar solicitud a camion\n";
    std::cout << "3. Marcar solicitud como entregada\n";
    std::cout << "0. Volver\n";

    int sub = -1;
    if (!leerEnteroSeguro("Seleccione una subopcion: ", sub)) {
        std::cout << "Entrada invalida.\n";
        return;
    }

    switch (sub) {
        case 1: cambiarEstadoConexion(conexiones); break;
        case 2: asignarPedidoAUnidad(pedidos, unidades); break;
        case 3: marcarPedidoEntregado(pedidos, unidades); break;
        case 0: break;
        default: std::cout << "Subopcion no valida.\n"; break;
    }
}

// TODO [4.1.6 - Diana]: Listar puntos registrados
void listarPuntos(const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Pendiente] TODO [4.1.6 - Diana]: Listar puntos.\n";
}

// TODO [4.1.6 - Diana]: Consultar conexiones habilitadas/cerradas de un punto
void consultarConexionesDePunto(const std::vector<Conexion>& /*conexiones*/) {
    std::cout << "[Pendiente] TODO [4.1.6 - Diana]: Consultar conexiones de un punto.\n";
}

// TODO [4.1.6 - Diana]: Consultar solicitudes filtradas por estado
void consultarPedidosPorEstado(const std::vector<Pedido>& /*pedidos*/) {
    std::cout << "[Pendiente] TODO [4.1.6 - Diana]: Consultar solicitudes por estado.\n";
}

// TODO [4.1.6 - Diana]: Consultar solicitudes asignadas a un camion
void consultarPedidosPorUnidad(const std::vector<Pedido>& /*pedidos*/) {
    std::cout << "[Pendiente] TODO [4.1.6 - Diana]: Consultar solicitudes por camion.\n";
}

void menuConsultasBasicas(const std::vector<Punto>& puntos,
                          const std::vector<Conexion>& conexiones,
                          const std::vector<Pedido>& pedidos) {
    // TODO [4.1.6 - Diana]: Completar consultas basicas
    std::cout << "\n--- CONSULTAS BASICAS (SURCO) ---\n";
    std::cout << "1. Listar puntos\n";
    std::cout << "2. Conexiones de un punto\n";
    std::cout << "3. Solicitudes por estado\n";
    std::cout << "4. Solicitudes por camion\n";
    std::cout << "0. Volver\n";

    int sub = -1;
    if (!leerEnteroSeguro("Seleccione una subopcion: ", sub)) {
        std::cout << "Entrada invalida.\n";
        return;
    }

    switch (sub) {
        case 1: listarPuntos(puntos); break;
        case 2: consultarConexionesDePunto(conexiones); break;
        case 3: consultarPedidosPorEstado(pedidos); break;
        case 4: consultarPedidosPorUnidad(pedidos); break;
        case 0: break;
        default: std::cout << "Subopcion no valida.\n"; break;
    }
}

// ============================================================================
// [4.1.7] Reporte general del sistema
// ============================================================================

void mostrarReporteGeneral(const std::vector<Punto>& puntos,
                           const std::vector<Conexion>& conexiones,
                           const std::vector<Unidad>& unidades,
                           const std::vector<Pedido>& pedidos) {
    int habilitadas = 0;
    int cerradas = 0;
    for (const Conexion& c : conexiones) {
        if (c.estado == "habilitada") {
            ++habilitadas;
        } else if (c.estado == "cerrada") {
            ++cerradas;
        }
    }

    int disponibles = 0;
    for (const Unidad& u : unidades) {
        if (u.estado == "disponible") {
            ++disponibles;
        }
    }

    int pendientes = 0;
    int asignados = 0;
    int entregados = 0;
    std::vector<std::pair<std::string, int>> conteoPorUnidad;

    for (const Pedido& p : pedidos) {
        if (p.estado == "pendiente") {
            ++pendientes;
        } else if (p.estado == "asignado") {
            ++asignados;
        } else if (p.estado == "entregado") {
            ++entregados;
        }

        if (!p.unidad_asignada.empty()) {
            bool encontrada = false;
            for (auto& par : conteoPorUnidad) {
                if (par.first == p.unidad_asignada) {
                    ++par.second;
                    encontrada = true;
                    break;
                }
            }
            if (!encontrada) {
                conteoPorUnidad.push_back({p.unidad_asignada, 1});
            }
        }
    }

    std::string unidadTop = "Ninguna";
    int maxPedidos = 0;
    for (const auto& par : conteoPorUnidad) {
        if (par.second > maxPedidos) {
            maxPedidos = par.second;
            unidadTop = par.first;
        }
    }

    std::cout << "\n=== REPORTE GENERAL - RECOLECCION DE RESIDUOS SURCO ===\n";
    std::cout << "Puntos registrados: " << puntos.size() << "\n";
    std::cout << "Conexiones habilitadas: " << habilitadas << " | Cerradas: " << cerradas << "\n";
    std::cout << "Unidades disponibles: " << disponibles << " de " << unidades.size() << "\n";
    std::cout << "Pedidos pendientes: " << pendientes
              << " | Asignados: " << asignados
              << " | Entregados: " << entregados << "\n";
    if (maxPedidos == 0) {
        std::cout << "Unidad con mas pedidos: Ninguna (0 pedidos)\n";
    } else {
        std::cout << "Unidad con mas pedidos: " << unidadTop
                  << " (" << maxPedidos << " pedidos)\n";
    }
}

// ============================================================================
// [4.1.1] Menu principal
// ============================================================================

int main() {
    std::vector<Punto> puntos;
    std::vector<Conexion> conexiones;
    std::vector<Unidad> unidades;
    std::vector<Pedido> pedidos;
    EsquemaCSV esquema;

    const std::string directorioData = detectarDirectorioDatos();
    cargarTodo(directorioData, puntos, conexiones, unidades, pedidos, esquema);

    int opcion = 0;
    while (true) {
        std::cout << "\n=== SISTEMA DE RECOLECCION DE RESIDUOS - SURCO ===\n";
        std::cout << "1. Registrar datos (Puntos de recojo/acopio, Vias/Conexiones, Camiones recolectores, Solicitudes de recojo)\n";
        std::cout << "2. Gestionar estados (Habilitar/Cerrar via, Asignar solicitud a camion, Marcar solicitud como entregada)\n";
        std::cout << "3. Consultas basicas (Listar puntos, Conexiones de un punto, Solicitudes por estado, Solicitudes por camion)\n";
        std::cout << "4. Reporte general del sistema\n";
        std::cout << "5. Salir\n";

        if (!leerEnteroSeguro("Seleccione una opcion: ", opcion)) {
            if (std::cin.eof()) {
                break;
            }
            std::cout << "Por favor, ingrese un numero valido.\n";
            continue;
        }

        switch (opcion) {
            case 1:
                menuRegistrarDatos(puntos, conexiones, unidades, pedidos);
                guardarTodo(directorioData, puntos, conexiones, unidades, pedidos, esquema);
                break;
            case 2:
                menuGestionarEstados(conexiones, unidades, pedidos);
                guardarTodo(directorioData, puntos, conexiones, unidades, pedidos, esquema);
                break;
            case 3:
                menuConsultasBasicas(puntos, conexiones, pedidos);
                break;
            case 4:
                mostrarReporteGeneral(puntos, conexiones, unidades, pedidos);
                break;
            case 5:
                std::cout << "Saliendo del sistema. Hasta luego.\n";
                return 0;
            default:
                std::cout << "Opcion invalida. Intente nuevamente.\n";
                break;
        }
    }

    return 0;
}
