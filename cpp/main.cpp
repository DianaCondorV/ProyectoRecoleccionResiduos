#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <cctype>
#include <utility>
#include <cstdlib>

using namespace std;

// ============================================================================
// 1. Estructuras (Recoleccion de Residuos - Surco)
// ============================================================================

struct Punto {
    string codigo;
    string nombre;
    string tipo_punto;
    string zona;
    int dias_sin_recolectar = 0; // FA - Autor: Diana
};

struct Conexion {
    string origen;
    string destino;
    double distancia_km = 0.0;
    int tiempo_min = 0;
    string estado;
};

struct Unidad {
    string codigo;
    string tipo;
    double capacidad_kg = 0.0;
    double costo_km = 0.0;
    string estado;
    string especialidad; // FA - Autor: Juan
};

struct Pedido {
    string codigo;
    string origen;
    string destino;
    double carga_kg = 0.0;
    string prioridad;
    string estado;
    string unidad_asignada;
    string tipo_residuo; // FA - Autor: Juan
};

// FA - Autor: Carlos
struct Combustible {
    string unidad;
    double capacidad_tanque_l = 0.0;
    double combustible_actual_l = 0.0;
    double consumo_l_km = 0.0;
};

// FA - Autor: Jason
struct Mantenimiento {
    string unidad;
    double km_actual = 0.0;
    double km_ultimo_mantenimiento = 0.0;
    double intervalo_km = 0.0;
};

// FA - Autor: Jason
struct HistorialMantenimiento {
    string codigo_servicio;
    string unidad;
    string fecha;
    double kilometraje = 0.0;
    string descripcion;
};

// Control de columnas adicionales en los archivos CSV
struct EsquemaCSV {
    bool puntosTieneDias = false;
    bool unidadesTieneEspecialidad = false;
    bool pedidosTieneTipoResiduo = false;
};


// ============================================================================
// 2. UTILIDADES GENERALES
// ============================================================================

// Elimina espacios al inicio y al final de un texto.
string limpiarTexto(const string& texto) {
    size_t inicio = 0;

    while (inicio < texto.size() &&
           isspace(static_cast<unsigned char>(texto[inicio]))) {
        ++inicio;
    }

    size_t fin = texto.size();

    while (fin > inicio &&
           isspace(static_cast<unsigned char>(texto[fin - 1]))) {
        --fin;
    }

    return texto.substr(inicio, fin - inicio);
}

// Elimina el BOM de archivos CSV guardados con UTF-8.
void quitarBOM(string& linea) {
    if (linea.size() >= 3 &&
        static_cast<unsigned char>(linea[0]) == 0xEF &&
        static_cast<unsigned char>(linea[1]) == 0xBB &&
        static_cast<unsigned char>(linea[2]) == 0xBF) {

        linea.erase(0, 3);
    }
}

// Separa los campos de una línea CSV respetando las comillas.
vector<string> dividirCSV(const string& linea) {
    vector<string> campos;
    string actual;
    bool entreComillas = false;

    for (size_t i = 0; i < linea.size(); ++i) {
        char c = linea[i];

        if (c == '"') {
            // Dos comillas consecutivas dentro de un campo representan
            // una comilla literal.
            if (entreComillas && i + 1 < linea.size() &&
                linea[i + 1] == '"') {

                actual += '"';
                ++i;
            } else {
                entreComillas = !entreComillas;
            }
        }
        else if (c == ',' && !entreComillas) {
            campos.push_back(limpiarTexto(actual));
            actual.clear();
        }
        else if (c != '\r') {
            actual += c;
        }
    }

    campos.push_back(limpiarTexto(actual));

    return campos;
}

// Convierte texto a double y comprueba que todo el contenido sea válido.
bool parsearDouble(const string& texto, double& resultado) {
    string valorTexto = limpiarTexto(texto);

    if (valorTexto.empty()) {
        return false;
    }

    try {
        size_t procesados = 0;
        double valor = stod(valorTexto, &procesados);

        if (procesados != valorTexto.size()) {
            return false;
        }

        resultado = valor;
        return true;
    }
    catch (...) {
        return false;
    }
}

// Convierte texto a entero y comprueba que todo el contenido sea válido.
bool parsearInt(const string& texto, int& resultado) {
    string valorTexto = limpiarTexto(texto);

    if (valorTexto.empty()) {
        return false;
    }

    try {
        size_t procesados = 0;
        int valor = stoi(valorTexto, &procesados);

        if (procesados != valorTexto.size()) {
            return false;
        }

        resultado = valor;
        return true;
    }
    catch (...) {
        return false;
    }
}

// Comprueba si un archivo existe y puede abrirse para lectura.
bool archivoExiste(const string& ruta) {
    ifstream archivo(ruta);
    return archivo.good();
}

// Detecta el directorio donde se encuentran los archivos CSV.
string detectarDirectorioDatos() {
    const vector<string> prefijos = {
        "data/",
        "../../data/",
        "../data/",
        ""
    };

    const vector<string> archivos = {
        "puntos.csv",
        "conexiones.csv",
        "unidades.csv",
        "pedidos.csv"
    };

    for (const string& prefijo : prefijos) {
        for (const string& nombre : archivos) {
            if (archivoExiste(prefijo + nombre)) {
                return prefijo;
            }
        }
    }

    // Si no encuentra archivos, devuelve la ruta data/
    // para que el sistema pueda crear allí los CSV.
    return "data/";
}

// Lee un entero desde consola y valida su formato.
bool leerEnteroSeguro(const string& mensaje, int& valor) {
    cout << mensaje;

    string linea;

    if (!getline(cin, linea)) {
        return false;
    }

    return parsearInt(limpiarTexto(linea), valor);
}


// ============================================================================
// 3. Carga y guardado de archivos CSV
// ============================================================================

// Convierte un texto en un campo CSV, protegiendo comas y comillas.
string escaparCampoCSV(const string& texto) {
    // Solo se usan comillas si el texto contiene coma, comilla o salto de
    // linea. Asi los CSV quedan simples de leer desde Haskell y Prolog.
    if (texto.find_first_of(",\"\n\r") == string::npos) {
        return texto;
    }

    string resultado = "\"";

    for (char c : texto) {
        if (c == '"') {
            resultado += "\"\"";
        } else {
            resultado += c;
        }
    }

    return resultado + "\"";
}

// Crea el archivo CSV con su cabecera si no existe o está vacío.
void asegurarCSV(const string& ruta, const string& cabecera) {
    ifstream entrada(ruta);

    if (entrada.good() && entrada.peek() != ifstream::traits_type::eof()) {
        return;
    }

    ofstream salida(ruta, ios::app);

    if (salida.is_open()) {
        salida << cabecera << "\n";
    } else {
        cerr << "[ERROR] No se pudo crear " << ruta << "\n";
    }
}

// Comprueba si una cabecera contiene una columna determinada.
bool tieneColumna(const vector<string>& cabecera, const string& nombre) {
    return find(cabecera.begin(), cabecera.end(), nombre) != cabecera.end();
}

// ---------------------------------------------------------------------------
// [4.1.2] CARGA DE LOS SIETE ARCHIVOS - Autor: Juan
// ---------------------------------------------------------------------------

void cargarTodo(
    vector<Punto>& puntos,
    vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial,
    EsquemaCSV& esquema
) {
    string directorio = detectarDirectorioDatos();
    string linea;

    puntos.clear();
    conexiones.clear();
    unidades.clear();
    pedidos.clear();
    combustibles.clear();
    mantenimientos.clear();
    historial.clear();

    esquema.puntosTieneDias = false;
    esquema.unidadesTieneEspecialidad = false;
    esquema.pedidosTieneTipoResiduo = false;

    // PUNTOS
    {
        string ruta = directorio + "puntos.csv";
        asegurarCSV(ruta,
            "codigo,nombre,tipo_punto,zona,dias_sin_recolectar");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);
            vector<string> cabecera = dividirCSV(linea);

            esquema.puntosTieneDias =
                tieneColumna(cabecera, "dias_sin_recolectar");

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 4) continue;

                Punto p;
                p.codigo = c[0];
                p.nombre = c[1];
                p.tipo_punto = c[2];
                p.zona = c[3];

                if (c.size() >= 5) {
                    int dias;
                    if (parsearInt(c[4], dias) && dias >= 0) {
                        p.dias_sin_recolectar = dias;
                    }
                }

                puntos.push_back(p);
            }
        }
    }

    // CONEXIONES
    {
        string ruta = directorio + "conexiones.csv";
        asegurarCSV(ruta,
            "origen,destino,distancia_km,tiempo_min,estado");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 5) continue;

                Conexion conexion;

                if (!parsearDouble(c[2], conexion.distancia_km) ||
                    !parsearInt(c[3], conexion.tiempo_min) ||
                    conexion.distancia_km <= 0 ||
                    conexion.tiempo_min <= 0) {
                    cerr << "[ADVERTENCIA] Conexion invalida: "
                         << linea << "\n";
                    continue;
                }

                conexion.origen = c[0];
                conexion.destino = c[1];
                conexion.estado = c[4];

                conexiones.push_back(conexion);
            }
        }
    }

    // UNIDADES
    {
        string ruta = directorio + "unidades.csv";
        asegurarCSV(ruta,
            "codigo,tipo,capacidad_kg,costo_km,estado,especialidad");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);
            vector<string> cabecera = dividirCSV(linea);

            esquema.unidadesTieneEspecialidad =
                tieneColumna(cabecera, "especialidad");

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 5) continue;

                Unidad u;

                if (!parsearDouble(c[2], u.capacidad_kg) ||
                    !parsearDouble(c[3], u.costo_km) ||
                    u.capacidad_kg <= 0 ||
                    u.costo_km <= 0) {
                    cerr << "[ADVERTENCIA] Unidad invalida: "
                         << linea << "\n";
                    continue;
                }

                u.codigo = c[0];
                u.tipo = c[1];
                u.estado = c[4];
                u.especialidad = c.size() >= 6 ? c[5] : "";

                unidades.push_back(u);
            }
        }
    }

    // PEDIDOS
    {
        string ruta = directorio + "pedidos.csv";
        asegurarCSV(ruta,
            "codigo,origen,destino,carga_kg,prioridad,estado,"
            "unidad_asignada,tipo_residuo");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);
            vector<string> cabecera = dividirCSV(linea);

            esquema.pedidosTieneTipoResiduo =
                tieneColumna(cabecera, "tipo_residuo");

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 7) continue;

                Pedido p;

                if (!parsearDouble(c[3], p.carga_kg) ||
                    p.carga_kg <= 0) {
                    cerr << "[ADVERTENCIA] Pedido invalido: "
                         << linea << "\n";
                    continue;
                }

                p.codigo = c[0];
                p.origen = c[1];
                p.destino = c[2];
                p.prioridad = c[4];
                p.estado = c[5];
                p.unidad_asignada = c[6];
                p.tipo_residuo = c.size() >= 8 ? c[7] : "";

                pedidos.push_back(p);
            }
        }
    }

    // COMBUSTIBLE
    {
        string ruta = directorio + "combustible.csv";
        asegurarCSV(ruta,
            "unidad,capacidad_tanque_l,combustible_actual_l,consumo_l_km");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 4) continue;

                Combustible combustible;

                if (!parsearDouble(c[1], combustible.capacidad_tanque_l) ||
                    !parsearDouble(c[2], combustible.combustible_actual_l) ||
                    !parsearDouble(c[3], combustible.consumo_l_km) ||
                    combustible.capacidad_tanque_l <= 0 ||
                    combustible.combustible_actual_l < 0 ||
                    combustible.combustible_actual_l >
                        combustible.capacidad_tanque_l ||
                    combustible.consumo_l_km <= 0) {
                    cerr << "[ADVERTENCIA] Combustible invalido: "
                         << linea << "\n";
                    continue;
                }

                combustible.unidad = c[0];
                combustibles.push_back(combustible);
            }
        }
    }

    // MANTENIMIENTO
    {
        string ruta = directorio + "mantenimiento.csv";
        asegurarCSV(ruta,
            "unidad,km_actual,km_ultimo_mantenimiento,intervalo_km");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 4) continue;

                Mantenimiento m;

                if (!parsearDouble(c[1], m.km_actual) ||
                    !parsearDouble(c[2], m.km_ultimo_mantenimiento) ||
                    !parsearDouble(c[3], m.intervalo_km) ||
                    m.km_actual < 0 ||
                    m.km_ultimo_mantenimiento < 0 ||
                    m.km_ultimo_mantenimiento > m.km_actual ||
                    m.intervalo_km <= 0) {
                    cerr << "[ADVERTENCIA] Mantenimiento invalido: "
                         << linea << "\n";
                    continue;
                }

                m.unidad = c[0];
                mantenimientos.push_back(m);
            }
        }
    }

    // HISTORIAL DE MANTENIMIENTO
    {
        string ruta = directorio + "historial_mantenimiento.csv";
        asegurarCSV(ruta,
            "codigo_servicio,unidad,fecha,kilometraje,descripcion");

        ifstream archivo(ruta);

        if (getline(archivo, linea)) {
            quitarBOM(linea);

            while (getline(archivo, linea)) {
                if (limpiarTexto(linea).empty()) continue;

                vector<string> c = dividirCSV(linea);
                if (c.size() < 5) continue;

                HistorialMantenimiento h;

                if (!parsearDouble(c[3], h.kilometraje) ||
                    h.kilometraje < 0) {
                    cerr << "[ADVERTENCIA] Historial invalido: "
                         << linea << "\n";
                    continue;
                }

                h.codigo_servicio = c[0];
                h.unidad = c[1];
                h.fecha = c[2];
                h.descripcion = c[4];

                historial.push_back(h);
            }
        }
    }

    cout << "\n[OK] Carga inicial completada.\n";
    cout << "Puntos: " << puntos.size() << "\n";
    cout << "Conexiones: " << conexiones.size() << "\n";
    cout << "Unidades: " << unidades.size() << "\n";
    cout << "Pedidos: " << pedidos.size() << "\n";
    cout << "Combustible: " << combustibles.size() << "\n";
    cout << "Mantenimientos: " << mantenimientos.size() << "\n";
    cout << "Historial: " << historial.size() << "\n";
}

// ---------------------------------------------------------------------------
// [4.1.5] GUARDADO DE LOS SIETE ARCHIVOS - Autor: Juan
// ---------------------------------------------------------------------------

void guardarTodo(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    string directorio = detectarDirectorioDatos();

    // PUNTOS
    {
        ofstream archivo(directorio + "puntos.csv");

        if (archivo.is_open()) {
            archivo << "codigo,nombre,tipo_punto,zona,dias_sin_recolectar\n";

            for (const Punto& p : puntos) {
                archivo << escaparCampoCSV(p.codigo) << ","
                        << escaparCampoCSV(p.nombre) << ","
                        << escaparCampoCSV(p.tipo_punto) << ","
                        << escaparCampoCSV(p.zona) << ","
                        << p.dias_sin_recolectar << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar puntos.csv\n";
        }
    }

    // CONEXIONES
    {
        ofstream archivo(directorio + "conexiones.csv");

        if (archivo.is_open()) {
            archivo << "origen,destino,distancia_km,tiempo_min,estado\n";
            archivo << setprecision(10);

            for (const Conexion& c : conexiones) {
                archivo << escaparCampoCSV(c.origen) << ","
                        << escaparCampoCSV(c.destino) << ","
                        << c.distancia_km << ","
                        << c.tiempo_min << ","
                        << escaparCampoCSV(c.estado) << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar conexiones.csv\n";
        }
    }

    // UNIDADES
    {
        ofstream archivo(directorio + "unidades.csv");

        if (archivo.is_open()) {
            archivo << "codigo,tipo,capacidad_kg,costo_km,estado,especialidad\n";
            archivo << setprecision(10);

            for (const Unidad& u : unidades) {
                archivo << escaparCampoCSV(u.codigo) << ","
                        << escaparCampoCSV(u.tipo) << ","
                        << u.capacidad_kg << ","
                        << u.costo_km << ","
                        << escaparCampoCSV(u.estado) << ","
                        << escaparCampoCSV(u.especialidad) << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar unidades.csv\n";
        }
    }

    // PEDIDOS
    {
        ofstream archivo(directorio + "pedidos.csv");

        if (archivo.is_open()) {
            archivo << "codigo,origen,destino,carga_kg,prioridad,estado,"
                       "unidad_asignada,tipo_residuo\n";
            archivo << setprecision(10);

            for (const Pedido& p : pedidos) {
                archivo << escaparCampoCSV(p.codigo) << ","
                        << escaparCampoCSV(p.origen) << ","
                        << escaparCampoCSV(p.destino) << ","
                        << p.carga_kg << ","
                        << escaparCampoCSV(p.prioridad) << ","
                        << escaparCampoCSV(p.estado) << ","
                        << escaparCampoCSV(p.unidad_asignada) << ","
                        << escaparCampoCSV(p.tipo_residuo) << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar pedidos.csv\n";
        }
    }

    // COMBUSTIBLE
    {
        ofstream archivo(directorio + "combustible.csv");

        if (archivo.is_open()) {
            archivo << "unidad,capacidad_tanque_l,combustible_actual_l,"
                       "consumo_l_km\n";
            archivo << setprecision(10);

            for (const Combustible& c : combustibles) {
                archivo << escaparCampoCSV(c.unidad) << ","
                        << c.capacidad_tanque_l << ","
                        << c.combustible_actual_l << ","
                        << c.consumo_l_km << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar combustible.csv\n";
        }
    }

    // MANTENIMIENTO
    {
        ofstream archivo(directorio + "mantenimiento.csv");

        if (archivo.is_open()) {
            archivo << "unidad,km_actual,km_ultimo_mantenimiento,intervalo_km\n";
            archivo << setprecision(10);

            for (const Mantenimiento& m : mantenimientos) {
                archivo << escaparCampoCSV(m.unidad) << ","
                        << m.km_actual << ","
                        << m.km_ultimo_mantenimiento << ","
                        << m.intervalo_km << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar mantenimiento.csv\n";
        }
    }

    // HISTORIAL DE MANTENIMIENTO
    {
        ofstream archivo(directorio + "historial_mantenimiento.csv");

        if (archivo.is_open()) {
            archivo << "codigo_servicio,unidad,fecha,kilometraje,descripcion\n";
            archivo << setprecision(10);

            for (const HistorialMantenimiento& h : historial) {
                archivo << escaparCampoCSV(h.codigo_servicio) << ","
                        << escaparCampoCSV(h.unidad) << ","
                        << escaparCampoCSV(h.fecha) << ","
                        << h.kilometraje << ","
                        << escaparCampoCSV(h.descripcion) << "\n";
            }
        } else {
            cerr << "[ERROR] No se pudo guardar historial_mantenimiento.csv\n";
        }
    }

    cout << "\n[OK] Los siete archivos CSV fueron guardados.\n";
}


 // ============================================================================
 // 4. Registro de información
 // ============================================================================

 // ---------------------------------------------------------------------------
 // [4.1.3] FUNCIONES DE VALIDACIÓN - Autor: Carlos
 // ---------------------------------------------------------------------------

 // Comprueba si un código de punto ya está registrado.
 bool existePunto(const vector<Punto>& puntos, const string& codigo) {
     for (const Punto& p : puntos) {
         if (p.codigo == codigo) return true;
     }
     return false;
 }

 // Comprueba si un código de unidad ya está registrado.
 bool existeUnidad(const vector<Unidad>& unidades, const string& codigo) {
     for (const Unidad& u : unidades) {
         if (u.codigo == codigo) return true;
     }
     return false;
 }

 // Comprueba si un código de pedido ya está registrado.
 bool existePedido(const vector<Pedido>& pedidos, const string& codigo) {
     for (const Pedido& p : pedidos) {
         if (p.codigo == codigo) return true;
     }
     return false;
 }

 // Comprueba si una opción pertenece a la lista permitida.
 bool opcionValida(const string& valor, const vector<string>& opciones) {
     return find(opciones.begin(), opciones.end(), valor) != opciones.end();
 }

 // Lee una linea; si la entrada se cierra (Ctrl+Z / Ctrl+D) termina el programa.
 void leerLineaOSalir(string& destino) {
     if (!getline(cin, destino)) {
         cout << "\n[AVISO] Entrada finalizada. Saliendo del programa.\n";
         exit(0);
     }
 }

 // Solicita un texto obligatorio.
 string leerTextoRegistro(const string& mensaje) {
     string valor;

     do {
         cout << mensaje;
         leerLineaOSalir(valor);
         valor = limpiarTexto(valor);

         if (valor.empty()) {
             cout << "[ERROR] El campo no puede estar vacio.\n";
         }
     } while (valor.empty());

     return valor;
 }

 // Solicita un número decimal mayor que cero.
 double leerNumeroPositivo(const string& mensaje) {
     string entrada;
     double valor;

     while (true) {
         cout << mensaje;
         leerLineaOSalir(entrada);

         if (parsearDouble(entrada, valor) && valor > 0) {
             return valor;
         }

         cout << "[ERROR] Ingrese un numero mayor que cero.\n";
     }
 }

 // Solicita un numero decimal mayor o igual que cero.
 double leerNumeroNoNegativo(const string& mensaje) {
     string entrada;
     double valor;

     while (true) {
         cout << mensaje;
         leerLineaOSalir(entrada);

         if (parsearDouble(entrada, valor) && valor >= 0) {
             return valor;
         }

         cout << "[ERROR] Ingrese un numero mayor o igual que cero.\n";
     }
 }

 // Solicita una opción perteneciente a una lista permitida.
 string leerOpcionRegistro(
     const string& mensaje,
     const vector<string>& opciones
 ) {
     string valor;

     while (true) {
         cout << mensaje;
         leerLineaOSalir(valor);
         valor = limpiarTexto(valor);

         if (opcionValida(valor, opciones)) {
             return valor;
         }

         cout << "[ERROR] Opcion no valida. Opciones: ";
         for (const string& opcion : opciones) {
             cout << opcion << " ";
         }
         cout << "\n";
     }
 }

 // ---------------------------------------------------------------------------
 // [4.1.3] REGISTRAR PUNTOS - Autor: Carlos
 // ---------------------------------------------------------------------------

 void registrarPunto(
     vector<Punto>& puntos,
     const vector<Conexion>& conexiones,
     const vector<Unidad>& unidades,
     const vector<Pedido>& pedidos,
     const vector<Combustible>& combustibles,
     const vector<Mantenimiento>& mantenimientos,
     const vector<HistorialMantenimiento>& historial
 ) {
     cout << "\n--- REGISTRAR PUNTO ---\n";

     Punto p;

     p.codigo = leerTextoRegistro("Codigo: ");

     if (existePunto(puntos, p.codigo)) {
         cout << "[ERROR] Ya existe un punto con ese codigo.\n";
         return;
     }

     p.nombre = leerTextoRegistro("Nombre: ");
     p.tipo_punto = leerTextoRegistro("Tipo de punto: ");
     p.zona = leerTextoRegistro("Zona: ");

     p.dias_sin_recolectar = 0;

     puntos.push_back(p);

     guardarTodo(
         puntos, conexiones, unidades, pedidos,
         combustibles, mantenimientos, historial
     );

     cout << "[OK] Punto registrado correctamente.\n";
 }

 // ---------------------------------------------------------------------------
 // [4.1.3] REGISTRAR CONEXIONES - Autor: Carlos
 // ---------------------------------------------------------------------------

 void registrarConexion(
     const vector<Punto>& puntos,
     vector<Conexion>& conexiones,
     const vector<Unidad>& unidades,
     const vector<Pedido>& pedidos,
     const vector<Combustible>& combustibles,
     const vector<Mantenimiento>& mantenimientos,
     const vector<HistorialMantenimiento>& historial
 ) {
     cout << "\n--- REGISTRAR CONEXION ---\n";

     if (puntos.size() < 2) {
         cout << "[ERROR] Se necesitan al menos dos puntos registrados.\n";
         return;
     }

     Conexion c;

     c.origen = leerTextoRegistro("Codigo del punto de origen: ");

     if (!existePunto(puntos, c.origen)) {
         cout << "[ERROR] El punto de origen no existe.\n";
         return;
     }

     c.destino = leerTextoRegistro("Codigo del punto de destino: ");

     if (!existePunto(puntos, c.destino)) {
         cout << "[ERROR] El punto de destino no existe.\n";
         return;
     }

     if (c.origen == c.destino) {
         cout << "[ERROR] El origen y destino deben ser diferentes.\n";
         return;
     }

     // Cada tramo se registra una sola vez, aunque sea bidireccional.
     for (const Conexion& existente : conexiones) {
         if ((existente.origen == c.origen &&
              existente.destino == c.destino) ||
             (existente.origen == c.destino &&
              existente.destino == c.origen)) {
             cout << "[ERROR] Esa conexion ya esta registrada.\n";
             return;
         }
     }

     c.distancia_km = leerNumeroPositivo("Distancia en km: ");

     double tiempo = leerNumeroPositivo("Tiempo en minutos: ");

     if (tiempo > 2147483647.0 || tiempo != static_cast<int>(tiempo)) {
         cout << "[ERROR] El tiempo debe ser un numero entero valido.\n";
         return;
     }

     c.tiempo_min = static_cast<int>(tiempo);

     c.estado = leerOpcionRegistro(
         "Estado (habilitada/cerrada): ",
         {"habilitada", "cerrada"}
     );

     conexiones.push_back(c);

     guardarTodo(
         puntos, conexiones, unidades, pedidos,
         combustibles, mantenimientos, historial
     );

     cout << "[OK] Conexion registrada correctamente.\n";
 }

 // ---------------------------------------------------------------------------
 // [4.1.3] REGISTRAR UNIDADES - Autor: Carlos
 // ---------------------------------------------------------------------------

 void registrarUnidad(
     const vector<Punto>& puntos,
     const vector<Conexion>& conexiones,
     vector<Unidad>& unidades,
     const vector<Pedido>& pedidos,
     const vector<Combustible>& combustibles,
     const vector<Mantenimiento>& mantenimientos,
     const vector<HistorialMantenimiento>& historial
 ) {
     cout << "\n--- REGISTRAR UNIDAD ---\n";

     Unidad u;

     u.codigo = leerTextoRegistro("Codigo de unidad: ");

     if (existeUnidad(unidades, u.codigo)) {
         cout << "[ERROR] Ya existe una unidad con ese codigo.\n";
         return;
     }

     u.tipo = leerTextoRegistro("Tipo de unidad: ");
     u.capacidad_kg = leerNumeroPositivo("Capacidad en kg: ");
     u.costo_km = leerNumeroPositivo("Costo por km: ");

     u.estado = leerOpcionRegistro(
         "Estado (disponible/ocupada): ",
         {"disponible", "ocupada"}
     );

     u.especialidad = leerOpcionRegistro(
         "Especialidad (domiciliario/reciclable/desmonte): ",
         {"domiciliario", "reciclable", "desmonte"}
     );

     unidades.push_back(u);

     guardarTodo(
         puntos, conexiones, unidades, pedidos,
         combustibles, mantenimientos, historial
     );

     cout << "[OK] Unidad registrada correctamente.\n";
 }


 // ---------------------------------------------------------------------------
 // [4.1.3] REGISTRAR PEDIDOS - Autor: Carlos
 // ---------------------------------------------------------------------------

void registrarPedido(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- REGISTRAR PEDIDO ---\n";

    if (puntos.size() < 2) {
        cout << "[ERROR] Se necesitan al menos dos puntos registrados.\n";
        return;
    }

    Pedido p;

    p.codigo = leerTextoRegistro("Codigo de pedido: ");

    if (existePedido(pedidos, p.codigo)) {
        cout << "[ERROR] Ya existe un pedido con ese codigo.\n";
        return;
    }

    p.origen = leerTextoRegistro("Punto de origen: ");

    if (!existePunto(puntos, p.origen)) {
        cout << "[ERROR] El punto de origen no existe.\n";
        return;
    }

    p.destino = leerTextoRegistro("Punto de destino: ");

    if (!existePunto(puntos, p.destino)) {
        cout << "[ERROR] El punto de destino no existe.\n";
        return;
    }

    if (p.origen == p.destino) {
        cout << "[ERROR] El origen y destino deben ser diferentes.\n";
        return;
    }

    p.carga_kg = leerNumeroPositivo("Carga en kg: ");

    p.prioridad = leerOpcionRegistro(
        "Prioridad (alta/media/baja): ",
        {"alta", "media", "baja"}
    );

    p.tipo_residuo = leerOpcionRegistro(
        "Tipo de residuo (domiciliario/reciclable/desmonte): ",
        {"domiciliario", "reciclable", "desmonte"}
    );

    // Todo pedido nuevo comienza pendiente.
    // La asignacion de unidades se realiza desde Gestion de estados.
    p.estado = "pendiente";
    p.unidad_asignada = "";

    pedidos.push_back(p);

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Pedido registrado correctamente.\n";
    cout << "[INFO] El pedido quedo pendiente de asignacion.\n";
}
 

 // ============================================================================
 // 5. Gestión de estados
 // ============================================================================

 // ---------------------------------------------------------------------------
 // [4.1.4] GESTIÓN DE ESTADOS - Autor: Jason
 // ---------------------------------------------------------------------------

 // Cierra o habilita una conexión existente.
 void gestionarConexion(
     vector<Punto>& puntos,
     vector<Conexion>& conexiones,
     vector<Unidad>& unidades,
     vector<Pedido>& pedidos,
     vector<Combustible>& combustibles,
     vector<Mantenimiento>& mantenimientos,
     vector<HistorialMantenimiento>& historial
 ) {
     string origen = leerTextoRegistro("Codigo del punto de origen: ");
     string destino = leerTextoRegistro("Codigo del punto de destino: ");

     auto conexion = find_if(conexiones.begin(), conexiones.end(),
         [&](const Conexion& c) {
             return (c.origen == origen && c.destino == destino) ||
                    (c.origen == destino && c.destino == origen);
         });

     if (conexion == conexiones.end()) {
         cout << "[ERROR] La conexion no existe.\n";
         return;
     }

     string nuevoEstado = leerOpcionRegistro(
         "Seleccione el estado (cerrar/habilitar): ",
         {"cerrar", "habilitar"}
     );

     if (nuevoEstado == "cerrar") {
         if (conexion->estado == "cerrada") {
             cout << "[AVISO] La conexion ya esta cerrada.\n";
             return;
         }

         conexion->estado = "cerrada";
     }
     else {
         if (conexion->estado == "habilitada") {
             cout << "[AVISO] La conexion ya esta habilitada.\n";
             return;
         }

         conexion->estado = "habilitada";
     }

     guardarTodo(puntos, conexiones, unidades, pedidos,
                 combustibles, mantenimientos, historial);

     cout << "[OK] Estado de la conexion actualizado.\n";
 }


 // Normaliza un texto para comparar especialidades y tipos de residuo.
string normalizarEspecialidad(const string& texto) {
    string resultado = limpiarTexto(texto);

    transform(
        resultado.begin(),
        resultado.end(),
        resultado.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        }
    );

    return resultado;
}


 
 // Asigna un pedido pendiente a una unidad disponible y compatible.
void asignarPedido(
    vector<Punto>& puntos,
    vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial
) {
    string codigoPedido = leerTextoRegistro("Codigo del pedido: ");

    auto pedido = find_if(pedidos.begin(), pedidos.end(),
        [&](const Pedido& p) {
            return p.codigo == codigoPedido;
        });

    if (pedido == pedidos.end()) {
        cout << "[ERROR] El pedido no existe.\n";
        return;
    }

    if (pedido->estado != "pendiente") {
        cout << "[ERROR] Solo se pueden asignar pedidos pendientes.\n";
        return;
    }

    string codigoUnidad = leerTextoRegistro("Codigo de la unidad: ");

    auto unidad = find_if(unidades.begin(), unidades.end(),
        [&](const Unidad& u) {
            return u.codigo == codigoUnidad;
        });

    if (unidad == unidades.end()) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    if (unidad->estado != "disponible") {
        cout << "[ERROR] La unidad no esta disponible.\n";
        return;
    }

    if (unidad->capacidad_kg < pedido->carga_kg) {
        cout << "[ERROR] La capacidad de la unidad es insuficiente.\n";
        return;
    }

    // Validar que la unidad pueda transportar el tipo de residuo.
    if (normalizarEspecialidad(unidad->especialidad).empty() ||
        normalizarEspecialidad(pedido->tipo_residuo).empty()) {
        cout << "[ERROR] Falta registrar la especialidad de la unidad "
             << "o el tipo de residuo del pedido.\n";
        return;
    }

    if (normalizarEspecialidad(unidad->especialidad) !=
        normalizarEspecialidad(pedido->tipo_residuo)) {
        cout << "[ERROR] La especialidad de la unidad no es compatible "
             << "con el tipo de residuo del pedido.\n";
        cout << "Tipo de residuo: " << pedido->tipo_residuo << "\n";
        cout << "Especialidad de la unidad: " << unidad->especialidad << "\n";
        return;
    }

    pedido->unidad_asignada = unidad->codigo;
    pedido->estado = "asignado";
    unidad->estado = "ocupada";

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Pedido asignado correctamente.\n";
}


 // Marca un pedido asignado como entregado y libera la unidad utilizada.
 void marcarPedidoEntregado(
     vector<Punto>& puntos,
     vector<Conexion>& conexiones,
     vector<Unidad>& unidades,
     vector<Pedido>& pedidos,
     vector<Combustible>& combustibles,
     vector<Mantenimiento>& mantenimientos,
     vector<HistorialMantenimiento>& historial
 ) {
     string codigoPedido = leerTextoRegistro("Codigo del pedido: ");

     auto pedido = find_if(pedidos.begin(), pedidos.end(),
         [&](const Pedido& p) {
             return p.codigo == codigoPedido;
         });

     if (pedido == pedidos.end()) {
         cout << "[ERROR] El pedido no existe.\n";
         return;
     }

     if (pedido->estado != "asignado") {
         cout << "[ERROR] Solo se pueden entregar pedidos asignados.\n";
         return;
     }

     auto unidad = find_if(unidades.begin(), unidades.end(),
         [&](const Unidad& u) {
             return u.codigo == pedido->unidad_asignada;
         });

     if (unidad == unidades.end()) {
         cout << "[ERROR] No se encontro la unidad asignada al pedido.\n";
         return;
     }

     pedido->estado = "entregado";
     unidad->estado = "disponible";

     // FA - Autor: Diana: al recolectar, el punto de origen reinicia su contador.
     auto puntoOrigen = find_if(puntos.begin(), puntos.end(),
         [&](const Punto& pt) {
             return pt.codigo == pedido->origen;
         });

     if (puntoOrigen != puntos.end()) {
         puntoOrigen->dias_sin_recolectar = 0;
     }

     guardarTodo(puntos, conexiones, unidades, pedidos,
                 combustibles, mantenimientos, historial);

     cout << "[OK] Pedido entregado. Unidad liberada correctamente.\n";
 }



// ============================================================================
// 6. Consultas básicas
// ============================================================================

// ---------------------------------------------------------------------------
// [4.1.6] CONSULTAS BÁSICAS - Autor: Diana
// ---------------------------------------------------------------------------

// Muestra todos los puntos registrados.
void consultarPuntos(const vector<Punto>& puntos) {

    cout << "\n--- PUNTOS REGISTRADOS ---\n";

    if (puntos.empty()) {
        cout << "[AVISO] No hay puntos registrados.\n";
        return;
    }

    // Los anchos se ajustan al texto mas largo de cada columna.
    size_t anchoCodigo = string("CODIGO").size();
    size_t anchoNombre = string("NOMBRE").size();
    size_t anchoTipo = string("TIPO").size();
    size_t anchoZona = string("ZONA").size();

    for (const Punto& p : puntos) {
        anchoCodigo = max(anchoCodigo, p.codigo.size());
        anchoNombre = max(anchoNombre, p.nombre.size());
        anchoTipo = max(anchoTipo, p.tipo_punto.size());
        anchoZona = max(anchoZona, p.zona.size());
    }

    anchoCodigo += 3;
    anchoNombre += 3;
    anchoTipo += 3;
    anchoZona += 3;

    cout << left
         << setw(anchoCodigo) << "CODIGO"
         << setw(anchoNombre) << "NOMBRE"
         << setw(anchoTipo) << "TIPO"
         << setw(anchoZona) << "ZONA" << "\n";

    cout << string(anchoCodigo + anchoNombre + anchoTipo + anchoZona, '-')
         << "\n";

    for (const Punto& p : puntos) {
        cout << left
             << setw(anchoCodigo) << p.codigo
             << setw(anchoNombre) << p.nombre
             << setw(anchoTipo) << p.tipo_punto
             << setw(anchoZona) << p.zona << "\n";
    }

    cout << "\nTotal de puntos: " << puntos.size() << "\n";
}


// Muestra las conexiones de un punto en ambos sentidos.
void consultarConexionesPunto(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones
) {

    cout << "\n--- CONEXIONES DE UN PUNTO ---\n";

    string codigo = leerTextoRegistro("Codigo del punto: ");

    if (!existePunto(puntos, codigo)) {
        cout << "[ERROR] El punto no existe.\n";
        return;
    }

    bool encontrado = false;

    cout << "\nConexiones del punto " << codigo << ":\n\n";

    cout << left
         << setw(15) << "ORIGEN"
         << setw(15) << "DESTINO"
         << setw(15) << "DISTANCIA"
         << setw(15) << "TIEMPO"
         << setw(15) << "ESTADO" << "\n";

    cout << string(75, '-') << "\n";

    for (const Conexion& c : conexiones) {

        if (c.origen == codigo || c.destino == codigo) {

            encontrado = true;

            cout << left
                 << setw(15) << c.origen
                 << setw(15) << c.destino
                 << setw(15) << c.distancia_km
                 << setw(15) << c.tiempo_min
                 << setw(15) << c.estado << "\n";
        }
    }

    if (!encontrado) {
        cout << "[AVISO] El punto no tiene conexiones registradas.\n";
    }
}


// Muestra los pedidos según su estado.
void consultarPedidosPorEstado(const vector<Pedido>& pedidos) {

    cout << "\n--- PEDIDOS POR ESTADO ---\n";

    string estado = leerOpcionRegistro(
        "Seleccione el estado (pendiente/asignado/entregado): ",
        {"pendiente", "asignado", "entregado"}
    );

    bool encontrado = false;
    int cantidad = 0;

    cout << "\nPedidos con estado: " << estado << "\n\n";

    cout << left
         << setw(12) << "CODIGO"
         << setw(12) << "ORIGEN"
         << setw(12) << "DESTINO"
         << setw(15) << "CARGA (KG)"
         << setw(12) << "PRIORIDAD"
         << setw(15) << "UNIDAD" << "\n";

    cout << string(78, '-') << "\n";

    for (const Pedido& p : pedidos) {

        if (p.estado == estado) {

            encontrado = true;
            cantidad++;

            cout << left
                 << setw(12) << p.codigo
                 << setw(12) << p.origen
                 << setw(12) << p.destino
                 << setw(15) << p.carga_kg
                 << setw(12) << p.prioridad
                 << setw(15) << (
                     p.unidad_asignada.empty()
                     ? "Sin asignar"
                     : p.unidad_asignada
                 ) << "\n";
        }
    }

    if (!encontrado) {
        cout << "[AVISO] No hay pedidos con ese estado.\n";
    }

    cout << "\nTotal de pedidos: " << cantidad << "\n";
}


// Muestra los pedidos relacionados con una unidad.
void consultarPedidosPorUnidad(
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos
) {

    cout << "\n--- PEDIDOS POR UNIDAD ---\n";

    string codigo = leerTextoRegistro("Codigo de la unidad: ");

    if (!existeUnidad(unidades, codigo)) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    bool encontrado = false;
    int cantidad = 0;

    cout << "\nPedidos de la unidad " << codigo << ":\n\n";

    cout << left
         << setw(12) << "CODIGO"
         << setw(12) << "ORIGEN"
         << setw(12) << "DESTINO"
         << setw(15) << "CARGA (KG)"
         << setw(12) << "PRIORIDAD"
         << setw(15) << "ESTADO" << "\n";

    cout << string(78, '-') << "\n";

    for (const Pedido& p : pedidos) {

        if (p.unidad_asignada == codigo) {

            encontrado = true;
            cantidad++;

            cout << left
                 << setw(12) << p.codigo
                 << setw(12) << p.origen
                 << setw(12) << p.destino
                 << setw(15) << p.carga_kg
                 << setw(12) << p.prioridad
                 << setw(15) << p.estado << "\n";
        }
    }

    if (!encontrado) {
        cout << "[AVISO] La unidad no tiene pedidos relacionados.\n";
    }

    cout << "\nTotal de pedidos: " << cantidad << "\n";
}


// Menú para acceder a las cuatro consultas básicas.
void menuConsultas(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos
) {

    string opcion;

    do {

        cout << "\n====================================\n";
        cout << "       CONSULTAS BASICAS\n";
        cout << "====================================\n";
        cout << "1. Mostrar puntos registrados\n";
        cout << "2. Consultar conexiones de un punto\n";
        cout << "3. Consultar pedidos por estado\n";
        cout << "4. Consultar pedidos por unidad\n";
        cout << "0. Volver al menu principal\n";
        cout << "====================================\n";

        opcion = leerOpcionRegistro(
            "Seleccione una opcion: ",
            {"0", "1", "2", "3", "4"}
        );

        if (opcion == "1") {
            consultarPuntos(puntos);
        }
        else if (opcion == "2") {
            consultarConexionesPunto(puntos, conexiones);
        }
        else if (opcion == "3") {
            consultarPedidosPorEstado(pedidos);
        }
        else if (opcion == "4") {
            consultarPedidosPorUnidad(unidades, pedidos);
        }

    } while (opcion != "0");
}



// ============================================================================
// 7. Funcionalidades adicionales
// ============================================================================

// ---------------------------------------------------------------------------
// FA - Autor: Diana
// PRIORIZACION AUTOMATICA DE PUNTOS POR RIESGO SANITARIO
//
// Cada tipo de punto tiene un maximo de dias permitidos sin recojo:
//   mercado = 2 | residencial = 3 | comercial = 4 | recreativo = 4
// Los centros de acopio y plantas no se evaluan (no generan residuos).
//   dias > maximo  -> Riesgo ALTO  (pedidos a prioridad alta + camion sugerido)
//   dias = maximo  -> Riesgo MEDIO (pedidos suben al menos a prioridad media)
//   dias < maximo  -> Riesgo BAJO
// ---------------------------------------------------------------------------

// Dias maximos sin recojo segun el tipo de punto (-1 = no se evalua).
int umbralDiasPorTipo(const string& tipoPunto) {
    string tipo = normalizarEspecialidad(tipoPunto);

    if (tipo == "mercado") return 2;
    if (tipo == "residencial") return 3;
    if (tipo == "comercial") return 4;
    if (tipo == "recreativo") return 4;

    return -1;
}

// Clasifica el riesgo sanitario de un punto.
string nivelRiesgoSanitario(int dias, int umbral) {
    if (umbral < 0) return "";
    if (dias > umbral) return "alto";
    if (dias == umbral) return "medio";
    return "bajo";
}

int pesoNivelRiesgo(const string& nivel) {
    if (nivel == "alto") return 3;
    if (nivel == "medio") return 2;
    if (nivel == "bajo") return 1;
    return 0;
}

struct RiesgoPunto {
    string codigo;
    string nombre;
    string tipo;
    string nivel;
    int dias = 0;
    int umbral = 0;
};

// Calcula el riesgo de cada punto de recojo, del mas critico al menos critico.
vector<RiesgoPunto> calcularRiesgos(const vector<Punto>& puntos) {
    vector<RiesgoPunto> riesgos;

    for (const Punto& p : puntos) {
        int umbral = umbralDiasPorTipo(p.tipo_punto);
        if (umbral < 0) continue;

        RiesgoPunto r;
        r.codigo = p.codigo;
        r.nombre = p.nombre;
        r.tipo = p.tipo_punto;
        r.dias = p.dias_sin_recolectar;
        r.umbral = umbral;
        r.nivel = nivelRiesgoSanitario(r.dias, umbral);
        riesgos.push_back(r);
    }

    sort(riesgos.begin(), riesgos.end(),
        [](const RiesgoPunto& a, const RiesgoPunto& b) {
            if (pesoNivelRiesgo(a.nivel) != pesoNivelRiesgo(b.nivel)) {
                return pesoNivelRiesgo(a.nivel) > pesoNivelRiesgo(b.nivel);
            }
            if ((a.dias - a.umbral) != (b.dias - b.umbral)) {
                return (a.dias - a.umbral) > (b.dias - b.umbral);
            }
            return a.codigo < b.codigo;
        });

    return riesgos;
}

// Simula el paso de dias: todos los puntos de recojo acumulan dias sin recojo.
void avanzarDiasSinRecolectar(
    vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- AVANZAR DIAS SIN RECOLECTAR ---\n";

    double dias = leerNumeroPositivo("Cantidad de dias a avanzar: ");

    if (dias != static_cast<int>(dias)) {
        cout << "[ERROR] Ingrese un numero entero de dias.\n";
        return;
    }

    int actualizados = 0;

    for (Punto& p : puntos) {
        if (umbralDiasPorTipo(p.tipo_punto) < 0) continue;

        p.dias_sin_recolectar += static_cast<int>(dias);
        actualizados++;
    }

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Se sumaron " << static_cast<int>(dias)
         << " dia(s) a " << actualizados << " puntos de recojo.\n";
}

// Evalua el riesgo, prioriza los pedidos y sugiere (o asigna) camiones.
void evaluarRiesgoSanitario(
    vector<Punto>& puntos,
    vector<Pedido>& pedidos,
    const vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    const vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- EVALUACION DE RIESGO SANITARIO ---\n";

    vector<RiesgoPunto> riesgos = calcularRiesgos(puntos);

    if (riesgos.empty()) {
        cout << "[AVISO] No hay puntos de recojo para evaluar.\n";
        return;
    }

    // 1. Ranking de puntos, del mas critico al menos critico.
    cout << "\nPuntos ordenados por riesgo:\n";

    for (const RiesgoPunto& r : riesgos) {
        string nivel = r.nivel;
        transform(nivel.begin(), nivel.end(), nivel.begin(),
            [](unsigned char c) { return static_cast<char>(toupper(c)); });

        cout << "  " << r.codigo << " - " << r.nombre
             << " (" << r.tipo << ")"
             << " | dias sin recojo: " << r.dias
             << " (maximo " << r.umbral << ")"
             << " | Riesgo " << nivel << "\n";
    }

    auto buscarRiesgo = [&](const string& codigo) -> const RiesgoPunto* {
        for (const RiesgoPunto& r : riesgos) {
            if (r.codigo == codigo) return &r;
        }
        return nullptr;
    };

    int cambios = 0;

    // 2. Subir la prioridad de los pedidos pendientes (nunca se baja).
    cout << "\nPrioridad de pedidos pendientes:\n";

    bool huboPendientes = false;

    for (Pedido& p : pedidos) {
        if (p.estado != "pendiente") continue;

        const RiesgoPunto* r = buscarRiesgo(p.origen);
        if (r == nullptr) continue;

        huboPendientes = true;

        string nueva = p.prioridad;

        if (r->nivel == "alto") {
            nueva = "alta";
        }
        else if (r->nivel == "medio" && p.prioridad == "baja") {
            nueva = "media";
        }

        cout << "  " << p.codigo << " (origen " << p.origen
             << ", riesgo " << r->nivel << "): prioridad " << p.prioridad;

        if (nueva != p.prioridad) {
            cout << " -> " << nueva << " [ACTUALIZADA]";
            p.prioridad = nueva;
            cambios++;
        }

        cout << "\n";
    }

    if (!huboPendientes) {
        cout << "  No hay pedidos pendientes en puntos de recojo.\n";
    }

    // 3. Sugerir camiones para los puntos de riesgo alto.
    struct Sugerencia {
        string pedido;
        string unidad;
    };

    vector<Sugerencia> sugerencias;
    set<string> reservadas;

    cout << "\nAtencion inmediata (riesgo ALTO):\n";

    bool huboAltos = false;

    for (const RiesgoPunto& r : riesgos) {
        if (r.nivel != "alto") continue;

        for (const Pedido& p : pedidos) {
            if (p.estado != "pendiente" || p.origen != r.codigo) continue;

            huboAltos = true;

            const Unidad* mejor = nullptr;

            for (const Unidad& u : unidades) {
                if (u.estado != "disponible") continue;
                if (reservadas.count(u.codigo) > 0) continue;
                if (u.capacidad_kg < p.carga_kg) continue;
                if (normalizarEspecialidad(u.especialidad) !=
                    normalizarEspecialidad(p.tipo_residuo)) continue;

                if (mejor == nullptr || u.costo_km < mejor->costo_km) {
                    mejor = &u;
                }
            }

            cout << "  " << r.codigo << " (" << r.dias << " dias sin recojo), "
                 << "pedido " << p.codigo << " (" << p.tipo_residuo
                 << ", " << p.carga_kg << " kg): ";

            if (mejor != nullptr) {
                cout << "asignar camion " << mejor->codigo << " de inmediato.\n";
                reservadas.insert(mejor->codigo);
                sugerencias.push_back({p.codigo, mejor->codigo});
            }
            else {
                cout << "sin camion compatible disponible.\n";
            }
        }
    }

    if (!huboAltos) {
        cout << "  No hay pedidos pendientes en puntos de riesgo alto.\n";
    }

    // 4. Asignacion automatica (con confirmacion).
    if (!sugerencias.empty()) {
        string respuesta = leerOpcionRegistro(
            "\nAsignar automaticamente estos camiones? (si/no): ",
            {"si", "no"}
        );

        if (respuesta == "si") {
            for (const Sugerencia& s : sugerencias) {
                for (Pedido& p : pedidos) {
                    if (p.codigo != s.pedido) continue;

                    for (Unidad& u : unidades) {
                        if (u.codigo != s.unidad) continue;

                        p.estado = "asignado";
                        p.unidad_asignada = u.codigo;
                        u.estado = "ocupada";
                        cambios++;

                        cout << "[OK] " << p.codigo << " asignado a "
                             << u.codigo << ".\n";
                    }
                }
            }
        }
    }

    if (cambios > 0) {
        guardarTodo(
            puntos, conexiones, unidades, pedidos,
            combustibles, mantenimientos, historial
        );
    }

    cout << "\nEvaluacion terminada. Cambios realizados: " << cambios << "\n";
}


// ---------------------------------------------------------------------------
// REGISTRO Y CONTROL DE COMBUSTIBLE - Autor: Carlos
// ---------------------------------------------------------------------------

// Registra los datos iniciales de combustible de una unidad.
void registrarCombustible(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- REGISTRAR CONTROL DE COMBUSTIBLE ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    if (!existeUnidad(unidades, codigo)) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    auto existente = find_if(
        combustibles.begin(), combustibles.end(),
        [&](const Combustible& c) {
            return c.unidad == codigo;
        }
    );

    if (existente != combustibles.end()) {
        cout << "[ERROR] La unidad ya tiene un registro de combustible.\n";
        return;
    }

    Combustible c;
    c.unidad = codigo;
    c.capacidad_tanque_l = leerNumeroPositivo("Capacidad del tanque (L): ");

    string entrada;
    double actual;

    while (true) {
        cout << "Combustible actual (L): ";
        leerLineaOSalir(entrada);

        if (parsearDouble(entrada, actual) &&
            actual >= 0 &&
            actual <= c.capacidad_tanque_l) {
            break;
        }

        cout << "[ERROR] Ingrese un valor entre 0 y la capacidad del tanque.\n";
    }

    c.combustible_actual_l = actual;
    c.consumo_l_km = leerNumeroPositivo("Consumo por kilometro (L/km): ");

    combustibles.push_back(c);

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Control de combustible registrado.\n";
}


// Registra el abastecimiento de combustible.
void abastecerCombustible(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- ABASTECER COMBUSTIBLE ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    auto c = find_if(
        combustibles.begin(), combustibles.end(),
        [&](const Combustible& combustible) {
            return combustible.unidad == codigo;
        }
    );

    if (c == combustibles.end()) {
        cout << "[ERROR] La unidad no tiene control de combustible.\n";
        return;
    }

    double litros = leerNumeroPositivo("Litros a abastecer: ");

    if (c->combustible_actual_l + litros > c->capacidad_tanque_l) {
        cout << "[ERROR] El abastecimiento supera la capacidad del tanque.\n";
        return;
    }

    c->combustible_actual_l += litros;

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Abastecimiento registrado.\n";
    cout << "Combustible actual: " << c->combustible_actual_l << " L\n";
}


// Descuenta el combustible consumido según la distancia recorrida.
void registrarConsumoCombustible(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- REGISTRAR CONSUMO DE COMBUSTIBLE ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    auto c = find_if(
        combustibles.begin(), combustibles.end(),
        [&](const Combustible& combustible) {
            return combustible.unidad == codigo;
        }
    );

    if (c == combustibles.end()) {
        cout << "[ERROR] No existe control de combustible para esa unidad.\n";
        return;
    }

    double distancia = leerNumeroPositivo("Distancia recorrida (km): ");
    double consumo = distancia * c->consumo_l_km;

    if (consumo > c->combustible_actual_l) {
        cout << "[ERROR] Combustible insuficiente para ese recorrido.\n";
        cout << "Consumo requerido: " << consumo << " L\n";
        return;
    }

    c->combustible_actual_l -= consumo;

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Consumo registrado.\n";
    cout << "Combustible consumido: " << consumo << " L\n";
    cout << "Combustible restante: " << c->combustible_actual_l << " L\n";
}


// Calcula la distancia que puede recorrer una unidad con su combustible actual.
void consultarAutonomia(const vector<Combustible>& combustibles) {
    cout << "\n--- CONSULTAR AUTONOMIA ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    auto c = find_if(
        combustibles.begin(), combustibles.end(),
        [&](const Combustible& combustible) {
            return combustible.unidad == codigo;
        }
    );

    if (c == combustibles.end()) {
        cout << "[ERROR] No existe control de combustible para esa unidad.\n";
        return;
    }

    if (c->consumo_l_km <= 0) {
        cout << "[ERROR] El consumo por kilometro debe ser mayor que cero.\n";
        return;
    }

    double autonomia =
        c->combustible_actual_l / c->consumo_l_km;

    cout << "Unidad: " << c->unidad << "\n";
    cout << "Combustible disponible: "
         << c->combustible_actual_l << " L\n";
    cout << "Autonomia estimada: " << autonomia << " km\n";
}


// ---------------------------------------------------------------------------
// MANTENIMIENTO PREVENTIVO E HISTORIAL - Autor: Jason
// ---------------------------------------------------------------------------

// Registra el control inicial de mantenimiento de una unidad.
void registrarControlMantenimiento(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- REGISTRAR CONTROL DE MANTENIMIENTO ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    if (!existeUnidad(unidades, codigo)) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    auto existente = find_if(
        mantenimientos.begin(), mantenimientos.end(),
        [&](const Mantenimiento& m) {
            return m.unidad == codigo;
        }
    );

    if (existente != mantenimientos.end()) {
        cout << "[ERROR] La unidad ya tiene un control de mantenimiento.\n";
        return;
    }

    Mantenimiento m;
    m.unidad = codigo;
    m.km_actual = leerNumeroNoNegativo("Kilometraje actual: ");

    while (true) {
        m.km_ultimo_mantenimiento = leerNumeroNoNegativo(
            "Kilometraje del ultimo mantenimiento: "
        );

        if (m.km_ultimo_mantenimiento <= m.km_actual) break;

        cout << "[ERROR] El ultimo mantenimiento no puede superar "
                "el kilometraje actual.\n";
    }

    m.intervalo_km = leerNumeroPositivo("Intervalo de mantenimiento (km): ");

    mantenimientos.push_back(m);

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Control inicial de mantenimiento registrado.\n";
}


// Consulta si una unidad necesita mantenimiento preventivo.
void consultarMantenimientoPreventivo(
    const vector<Unidad>& unidades,
    vector<Mantenimiento>& mantenimientos
) {
    cout << "\n--- MANTENIMIENTO PREVENTIVO ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    if (!existeUnidad(unidades, codigo)) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    auto m = find_if(
        mantenimientos.begin(), mantenimientos.end(),
        [&](const Mantenimiento& mantenimiento) {
            return mantenimiento.unidad == codigo;
        }
    );

    if (m == mantenimientos.end()) {
        cout << "[AVISO] La unidad no tiene control de mantenimiento.\n";
        return;
    }

    double kmRecorridos =
        m->km_actual - m->km_ultimo_mantenimiento;

    double restantes = m->intervalo_km - kmRecorridos;

    cout << "Kilometraje actual: " << m->km_actual << " km\n";
    cout << "Ultimo mantenimiento: "
         << m->km_ultimo_mantenimiento << " km\n";
    cout << "Intervalo: " << m->intervalo_km << " km\n";

    if (restantes <= 0) {
        cout << "[ALERTA] Mantenimiento pendiente.\n";
    }
    else {
        cout << "Kilometros restantes para mantenimiento: "
             << restantes << " km\n";
    }
}


// Registra un servicio y actualiza el control preventivo.
void registrarServicioMantenimiento(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- REGISTRAR SERVICIO DE MANTENIMIENTO ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    if (!existeUnidad(unidades, codigo)) {
        cout << "[ERROR] La unidad no existe.\n";
        return;
    }

    auto m = find_if(
        mantenimientos.begin(), mantenimientos.end(),
        [&](const Mantenimiento& mantenimiento) {
            return mantenimiento.unidad == codigo;
        }
    );

    if (m == mantenimientos.end()) {
        cout << "[ERROR] La unidad no tiene control de mantenimiento.\n";
        return;
    }

    double kilometraje = leerNumeroPositivo("Kilometraje del servicio: ");

    if (kilometraje < m->km_actual) {
        cout << "[ERROR] El kilometraje no puede ser menor al registrado.\n";
        return;
    }

    HistorialMantenimiento h;

    h.codigo_servicio = leerTextoRegistro("Codigo de servicio: ");

    bool servicioRepetido = any_of(
        historial.begin(), historial.end(),
        [&](const HistorialMantenimiento& x) {
            return x.codigo_servicio == h.codigo_servicio;
        }
    );

    if (servicioRepetido) {
        cout << "[ERROR] Ya existe un servicio con ese codigo.\n";
        return;
    }

    h.unidad = codigo;
    h.fecha = leerTextoRegistro("Fecha del servicio: ");
    h.kilometraje = kilometraje;
    h.descripcion = leerTextoRegistro("Descripcion del servicio: ");

    m->km_actual = kilometraje;
    m->km_ultimo_mantenimiento = kilometraje;

    historial.push_back(h);

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Servicio registrado y mantenimiento actualizado.\n";
}

// ---------------------------------------------------------------------------
// ACTUALIZACION DE KILOMETRAJE - Autor: Jason
// ---------------------------------------------------------------------------

// Actualiza el kilometraje actual de una unidad con control de mantenimiento.
void actualizarKilometraje(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- ACTUALIZAR KILOMETRAJE ---\n";

    string codigo = leerTextoRegistro("Codigo de unidad: ");

    auto m = find_if(
        mantenimientos.begin(), mantenimientos.end(),
        [&](const Mantenimiento& mantenimiento) {
            return mantenimiento.unidad == codigo;
        }
    );

    if (m == mantenimientos.end()) {
        cout << "[ERROR] La unidad no tiene control de mantenimiento.\n";
        return;
    }

    double nuevoKm = leerNumeroPositivo("Nuevo kilometraje actual: ");

    if (nuevoKm < m->km_actual) {
        cout << "[ERROR] El kilometraje no puede ser menor al registrado ("
             << m->km_actual << " km).\n";
        return;
    }

    m->km_actual = nuevoKm;

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Kilometraje actualizado.\n";
}


// ---------------------------------------------------------------------------
// ACTUALIZACION DE DIAS SIN RECOLECTAR - Autor: Diana
// ---------------------------------------------------------------------------

// Registra cuantos dias lleva un punto sin recojo de residuos.
void actualizarDiasSinRecolectar(
    vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos,
    const vector<Combustible>& combustibles,
    const vector<Mantenimiento>& mantenimientos,
    const vector<HistorialMantenimiento>& historial
) {
    cout << "\n--- ACTUALIZAR DIAS SIN RECOLECTAR ---\n";

    string codigo = leerTextoRegistro("Codigo del punto: ");

    auto punto = find_if(
        puntos.begin(), puntos.end(),
        [&](const Punto& p) {
            return p.codigo == codigo;
        }
    );

    if (punto == puntos.end()) {
        cout << "[ERROR] El punto no existe.\n";
        return;
    }

    double dias = leerNumeroNoNegativo("Dias sin recolectar: ");

    if (dias != static_cast<int>(dias)) {
        cout << "[ERROR] Ingrese un numero entero de dias.\n";
        return;
    }

    punto->dias_sin_recolectar = static_cast<int>(dias);

    guardarTodo(
        puntos, conexiones, unidades, pedidos,
        combustibles, mantenimientos, historial
    );

    cout << "[OK] Dias sin recolectar actualizados.\n";
}


// ============================================================================
// 8. Reporte general
// ============================================================================

// ---------------------------------------------------------------------------
// [4.1.7] REPORTE GENERAL - Autor: Juan
// ---------------------------------------------------------------------------

// Muestra un resumen del estado del sistema.
void reporteGeneral(
    const vector<Punto>& puntos,
    const vector<Conexion>& conexiones,
    const vector<Unidad>& unidades,
    const vector<Pedido>& pedidos
) {
    int habilitadas = 0, cerradas = 0;
    int disponibles = 0;
    int pendientes = 0, asignados = 0, entregados = 0;

    for (const Conexion& c : conexiones) {
        if (c.estado == "habilitada") habilitadas++;
        else if (c.estado == "cerrada") cerradas++;
    }

    for (const Unidad& u : unidades) {
        if (u.estado == "disponible") disponibles++;
    }

    // Cuenta cuantos pedidos (asignados o entregados) tiene cada unidad.
    map<string, int> pedidosPorUnidad;

    for (const Pedido& p : pedidos) {
        if (p.estado == "pendiente") pendientes++;
        else if (p.estado == "asignado") asignados++;
        else if (p.estado == "entregado") entregados++;

        if (!p.unidad_asignada.empty()) {
            pedidosPorUnidad[p.unidad_asignada]++;
        }
    }

    string mejorUnidad;
    int mejorCantidad = 0;

    for (const Unidad& u : unidades) {
        int cantidad = pedidosPorUnidad[u.codigo];

        if (cantidad > mejorCantidad) {
            mejorCantidad = cantidad;
            mejorUnidad = u.codigo;
        }
    }

    cout << "\n=========== REPORTE GENERAL ===========\n";
    cout << "Puntos registrados: " << puntos.size() << "\n";
    cout << "Conexiones habilitadas: " << habilitadas
         << " | Cerradas: " << cerradas << "\n";
    cout << "Unidades disponibles: " << disponibles
         << " de " << unidades.size() << "\n";
    cout << "Pedidos pendientes: " << pendientes
         << " | Asignados: " << asignados
         << " | Entregados: " << entregados << "\n";

    if (mejorCantidad > 0) {
        cout << "Unidad con mas pedidos: " << mejorUnidad
             << " (" << mejorCantidad << " pedidos)\n";
    } else {
        cout << "Unidad con mas pedidos: ninguna\n";
    }

    cout << "=======================================\n";
}


// ============================================================================
// 9. Menus y programa principal
// ============================================================================

// ---------------------------------------------------------------------------
// [4.1.1] MENU PRINCIPAL - Autor: Juan
// ---------------------------------------------------------------------------

void menuRegistro(
    vector<Punto>& puntos,
    vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial
) {
    string opcion;

    do {
        cout << "\n====================================\n";
        cout << "       REGISTRO DE INFORMACION\n";
        cout << "====================================\n";
        cout << "1. Registrar punto\n";
        cout << "2. Registrar conexion\n";
        cout << "3. Registrar unidad\n";
        cout << "4. Registrar pedido\n";
        cout << "0. Volver al menu principal\n";
        cout << "====================================\n";

        opcion = leerOpcionRegistro(
            "Seleccione una opcion: ", {"0", "1", "2", "3", "4"}
        );

        if (opcion == "1") {
            registrarPunto(puntos, conexiones, unidades, pedidos,
                           combustibles, mantenimientos, historial);
        }
        else if (opcion == "2") {
            registrarConexion(puntos, conexiones, unidades, pedidos,
                              combustibles, mantenimientos, historial);
        }
        else if (opcion == "3") {
            registrarUnidad(puntos, conexiones, unidades, pedidos,
                            combustibles, mantenimientos, historial);
        }
        else if (opcion == "4") {
            registrarPedido(puntos, conexiones, unidades, pedidos,
                            combustibles, mantenimientos, historial);
        }
    } while (opcion != "0");
}


void menuGestionEstados(
    vector<Punto>& puntos,
    vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial
) {
    string opcion;

    do {
        cout << "\n====================================\n";
        cout << "         GESTION DE ESTADOS\n";
        cout << "====================================\n";
        cout << "1. Cerrar o habilitar una conexion\n";
        cout << "2. Asignar pedido a una unidad\n";
        cout << "3. Marcar pedido como entregado\n";
        cout << "0. Volver al menu principal\n";
        cout << "====================================\n";

        opcion = leerOpcionRegistro(
            "Seleccione una opcion: ", {"0", "1", "2", "3"}
        );

        if (opcion == "1") {
            gestionarConexion(puntos, conexiones, unidades, pedidos,
                              combustibles, mantenimientos, historial);
        }
        else if (opcion == "2") {
            asignarPedido(puntos, conexiones, unidades, pedidos,
                          combustibles, mantenimientos, historial);
        }
        else if (opcion == "3") {
            marcarPedidoEntregado(puntos, conexiones, unidades, pedidos,
                                  combustibles, mantenimientos, historial);
        }
    } while (opcion != "0");
}


void menuFuncionalidadesAdicionales(
    vector<Punto>& puntos,
    vector<Conexion>& conexiones,
    vector<Unidad>& unidades,
    vector<Pedido>& pedidos,
    vector<Combustible>& combustibles,
    vector<Mantenimiento>& mantenimientos,
    vector<HistorialMantenimiento>& historial
) {
    string opcion;

    do {
        cout << "\n==========================================\n";
        cout << "        FUNCIONALIDADES ADICIONALES\n";
        cout << "==========================================\n";
        cout << " -- Riesgo sanitario (Diana) --\n";
        cout << " 1. Actualizar dias sin recolectar de un punto\n";
        cout << " 2. Avanzar dias sin recolectar (simulacion)\n";
        cout << " 3. Evaluar riesgo sanitario y priorizar\n";
        cout << " -- Combustible y autonomia (Carlos) --\n";
        cout << " 4. Registrar control de combustible\n";
        cout << " 5. Abastecer combustible\n";
        cout << " 6. Registrar consumo por recorrido\n";
        cout << " 7. Consultar autonomia\n";
        cout << " -- Mantenimiento preventivo (Jason) --\n";
        cout << " 8. Registrar control de mantenimiento\n";
        cout << " 9. Actualizar kilometraje\n";
        cout << "10. Consultar mantenimiento de una unidad\n";
        cout << "11. Registrar servicio de mantenimiento\n";
        cout << " 0. Volver al menu principal\n";
        cout << "==========================================\n";

        opcion = leerOpcionRegistro(
            "Seleccione una opcion: ",
            {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"}
        );

        if (opcion == "1") {
            actualizarDiasSinRecolectar(puntos, conexiones, unidades, pedidos,
                                        combustibles, mantenimientos, historial);
        }
        else if (opcion == "2") {
            avanzarDiasSinRecolectar(puntos, conexiones, unidades, pedidos,
                                     combustibles, mantenimientos, historial);
        }
        else if (opcion == "3") {
            evaluarRiesgoSanitario(puntos, pedidos, conexiones, unidades,
                                   combustibles, mantenimientos, historial);
        }
        else if (opcion == "4") {
            registrarCombustible(puntos, conexiones, unidades, pedidos,
                                 combustibles, mantenimientos, historial);
        }
        else if (opcion == "5") {
            abastecerCombustible(puntos, conexiones, unidades, pedidos,
                                 combustibles, mantenimientos, historial);
        }
        else if (opcion == "6") {
            registrarConsumoCombustible(puntos, conexiones, unidades, pedidos,
                                        combustibles, mantenimientos, historial);
        }
        else if (opcion == "7") {
            consultarAutonomia(combustibles);
        }
        else if (opcion == "8") {
            registrarControlMantenimiento(puntos, conexiones, unidades, pedidos,
                                          combustibles, mantenimientos, historial);
        }
        else if (opcion == "9") {
            actualizarKilometraje(puntos, conexiones, unidades, pedidos,
                                  combustibles, mantenimientos, historial);
        }
        else if (opcion == "10") {
            consultarMantenimientoPreventivo(unidades, mantenimientos);
        }
        else if (opcion == "11") {
            registrarServicioMantenimiento(puntos, conexiones, unidades, pedidos,
                                           combustibles, mantenimientos, historial);
        }
    } while (opcion != "0");
}


int main() {
    vector<Punto> puntos;
    vector<Conexion> conexiones;
    vector<Unidad> unidades;
    vector<Pedido> pedidos;
    vector<Combustible> combustibles;
    vector<Mantenimiento> mantenimientos;
    vector<HistorialMantenimiento> historial;
    EsquemaCSV esquema;

    // [4.1.2] Carga inicial: lee los CSV existentes al iniciar.
    cargarTodo(puntos, conexiones, unidades, pedidos,
               combustibles, mantenimientos, historial, esquema);

    string opcion;

    do {
        cout << "\n==========================================\n";
        cout << "  PLANIFICADOR DE RUTAS - RESIDUOS SURCO\n";
        cout << "==========================================\n";
        cout << "1. Registrar informacion\n";
        cout << "2. Gestion de estados\n";
        cout << "3. Consultas basicas\n";
        cout << "4. Reporte general\n";
        cout << "5. Funcionalidades adicionales\n";
        cout << "0. Salir\n";
        cout << "==========================================\n";

        opcion = leerOpcionRegistro(
            "Seleccione una opcion: ", {"0", "1", "2", "3", "4", "5"}
        );

        if (opcion == "1") {
            menuRegistro(puntos, conexiones, unidades, pedidos,
                         combustibles, mantenimientos, historial);
        }
        else if (opcion == "2") {
            menuGestionEstados(puntos, conexiones, unidades, pedidos,
                               combustibles, mantenimientos, historial);
        }
        else if (opcion == "3") {
            menuConsultas(puntos, conexiones, unidades, pedidos);
        }
        else if (opcion == "4") {
            reporteGeneral(puntos, conexiones, unidades, pedidos);
        }
        else if (opcion == "5") {
            menuFuncionalidadesAdicionales(
                puntos, conexiones, unidades, pedidos,
                combustibles, mantenimientos, historial
            );
        }
    } while (opcion != "0");

    cout << "\nHasta pronto.\n";
    return 0;
}
