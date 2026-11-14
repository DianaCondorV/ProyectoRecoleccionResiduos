#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// [4.1.2] Estructuras de datos base del sistema (Santiago de Surco)
// ============================================================================

struct Punto {
    std::string codigo;
    std::string nombre;
    std::string tipo_punto;
    std::string zona;
};

struct Conexion {
    std::string origen;
    std::string destino;
    double distancia_km;
    int tiempo_min;
    std::string estado;
};

struct Unidad {
    std::string codigo;
    std::string tipo;
    double capacidad_kg;
    double costo_km;
    std::string estado;
};

struct Pedido {
    std::string codigo;
    std::string origen;
    std::string destino;
    double carga_kg;
    std::string prioridad;
    std::string estado;
    std::string unidad_asignada;
};

// ============================================================================
// Funciones auxiliares de texto, CSV y resolucion de rutas
// ============================================================================

std::string recortar(const std::string& texto) {
    std::size_t inicio = 0;
    while (inicio < texto.size() &&
           (texto[inicio] == ' ' || texto[inicio] == '\t' ||
            texto[inicio] == '\r' || texto[inicio] == '\n')) {
        ++inicio;
    }
    std::size_t fin = texto.size();
    while (fin > inicio &&
           (texto[fin - 1] == ' ' || texto[fin - 1] == '\t' ||
            texto[fin - 1] == '\r' || texto[fin - 1] == '\n')) {
        --fin;
    }
    return texto.substr(inicio, fin - inicio);
}

std::string aMinusculas(std::string texto) {
    for (char& ch : texto) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return texto;
}

std::vector<std::string> dividirCSV(std::string linea) {
    if (!linea.empty() && linea.back() == '\r') {
        linea.pop_back();
    }
    // Eliminar posible BOM UTF-8 al inicio del archivo
    if (linea.size() >= 3 &&
        static_cast<unsigned char>(linea[0]) == 0xEF &&
        static_cast<unsigned char>(linea[1]) == 0xBB &&
        static_cast<unsigned char>(linea[2]) == 0xBF) {
        linea.erase(0, 3);
    }

    std::vector<std::string> campos;
    std::size_t inicio = 0;
    while (true) {
        std::size_t pos = linea.find(',', inicio);
        if (pos == std::string::npos) {
            campos.push_back(recortar(linea.substr(inicio)));
            break;
        }
        campos.push_back(recortar(linea.substr(inicio, pos - inicio)));
        inicio = pos + 1;
    }
    return campos;
}

std::string normalizarNombreCSV(const std::string& nombreArchivo) {
    if (nombreArchivo.size() >= 4 &&
        aMinusculas(nombreArchivo.substr(nombreArchivo.size() - 4)) == ".csv") {
        return nombreArchivo;
    }
    return nombreArchivo + ".csv";
}

// Busca primero en data/<archivo>.csv, luego ../data/<archivo>.csv y ./<archivo>.csv.
// Si ninguno existe todavia, determina la carpeta data/ adecuada para crearlo.
std::string resolverRutaCSV(const std::string& nombreArchivo) {
    const std::string archivo = normalizarNombreCSV(nombreArchivo);
    const std::vector<std::string> candidatos = {
        "data/" + archivo,
        "../data/" + archivo,
        "./" + archivo
    };

    for (const std::string& ruta : candidatos) {
        if (fs::exists(ruta) && fs::is_regular_file(ruta)) {
            return ruta;
        }
    }

    if (fs::exists("data") && fs::is_directory("data")) {
        return "data/" + archivo;
    }
    if (fs::exists("../data") && fs::is_directory("../data")) {
        return "../data/" + archivo;
    }

    fs::create_directories("data");
    return "data/" + archivo;
}

void asegurarArchivoCSV(const std::string& ruta, const std::string& cabecera) {
    if (fs::exists(ruta)) {
        return;
    }
    fs::path p(ruta);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    std::ofstream salida(ruta);
    if (salida.is_open()) {
        salida << cabecera << "\n";
    } else {
        std::cerr << "[ERROR] No se pudo crear el archivo inicial: " << ruta << "\n";
    }
}

int leerOpcionEntera(const std::string& prompt, int defecto = -1) {
    std::cout << prompt;
    std::string linea;
    if (!std::getline(std::cin, linea)) {
        return defecto;
    }
    linea = recortar(linea);
    if (linea.empty()) {
        return -1;
    }
    try {
        std::size_t procesados = 0;
        int valor = std::stoi(linea, &procesados);
        if (procesados != linea.size()) {
            return -1;
        }
        return valor;
    } catch (...) {
        return -1;
    }
}

// ============================================================================
// [4.1.2] Carga inicial desde data/*.csv
// ============================================================================

std::vector<Punto> cargarPuntos() {
    std::vector<Punto> puntos;
    const std::string ruta = resolverRutaCSV("puntos.csv");
    asegurarArchivoCSV(ruta, "codigo,nombre,tipo_punto,zona");

    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << ruta << "\n";
        return puntos;
    }

    std::string linea;
    bool esCabecera = true;
    while (std::getline(archivo, linea)) {
        if (recortar(linea).empty()) {
            continue;
        }
        if (esCabecera) {
            esCabecera = false;
            continue;
        }
        // Tolerar columnas extra al final (ej. dias_sin_recolectar)
        std::vector<std::string> campos = dividirCSV(linea);
        if (campos.size() >= 4) {
            Punto p;
            p.codigo = campos[0];
            p.nombre = campos[1];
            p.tipo_punto = campos[2];
            p.zona = campos[3];
            puntos.push_back(p);
        }
    }
    return puntos;
}

std::vector<Conexion> cargarConexiones() {
    std::vector<Conexion> conexiones;
    const std::string ruta = resolverRutaCSV("conexiones.csv");
    asegurarArchivoCSV(ruta, "origen,destino,distancia_km,tiempo_min,estado");

    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << ruta << "\n";
        return conexiones;
    }

    std::string linea;
    bool esCabecera = true;
    while (std::getline(archivo, linea)) {
        if (recortar(linea).empty()) {
            continue;
        }
        if (esCabecera) {
            esCabecera = false;
            continue;
        }
        std::vector<std::string> campos = dividirCSV(linea);
        if (campos.size() >= 5) {
            try {
                Conexion c;
                c.origen = campos[0];
                c.destino = campos[1];
                c.distancia_km = std::stod(campos[2]);
                c.tiempo_min = std::stoi(campos[3]);
                c.estado = campos[4];
                conexiones.push_back(c);
            } catch (...) {
                std::cerr << "[ADVERTENCIA] Fila invalida en conexiones.csv: " << linea << "\n";
            }
        }
    }
    return conexiones;
}

std::vector<Unidad> cargarUnidades() {
    std::vector<Unidad> unidades;
    const std::string ruta = resolverRutaCSV("unidades.csv");
    asegurarArchivoCSV(ruta, "codigo,tipo,capacidad_kg,costo_km,estado");

    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << ruta << "\n";
        return unidades;
    }

    std::string linea;
    bool esCabecera = true;
    while (std::getline(archivo, linea)) {
        if (recortar(linea).empty()) {
            continue;
        }
        if (esCabecera) {
            esCabecera = false;
            continue;
        }
        // Tolerar columnas extra al final (ej. especialidad)
        std::vector<std::string> campos = dividirCSV(linea);
        if (campos.size() >= 5) {
            try {
                Unidad u;
                u.codigo = campos[0];
                u.tipo = campos[1];
                u.capacidad_kg = std::stod(campos[2]);
                u.costo_km = std::stod(campos[3]);
                u.estado = campos[4];
                unidades.push_back(u);
            } catch (...) {
                std::cerr << "[ADVERTENCIA] Fila invalida en unidades.csv: " << linea << "\n";
            }
        }
    }
    return unidades;
}

std::vector<Pedido> cargarPedidos() {
    std::vector<Pedido> pedidos;
    const std::string ruta = resolverRutaCSV("pedidos.csv");
    asegurarArchivoCSV(ruta, "codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada");

    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << ruta << "\n";
        return pedidos;
    }

    std::string linea;
    bool esCabecera = true;
    while (std::getline(archivo, linea)) {
        if (recortar(linea).empty()) {
            continue;
        }
        if (esCabecera) {
            esCabecera = false;
            continue;
        }
        // Manejar correctamente campos vacios al final y tolerar columnas extra (ej. tipo_residuo)
        std::vector<std::string> campos = dividirCSV(linea);
        if (campos.size() >= 7) {
            try {
                Pedido ped;
                ped.codigo = campos[0];
                ped.origen = campos[1];
                ped.destino = campos[2];
                ped.carga_kg = std::stod(campos[3]);
                ped.prioridad = campos[4];
                ped.estado = campos[5];
                ped.unidad_asignada = campos[6];
                pedidos.push_back(ped);
            } catch (...) {
                std::cerr << "[ADVERTENCIA] Fila invalida en pedidos.csv: " << linea << "\n";
            }
        }
    }
    return pedidos;
}

void cargaInicial(std::vector<Punto>& puntos,
                  std::vector<Conexion>& conexiones,
                  std::vector<Unidad>& unidades,
                  std::vector<Pedido>& pedidos) {
    puntos = cargarPuntos();
    conexiones = cargarConexiones();
    unidades = cargarUnidades();
    pedidos = cargarPedidos();
}

// ============================================================================
// [4.1.5] Generacion y persistencia de archivos CSV
// ============================================================================

void guardarPuntos(const std::vector<Punto>& puntos) {
    const std::string ruta = resolverRutaCSV("puntos.csv");
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo guardar " << ruta << "\n";
        return;
    }
    archivo << "codigo,nombre,tipo_punto,zona\n";
    for (const Punto& p : puntos) {
        archivo << p.codigo << ","
                << p.nombre << ","
                << p.tipo_punto << ","
                << p.zona << "\n";
    }
}

void guardarConexiones(const std::vector<Conexion>& conexiones) {
    const std::string ruta = resolverRutaCSV("conexiones.csv");
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo guardar " << ruta << "\n";
        return;
    }
    archivo << "origen,destino,distancia_km,tiempo_min,estado\n";
    for (const Conexion& c : conexiones) {
        archivo << c.origen << ","
                << c.destino << ","
                << c.distancia_km << ","
                << c.tiempo_min << ","
                << c.estado << "\n";
    }
}

void guardarUnidades(const std::vector<Unidad>& unidades) {
    const std::string ruta = resolverRutaCSV("unidades.csv");
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo guardar " << ruta << "\n";
        return;
    }
    archivo << "codigo,tipo,capacidad_kg,costo_km,estado\n";
    for (const Unidad& u : unidades) {
        archivo << u.codigo << ","
                << u.tipo << ","
                << u.capacidad_kg << ","
                << u.costo_km << ","
                << u.estado << "\n";
    }
}

void guardarPedidos(const std::vector<Pedido>& pedidos) {
    const std::string ruta = resolverRutaCSV("pedidos.csv");
    std::ofstream archivo(ruta, std::ios::trunc);
    if (!archivo.is_open()) {
        std::cerr << "[ERROR] No se pudo guardar " << ruta << "\n";
        return;
    }
    archivo << "codigo,origen,destino,carga_kg,prioridad,estado,unidad_asignada\n";
    for (const Pedido& ped : pedidos) {
        archivo << ped.codigo << ","
                << ped.origen << ","
                << ped.destino << ","
                << ped.carga_kg << ","
                << ped.prioridad << ","
                << ped.estado << ","
                << ped.unidad_asignada << "\n";
    }
}

void guardarTodo(const std::vector<Punto>& puntos,
                 const std::vector<Conexion>& conexiones,
                 const std::vector<Unidad>& unidades,
                 const std::vector<Pedido>& pedidos) {
    guardarPuntos(puntos);
    guardarConexiones(conexiones);
    guardarUnidades(unidades);
    guardarPedidos(pedidos);
}

// ============================================================================
// Puntos de extension (Stubs para integracion del equipo)
// ============================================================================

// TODO [4.1.3 - Carlos]: Implementar registro de punto de recojo/acopio
void registrarPunto(std::vector<Punto>& /*puntos*/) {
    std::cout << "[Stub] Registrar punto de recojo/acopio - Pendiente [4.1.3 - Carlos]\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de via/conexion
void registrarConexion(std::vector<Conexion>& /*conexiones*/,
                       const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Stub] Registrar via/conexion - Pendiente [4.1.3 - Carlos]\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de camion recolector
void registrarUnidad(std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Stub] Registrar camion recolector - Pendiente [4.1.3 - Carlos]\n";
}

// TODO [4.1.3 - Carlos]: Implementar registro de solicitud de recojo
void registrarPedido(std::vector<Pedido>& /*pedidos*/,
                     const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Stub] Registrar solicitud de recojo - Pendiente [4.1.3 - Carlos]\n";
}

void menuRegistrarDatos(std::vector<Punto>& puntos,
                        std::vector<Conexion>& conexiones,
                        std::vector<Unidad>& unidades,
                        std::vector<Pedido>& pedidos) {
    int subopcion = -1;
    do {
        std::cout << "\n--- REGISTRAR DATOS (SURCO) ---\n";
        std::cout << "1. Registrar punto de recojo/acopio\n";
        std::cout << "2. Registrar via/conexion\n";
        std::cout << "3. Registrar camion recolector\n";
        std::cout << "4. Registrar solicitud de recojo\n";
        std::cout << "0. Volver al menu principal\n";
        subopcion = leerOpcionEntera("Seleccione una opcion: ", 0);

        switch (subopcion) {
            case 1:
                // TODO [4.1.3 - Carlos]
                registrarPunto(puntos);
                break;
            case 2:
                // TODO [4.1.3 - Carlos]
                registrarConexion(conexiones, puntos);
                break;
            case 3:
                // TODO [4.1.3 - Carlos]
                registrarUnidad(unidades);
                break;
            case 4:
                // TODO [4.1.3 - Carlos]
                registrarPedido(pedidos, puntos);
                break;
            case 0:
                break;
            default:
                std::cout << "Opcion invalida.\n";
                break;
        }
    } while (subopcion != 0);
}

// TODO [4.1.4 - Jason]: Implementar habilitar/cerrar via
void gestionarEstadoVia(std::vector<Conexion>& /*conexiones*/) {
    std::cout << "[Stub] Habilitar/Cerrar via - Pendiente [4.1.4 - Jason]\n";
}

// TODO [4.1.4 - Jason]: Implementar asignacion de solicitud a camion
void asignarSolicitudACamion(std::vector<Pedido>& /*pedidos*/,
                               std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Stub] Asignar solicitud a camion - Pendiente [4.1.4 - Jason]\n";
}

// TODO [4.1.4 - Jason]: Implementar marcado de solicitud como entregada
void marcarSolicitudEntregada(std::vector<Pedido>& /*pedidos*/,
                              std::vector<Unidad>& /*unidades*/) {
    std::cout << "[Stub] Marcar solicitud como entregada - Pendiente [4.1.4 - Jason]\n";
}

void menuGestionarEstados(std::vector<Conexion>& conexiones,
                          std::vector<Unidad>& unidades,
                          std::vector<Pedido>& pedidos) {
    int subopcion = -1;
    do {
        std::cout << "\n--- GESTIONAR ESTADOS ---\n";
        std::cout << "1. Habilitar/Cerrar via\n";
        std::cout << "2. Asignar solicitud a camion\n";
        std::cout << "3. Marcar solicitud como entregada\n";
        std::cout << "0. Volver al menu principal\n";
        subopcion = leerOpcionEntera("Seleccione una opcion: ", 0);

        switch (subopcion) {
            case 1:
                // TODO [4.1.4 - Jason]
                gestionarEstadoVia(conexiones);
                break;
            case 2:
                // TODO [4.1.4 - Jason]
                asignarSolicitudACamion(pedidos, unidades);
                break;
            case 3:
                // TODO [4.1.4 - Jason]
                marcarSolicitudEntregada(pedidos, unidades);
                break;
            case 0:
                break;
            default:
                std::cout << "Opcion invalida.\n";
                break;
        }
    } while (subopcion != 0);
}

// TODO [4.1.6 - Diana]: Implementar listado de puntos
void listarPuntos(const std::vector<Punto>& /*puntos*/) {
    std::cout << "[Stub] Listar puntos - Pendiente [4.1.6 - Diana]\n";
}

// TODO [4.1.6 - Diana]: Implementar consulta de conexiones de un punto
void consultarConexionesDePunto(const std::vector<Conexion>& /*conexiones*/) {
    std::cout << "[Stub] Conexiones de un punto - Pendiente [4.1.6 - Diana]\n";
}

// TODO [4.1.6 - Diana]: Implementar consulta de solicitudes por estado
void consultarSolicitudesPorEstado(const std::vector<Pedido>& /*pedidos*/) {
    std::cout << "[Stub] Solicitudes por estado - Pendiente [4.1.6 - Diana]\n";
}

// TODO [4.1.6 - Diana]: Implementar consulta de solicitudes por camion
void consultarSolicitudesPorCamion(const std::vector<Pedido>& /*pedidos*/) {
    std::cout << "[Stub] Solicitudes por camion - Pendiente [4.1.6 - Diana]\n";
}

void menuConsultasBasicas(const std::vector<Punto>& puntos,
                          const std::vector<Conexion>& conexiones,
                          const std::vector<Pedido>& pedidos) {
    int subopcion = -1;
    do {
        std::cout << "\n--- CONSULTAS BASICAS ---\n";
        std::cout << "1. Listar puntos de recojo y acopio\n";
        std::cout << "2. Consultar conexiones de un punto\n";
        std::cout << "3. Consultar solicitudes por estado\n";
        std::cout << "4. Consultar solicitudes por camion\n";
        std::cout << "0. Volver al menu principal\n";
        subopcion = leerOpcionEntera("Seleccione una opcion: ", 0);

        switch (subopcion) {
            case 1:
                // TODO [4.1.6 - Diana]
                listarPuntos(puntos);
                break;
            case 2:
                // TODO [4.1.6 - Diana]
                consultarConexionesDePunto(conexiones);
                break;
            case 3:
                // TODO [4.1.6 - Diana]
                consultarSolicitudesPorEstado(pedidos);
                break;
            case 4:
                // TODO [4.1.6 - Diana]
                consultarSolicitudesPorCamion(pedidos);
                break;
            case 0:
                break;
            default:
                std::cout << "Opcion invalida.\n";
                break;
        }
    } while (subopcion != 0);
}

// ============================================================================
// [4.1.7] Reporte general del sistema (Juan David Morales Rabelly)
// ============================================================================

void mostrarReporteGeneral(const std::vector<Punto>& puntos,
                           const std::vector<Conexion>& conexiones,
                           const std::vector<Unidad>& unidades,
                           const std::vector<Pedido>& pedidos) {
    int habilitadas = 0;
    int cerradas = 0;
    for (const Conexion& c : conexiones) {
        const std::string est = aMinusculas(recortar(c.estado));
        if (est == "habilitada") {
            ++habilitadas;
        } else if (est == "cerrada") {
            ++cerradas;
        }
    }

    int disponibles = 0;
    for (const Unidad& u : unidades) {
        if (aMinusculas(recortar(u.estado)) == "disponible") {
            ++disponibles;
        }
    }

    int pendientes = 0;
    int asignados = 0;
    int entregados = 0;
    std::vector<std::pair<std::string, int>> conteoPorUnidad;

    for (const Pedido& ped : pedidos) {
        const std::string est = aMinusculas(recortar(ped.estado));
        if (est == "pendiente") {
            ++pendientes;
        } else if (est == "asignado") {
            ++asignados;
        } else if (est == "entregado") {
            ++entregados;
        }

        const std::string codUnidad = recortar(ped.unidad_asignada);
        if (!codUnidad.empty()) {
            bool encontrada = false;
            for (auto& par : conteoPorUnidad) {
                if (par.first == codUnidad) {
                    ++par.second;
                    encontrada = true;
                    break;
                }
            }
            if (!encontrada) {
                conteoPorUnidad.push_back({codUnidad, 1});
            }
        }
    }

    std::string unidadTop;
    int maxPedidos = 0;
    for (const auto& par : conteoPorUnidad) {
        if (par.second > maxPedidos) {
            maxPedidos = par.second;
            unidadTop = par.first;
        }
    }

    std::cout << "\n=== REPORTE GENERAL - RECOLECCION DE RESIDUOS SURCO ===\n";
    std::cout << "Puntos registrados: " << puntos.size() << "\n";
    std::cout << "Conexiones habilitadas: " << habilitadas
              << " | Cerradas: " << cerradas << "\n";
    std::cout << "Unidades disponibles: " << disponibles
              << " de " << unidades.size() << "\n";
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

    cargaInicial(puntos, conexiones, unidades, pedidos);

    int opcion = -1;
    do {
        std::cout << "\n============================================================\n";
        std::cout << "  RECOLECCION DE RESIDUOS SOLIDOS - SANTIAGO DE SURCO\n";
        std::cout << "============================================================\n";
        std::cout << "1. Registrar datos (Puntos, Vias, Camiones, Solicitudes)\n";
        std::cout << "2. Gestionar estados (Vias y Solicitudes)\n";
        std::cout << "3. Consultas basicas\n";
        std::cout << "4. Reporte general del sistema\n";
        std::cout << "5. Salir\n";

        opcion = leerOpcionEntera("Seleccione una opcion: ", 5);

        switch (opcion) {
            case 1:
                menuRegistrarDatos(puntos, conexiones, unidades, pedidos);
                guardarTodo(puntos, conexiones, unidades, pedidos);
                break;
            case 2:
                menuGestionarEstados(conexiones, unidades, pedidos);
                guardarTodo(puntos, conexiones, unidades, pedidos);
                break;
            case 3:
                menuConsultasBasicas(puntos, conexiones, pedidos);
                break;
            case 4:
                mostrarReporteGeneral(puntos, conexiones, unidades, pedidos);
                break;
            case 5:
                std::cout << "Saliendo del sistema...\n";
                break;
            default:
                std::cout << "Opcion invalida. Intente nuevamente.\n";
                break;
        }
    } while (opcion != 5);

    return 0;
}
