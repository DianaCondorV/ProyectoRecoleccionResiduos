
% ============================================================================
% 1. LIBRERIAS Y UTILIDADES GENERALES
% ============================================================================

:- use_module(library(csv)).
:- use_module(library(readutil)).
:- use_module(library(lists)).

% Convierte cualquier valor en un atomo en minusculas y sin espacios sobrantes.
a_atomo(Valor, Atomo) :-
    format(string(Crudo), '~w', [Valor]),
    normalize_space(string(Limpio), Crudo),
    string_lower(Limpio, Minuscula),
    atom_string(Atomo, Minuscula).

% Convierte cualquier valor en un atomo conservando mayusculas (nombres).
a_texto(Valor, Atomo) :-
    format(atom(Crudo), '~w', [Valor]),
    normalize_space(atom(Atomo), Crudo).

% Convierte un valor en numero; falla si no es un numero valido.
a_numero(Valor, Numero) :-
    number(Valor), !,
    Numero = Valor.
a_numero(Valor, Numero) :-
    format(string(Texto), '~w', [Valor]),
    number_string(Numero, Texto).

% Une los elementos de una lista con un separador (lista vacia -> "ninguno").
unir(Lista, Texto) :-
    (   Lista == []
    ->  Texto = ninguno
    ;   atomic_list_concat(Lista, ', ', Texto)
    ).


% ============================================================================
% 2. BASE DE CONOCIMIENTO (HECHOS DINAMICOS)
% ============================================================================

% punto(Codigo).
% conexion(Origen, Destino, DistanciaKm, TiempoMin, Estado).
% unidad(Codigo, Tipo, CapacidadKg, CostoKm, Estado, Especialidad).
% pedido(Codigo, Origen, Destino, CargaKg, Prioridad, Estado, Unidad, Residuo).

:- dynamic punto/1.
:- dynamic detalle_punto/4.
:- dynamic conexion/5.
:- dynamic unidad/6.
:- dynamic pedido/8.
:- dynamic mantenimiento/4.
:- dynamic historial_mantenimiento/5.
:- dynamic combustible/4.


% ============================================================================
% 3. LECTURA DE ARCHIVOS CSV
% ============================================================================

% ---------------------------------------------------------------------------
% [4.3.1] LECTURA DE ARCHIVOS CSV - Autor: Diana
% Lee los CSV (puntos, conexiones, unidades y pedidos) y construye con ellos
% la base de conocimiento.
% ---------------------------------------------------------------------------

% Busca un archivo en las carpetas data/ mas comunes y devuelve su ruta.
resolver_ruta_csv(Archivo, Ruta) :-
    member(Prefijo, ['data/', '../data/', '../../data/', '../../../data/',
                     '../cpp/data/']),
    atom_concat(Prefijo, Archivo, Ruta),
    exists_file(Ruta),
    !.

% Lee un CSV y devuelve sus filas sin la cabecera.
leer_filas(Ruta, Filas) :-
    csv_read_file(Ruta, Todas, [strip(true)]),
    (   Todas = [_ | Resto]
    ->  Filas = Resto
    ;   Filas = []
    ).

% Lee un CSV (si existe) y aplica Insertar a los campos de cada fila.
% Las filas mal formadas se ignoran.
cargar_tabla(Archivo, Insertar) :-
    (   resolver_ruta_csv(Archivo, Ruta)
    ->  leer_filas(Ruta, Filas),
        forall(member(Fila, Filas),
               (   Fila =.. [_ | Campos],
                   (   call(Insertar, Campos) -> true ; true )
               ))
    ;   true
    ).

% Inserta un punto a partir de una fila de puntos.csv (solo usa el codigo).
insertar_punto([Cod, _Nombre, Tipo, Zona, Dias | _]) :-
    a_atomo(Cod, Punto),
    a_atomo(Tipo, TipoPunto),
    a_texto(Zona, ZonaPunto),
    a_numero(Dias, DiasSinRecojo),
    Punto \== '',
    (punto(Punto) -> true ; assertz(punto(Punto))),
    assertz(detalle_punto(Punto, TipoPunto, ZonaPunto, DiasSinRecojo)).
insertar_punto([Cod | _]) :-
    a_atomo(Cod, Punto),
    Punto \== '',
    \+ punto(Punto),
    assertz(punto(Punto)).

% Inserta una conexion a partir de una fila de conexiones.csv.
insertar_conexion([Origen, Destino, Dist, Tiempo, Estado | _]) :-
    a_atomo(Origen, O),
    a_atomo(Destino, D),
    a_numero(Dist, Km),
    a_numero(Tiempo, Min),
    a_atomo(Estado, E),
    assertz(conexion(O, D, Km, Min, E)).

% Inserta una unidad a partir de una fila de unidades.csv (con especialidad).
insertar_unidad([Cod, Tipo, Cap, Costo, Estado, Esp | _]) :-
    a_atomo(Cod, C),
    a_texto(Tipo, T),
    a_numero(Cap, Capacidad),
    a_numero(Costo, CostoKm),
    a_atomo(Estado, E),
    a_atomo(Esp, Especialidad),
    assertz(unidad(C, T, Capacidad, CostoKm, E, Especialidad)).
% Inserta una unidad de un CSV antiguo que no trae especialidad.
insertar_unidad([Cod, Tipo, Cap, Costo, Estado]) :-
    insertar_unidad([Cod, Tipo, Cap, Costo, Estado, sin_definir]).

% Inserta un pedido a partir de una fila de pedidos.csv (con tipo de residuo).
insertar_pedido([Cod, Origen, Destino, Carga, Prio, Estado, Unidad, Residuo | _]) :-
    a_atomo(Cod, C),
    a_atomo(Origen, O),
    a_atomo(Destino, D),
    a_numero(Carga, Kg),
    a_atomo(Prio, P),
    a_atomo(Estado, E),
    a_atomo(Unidad, U),
    a_atomo(Residuo, R),
    assertz(pedido(C, O, D, Kg, P, E, U, R)).
% Inserta un pedido de un CSV antiguo que no trae tipo de residuo.
insertar_pedido([Cod, Origen, Destino, Carga, Prio, Estado, Unidad]) :-
    insertar_pedido([Cod, Origen, Destino, Carga, Prio, Estado, Unidad,
                     sin_definir]).

% FA Jason: kilometraje actual, kilometraje del ultimo servicio e intervalo.
insertar_mantenimiento([Unidad, KmActual, KmUltimo, Intervalo | _]) :-
    a_atomo(Unidad, U),
    a_numero(KmActual, Actual),
    a_numero(KmUltimo, Ultimo),
    a_numero(Intervalo, Cada),
    Actual >= Ultimo,
    Cada > 0,
    assertz(mantenimiento(U, Actual, Ultimo, Cada)).

% FA Jason: el historial conserva los servicios registrados desde C++.
insertar_historial_mantenimiento([Codigo, Unidad, Fecha, Km, Descripcion | _]) :-
    a_atomo(Codigo, C),
    a_atomo(Unidad, U),
    a_texto(Fecha, F),
    a_numero(Km, Kilometraje),
    a_texto(Descripcion, D),
    assertz(historial_mantenimiento(C, U, F, Kilometraje, D)).

% FA Carlos: capacidad del tanque, combustible disponible y consumo por km.
insertar_combustible([Unidad, Capacidad, Actual, Consumo | _]) :-
    a_atomo(Unidad, U),
    a_numero(Capacidad, Cap),
    a_numero(Actual, Litros),
    a_numero(Consumo, LitrosKm),
    Cap > 0,
    Litros >= 0,
    Litros =< Cap,
    LitrosKm > 0,
    assertz(combustible(U, Cap, Litros, LitrosKm)).

% Borra la base de conocimiento y la reconstruye desde los CSV.
cargar_csv :-
    retractall(punto(_)),
    retractall(detalle_punto(_, _, _, _)),
    retractall(conexion(_, _, _, _, _)),
    retractall(unidad(_, _, _, _, _, _)),
    retractall(pedido(_, _, _, _, _, _, _, _)),
    retractall(mantenimiento(_, _, _, _)),
    retractall(historial_mantenimiento(_, _, _, _, _)),
    retractall(combustible(_, _, _, _)),
    cargar_tabla('puntos.csv',     insertar_punto),
    cargar_tabla('conexiones.csv', insertar_conexion),
    cargar_tabla('unidades.csv',   insertar_unidad),
    cargar_tabla('pedidos.csv',    insertar_pedido),
    cargar_tabla('mantenimiento.csv', insertar_mantenimiento),
    cargar_tabla('historial_mantenimiento.csv', insertar_historial_mantenimiento),
    cargar_tabla('combustible.csv', insertar_combustible).

% Verifica que existan los CSV obligatorios; si no, termina el programa.
verificar_archivos :-
    findall(A,
            (   member(A, ['puntos.csv', 'conexiones.csv', 'unidades.csv', 'pedidos.csv']),
                \+ resolver_ruta_csv(A, _)
            ),
            Faltantes),
    (   Faltantes == []
    ->  true
    ;   writeln('[ERROR] No se encontraron los archivos CSV obligatorios:'),
        forall(member(F, Faltantes), format("  - ~w~n", [F])),
        writeln('Ejecute el programa desde la carpeta del proyecto y verifique'),
        writeln('que exista la carpeta data/.'),
        halt(1)
    ).


% ============================================================================
% 4. CONEXION DIRECTA
% ============================================================================

% Comprueba si un punto existe en puntos.csv o participa en alguna conexion.
existe_punto(Punto) :-
    (   punto(Punto)
    ;   conexion(Punto, _, _, _, _)
    ;   conexion(_, Punto, _, _, _)
    ),
    !.

% ---------------------------------------------------------------------------
% [4.3.2] REGLA DE CONEXION DIRECTA - Autor: Carlos
% Dos puntos estan conectados directamente si hay una conexion habilitada
% entre ellos, en cualquier sentido. Ejemplo: conectado(p01, p03, Distancia).
% ---------------------------------------------------------------------------

% Conexion habilitada registrada de A hacia B.
conectado(A, B, Distancia) :-
    conexion(A, B, Distancia, _, habilitada).
% Conexion habilitada registrada de B hacia A (doble sentido).
conectado(A, B, Distancia) :-
    conexion(B, A, Distancia, _, habilitada).


% ============================================================================
% 5. ALCANZABILIDAD
% ============================================================================

% Busca recursivamente un camino sin repetir puntos (usa backtracking).
% Visitados evita volver a un punto ya recorrido.
camino(Destino, Destino, _, [Destino]).
camino(Actual, Destino, Visitados, [Actual | Resto]) :-
    Actual \== Destino,
    conectado(Actual, Siguiente, _),
    \+ memberchk(Siguiente, Visitados),
    camino(Siguiente, Destino, [Siguiente | Visitados], Resto).

% ---------------------------------------------------------------------------
% [4.3.3] REGLA DE ALCANZABILIDAD - Autor: Jason
% Se puede llegar de un punto a otro, directa o indirectamente, usando solo
% conexiones habilitadas. Ejemplo: alcanzable(p01, p06).
% ---------------------------------------------------------------------------

alcanzable(Origen, Destino) :-
    camino(Origen, Destino, [Origen], _),
    !.


% ============================================================================
% 6. RUTA EVITANDO UN PUNTO
% ============================================================================

% ---------------------------------------------------------------------------
% [4.3.4] REGLA DE RUTA EVITANDO UN PUNTO - Autor: Diana
% Obtiene una ruta de Origen a Destino que no pasa por el punto Evitado.
% Con backtracking devuelve todas las rutas posibles.
% Ejemplo: ruta_evitando(p01, p06, p03, Ruta).
% ---------------------------------------------------------------------------

ruta_evitando(Origen, Destino, Evitado, Ruta) :-
    Origen \== Evitado,
    Destino \== Evitado,
    camino(Origen, Destino, [Origen, Evitado], Ruta).


% ============================================================================
% 7. ATENCION DE PEDIDOS
% ============================================================================

% ---------------------------------------------------------------------------
% [4.3.5] REGLA DE ATENCION DE PEDIDOS - Autor: Diana
% Una unidad puede atender un pedido si esta disponible y su capacidad
% alcanza para la carga. Ejemplo: puede_atender(u01, d002).
% ---------------------------------------------------------------------------

puede_atender(CodUnidad, CodPedido) :-
    unidad(CodUnidad, _, Capacidad, _, disponible, _),
    pedido(CodPedido, _, _, Carga, _, _, _, _),
    Capacidad >= Carga.


% ============================================================================
% 8. PEDIDOS INACCESIBLES
% ============================================================================

% ---------------------------------------------------------------------------
% [4.3.6] REGLA DE PEDIDOS INACCESIBLES - Autor: Jason
% Obtiene los pedidos pendientes cuyo destino no es alcanzable desde su
% origen (por las conexiones cerradas). Ejemplo: pedidos_inaccesibles(P).
% ---------------------------------------------------------------------------

pedidos_inaccesibles(Pedidos) :-
    findall(CodPedido,
            (   pedido(CodPedido, Origen, Destino, _, _, pendiente, _, _),
                \+ alcanzable(Origen, Destino)
            ),
            Pedidos).


% ============================================================================
% 9. FA - COMPATIBILIDAD DE CAMIONES (Autor: Juan)
% ============================================================================

% FA - Autor: Juan
% Un camion es compatible con un pedido pendiente si esta disponible, tiene
% la misma especialidad que el residuo y su capacidad alcanza para la carga.
camion_compatible(CodPedido, CodUnidad) :-
    pedido(CodPedido, _, _, Carga, _, pendiente, _, Tipo),
    unidad(CodUnidad, _, Capacidad, _, disponible, Tipo),
    Capacidad >= Carga.

% FA - Autor: Juan
% Reune en una lista todos los camiones compatibles con un pedido.
camiones_compatibles(CodPedido, ListaUnidades) :-
    findall(CodUnidad, camion_compatible(CodPedido, CodUnidad), ListaUnidades).

% FA - Autor: Juan
% Demostracion no interactiva: swipl -q -s logistica.pl -g demo -t halt
demo :-
    cargar_csv,
    forall(pedido(CodP, _, _, Carga, _, pendiente, _, Tipo),
           (   camiones_compatibles(CodP, Unidades),
               format("Pedido pendiente ~w (~w, ~w kg) -> Camiones compatibles: ~w~n",
                      [CodP, Tipo, Carga, Unidades])
           )).

% ============================================================================
% 10. FUNCIONALIDADES ADICIONALES EN PROLOG
% ============================================================================

% FA - Autor: Jason
% Solo devuelve unidades cuyo kilometraje recorrido desde el ultimo servicio
% supero el intervalo configurado.
km_hasta_mantenimiento(Unidad, Restantes) :-
    mantenimiento(Unidad, Actual, Ultimo, Intervalo),
    Recorridos is Actual - Ultimo,
    Restantes is Intervalo - Recorridos.

requiere_mantenimiento(Unidad, KmExcedidos) :-
    km_hasta_mantenimiento(Unidad, Restantes),
    Restantes =< 0,
    KmExcedidos is -Restantes.

% FA - Autor: Carlos
% Calcula autonomia, litros necesarios y suficiencia para una distancia.
autonomia_km(Unidad, Km) :-
    combustible(Unidad, _, Litros, Consumo),
    Consumo > 0,
    Km is Litros / Consumo.

litros_necesarios(Unidad, DistanciaKm, Litros) :-
    combustible(Unidad, _, _, Consumo),
    DistanciaKm >= 0,
    Litros is DistanciaKm * Consumo.

litros_faltantes(Unidad, DistanciaKm, Faltan) :-
    combustible(Unidad, _, Actual, Consumo),
    DistanciaKm >= 0,
    Faltan is max(0, DistanciaKm * Consumo - Actual).

combustible_suficiente(Unidad, DistanciaKm) :-
    combustible(Unidad, _, Actual, Consumo),
    DistanciaKm >= 0,
    Actual >= DistanciaKm * Consumo.

% FA - Autor: Diana
% Umbrales iguales a los documentados e implementados en el modulo C++.
max_dias_sin_recojo(mercado, 2).
max_dias_sin_recojo(residencial, 3).
max_dias_sin_recojo(comercial, 4).
max_dias_sin_recojo(recreativo, 4).

nivel_riesgo_sanitario(Punto, Nivel, Dias) :-
    detalle_punto(Punto, Tipo, _, Dias),
    max_dias_sin_recojo(Tipo, Maximo),
    (   Dias > Maximo
    ->  Nivel = alto
    ;   Dias =:= Maximo
    ->  Nivel = medio
    ;   Nivel = bajo
    ).

% Consulta solicitada en el Anexo: devuelve los dias de los puntos de riesgo alto.
evaluar_riesgo_sanitario(Punto, Dias) :-
    nivel_riesgo_sanitario(Punto, alto, Dias).

pedido_prioritario_por_riesgo(Pedido, Punto) :-
    pedido(Pedido, Punto, _, _, _, pendiente, _, _),
    evaluar_riesgo_sanitario(Punto, _).

camion_sugerido_por_riesgo(Pedido, Unidad) :-
    camion_compatible(Pedido, Unidad).

% Muestra estado de mantenimiento para toda la flota.
opcion_mantenimiento :-
    writeln('\n--- MANTENIMIENTO PREVENTIVO (Jason) ---'),
    (   mantenimiento(_, _, _, _)
    ->  forall(mantenimiento(U, Actual, Ultimo, Intervalo),
               (   km_hasta_mantenimiento(U, Restantes),
                   (   Restantes =< 0
                   ->  Excedidos is -Restantes,
                       format("~w: mantenimiento pendiente; excedido ~w km (actual ~w, ultimo servicio ~w, intervalo ~w).~n",
                              [U, Excedidos, Actual, Ultimo, Intervalo])
                   ;   format("~w: faltan ~w km para mantenimiento.~n",
                              [U, Restantes])
                   )
               ))
    ;   writeln('No hay registros en mantenimiento.csv.')
    ),
    findall(U-Km, requiere_mantenimiento(U, Km), Pendientes),
    (   Pendientes == []
    ->  writeln('Resumen: no hay unidades vencidas.')
    ;   format('Unidades con servicio vencido (unidad-km excedidos): ~w~n',
               [Pendientes])
    ).

% Consulta combustible para una distancia de recorrido ingresada.
opcion_combustible :-
    writeln('\n--- COMBUSTIBLE Y AUTONOMIA (Carlos) ---'),
    pedir_texto('Distancia del recorrido en km: ', Texto),
    (   a_numero(Texto, Distancia),
        Distancia > 0
    ->  (   combustible(_, _, _, _)
        ->  forall(combustible(U, _, Actual, _Consumo),
                   (   autonomia_km(U, Autonomia),
                       litros_necesarios(U, Distancia, Necesarios),
                       litros_faltantes(U, Distancia, Faltan),
                       (   combustible_suficiente(U, Distancia)
                       ->  Estado = suficiente
                       ;   Estado = insuficiente
                       ),
                       format("~w: autonomia ~2f km; ~2f L disponibles; ~2f L necesarios; faltan ~2f L; ~w.~n",
                              [U, Autonomia, Actual, Necesarios, Faltan, Estado])
                   ))
        ;   writeln('No hay registros en combustible.csv.')
        )
    ;   writeln('[ERROR] Ingrese una distancia numerica mayor que cero.')
    ).

% Clasifica puntos por riesgo y propone unidades compatibles para pedidos
% pendientes en puntos de riesgo alto. La asignacion se confirma en C++.
opcion_riesgo_sanitario :-
    writeln('\n--- PRIORIZACION POR RIESGO SANITARIO (Diana) ---'),
    (   detalle_punto(_, _, _, _)
    ->  forall(nivel_riesgo_sanitario(P, Nivel, Dias),
               (   detalle_punto(P, Tipo, Zona, _),
                   format("~w (~w, ~w): ~w; ~w dias sin recojo.~n",
                          [P, Tipo, Zona, Nivel, Dias])
               )),
        findall(Pedido-Punto, pedido_prioritario_por_riesgo(Pedido, Punto), Altos),
        (   Altos == []
        ->  writeln('No hay pedidos pendientes en puntos de riesgo alto.')
        ;   writeln('Atencion prioritaria para pedidos en riesgo alto:'),
            forall(member(Pedido-Punto, Altos),
                   (   findall(U, camion_sugerido_por_riesgo(Pedido, U), Unidades),
                       unir(Unidades, Lista),
                       format("Pedido ~w en ~w -> unidad compatible sugerida: ~w~n",
                              [Pedido, Punto, Lista])
                   ))
        )
    ;   writeln('No hay dias_sin_recolectar cargados en puntos.csv.')
    ).


% ============================================================================
% 11. UTILIDADES DE MENU
% ============================================================================

% Pide un texto por consola (en minusculas). Si se cierra la entrada
% devuelve '0' para que el programa pueda terminar.
pedir_texto(Mensaje, Texto) :-
    format("~w", [Mensaje]),
    flush_output,
    read_line_to_string(user_input, Linea),
    (   Linea == end_of_file
    ->  Texto = '0'
    ;   a_atomo(Linea, Texto)
    ).

% Muestra el menu principal.
mostrar_menu :-
    nl,
    writeln('=========================================='),
    writeln('  PLANIFICADOR DE RUTAS - RESIDUOS SURCO'),
    writeln('=========================================='),
    writeln('1. Ver base de conocimiento'),
    writeln('2. Consultas de la red'),
    writeln('3. Atencion de pedidos'),
    writeln('4. Funcionalidades adicionales'),
    writeln('0. Salir'),
    writeln('==========================================').

% Muestra el submenu de consultas de la red.
mostrar_menu_red :-
    nl,
    writeln('===================================='),
    writeln('        CONSULTAS DE LA RED'),
    writeln('===================================='),
    writeln('1. Conexion directa entre 2 puntos'),
    writeln('2. Alcanzabilidad entre 2 puntos'),
    writeln('3. Ruta evitando un punto'),
    writeln('0. Volver al menu principal'),
    writeln('====================================').

% Muestra el submenu de atencion de pedidos.
mostrar_menu_pedidos :-
    nl,
    writeln('===================================='),
    writeln('        ATENCION DE PEDIDOS'),
    writeln('===================================='),
    writeln('1. Verificar si una unidad puede atender un pedido'),
    writeln('2. Listar pedidos inaccesibles'),
    writeln('0. Volver al menu principal'),
    writeln('====================================').

% Muestra el submenu de funcionalidades adicionales.
mostrar_menu_adicionales :-
    nl,
    writeln('=========================================='),
    writeln('        FUNCIONALIDADES ADICIONALES'),
    writeln('=========================================='),
    writeln(' -- Compatibilidad de camiones (Juan) --'),
    writeln(' 1. Camiones compatibles con un pedido'),
    writeln(' 2. Camiones compatibles con todos los pendientes'),
    writeln(' -- Mantenimiento preventivo (Jason) --'),
    writeln(' 3. Consultar kilometraje y servicios vencidos'),
    writeln(' -- Combustible y autonomia (Carlos) --'),
    writeln(' 4. Evaluar combustible para una distancia'),
    writeln(' -- Riesgo sanitario (Diana) --'),
    writeln(' 5. Priorizar puntos y pedidos de riesgo alto'),
    writeln(' 0. Volver al menu principal'),
    writeln('==========================================').

% Bucle generico de un submenu: relee los CSV, muestra el menu, ejecuta la
% accion elegida y repite hasta que el usuario escribe 0.
submenu(Mostrar, Elegir) :-
    cargar_csv,
    call(Mostrar),
    pedir_texto('Seleccione una opcion: ', Opcion),
    (   Opcion == '0'
    ->  true
    ;   (   call(Elegir, Opcion, Accion)
        ->  (   call(Accion) -> true ; true )
        ;   writeln('[ERROR] Opcion no valida.')
        ),
        submenu(Mostrar, Elegir)
    ).


% ============================================================================
% 11. OPCIONES DEL MENU
% ============================================================================

% [4.3.1] Muestra la base de conocimiento construida desde los CSV.
opcion_mostrar_datos :-
    writeln('\n--- BASE DE CONOCIMIENTO (CSV) ---'),
    findall(P, punto(P), Puntos),
    length(Puntos, NPuntos),
    format("~nPuntos: ~w~n", [NPuntos]),
    unir(Puntos, TextoPuntos),
    format("  ~w~n", [TextoPuntos]),

    aggregate_all(count, conexion(_, _, _, _, _), NCon),
    format("~nConexiones: ~w~n", [NCon]),
    format("~w~t~10|~w~t~20|~w~t~32|~w~t~42|~w~n",
           ['ORIGEN', 'DESTINO', 'DIST. (KM)', 'TIEMPO', 'ESTADO']),
    forall(conexion(O, D, Km, Min, E),
           format("~w~t~10|~w~t~20|~2f~t~32|~w~t~42|~w~n", [O, D, Km, Min, E])),

    aggregate_all(count, unidad(_, _, _, _, _, _), NUni),
    format("~nUnidades: ~w~n", [NUni]),
    format("~w~t~8|~w~t~36|~w~t~48|~w~t~58|~w~t~70|~w~n",
           ['CODIGO', 'TIPO', 'CAP. (KG)', 'S/ POR KM', 'ESTADO', 'ESPECIALIDAD']),
    forall(unidad(C, T, Cap, Costo, Est, Esp),
           format("~w~t~8|~w~t~36|~w~t~48|~2f~t~58|~w~t~70|~w~n",
                  [C, T, Cap, Costo, Est, Esp])),

    aggregate_all(count, pedido(_, _, _, _, _, _, _, _), NPed),
    format("~nPedidos: ~w~n", [NPed]),
    format("~w~t~8|~w~t~16|~w~t~24|~w~t~35|~w~t~46|~w~t~58|~w~t~67|~w~n",
           ['CODIGO', 'ORIGEN', 'DESTINO', 'CARGA (KG)', 'PRIORIDAD',
            'ESTADO', 'UNIDAD', 'RESIDUO']),
    forall(pedido(Cod, Ori, Des, Carga, Prio, Estado, Uni, Res),
           (   (Uni == '' -> UniTxt = '-' ; UniTxt = Uni),
               format("~w~t~8|~w~t~16|~w~t~24|~w~t~35|~w~t~46|~w~t~58|~w~t~67|~w~n",
                      [Cod, Ori, Des, Carga, Prio, Estado, UniTxt, Res])
           )).

% [4.3.2] Pide dos puntos y muestra si estan conectados directamente.
opcion_conectado :-
    writeln('\n--- CONEXION DIRECTA ---'),
    pedir_texto('Punto A (ejemplo p01): ', A),
    pedir_texto('Punto B (ejemplo p03): ', B),
    (   \+ existe_punto(A)
    ->  format("[ERROR] El punto ~w no existe en la red.~n", [A])
    ;   \+ existe_punto(B)
    ->  format("[ERROR] El punto ~w no existe en la red.~n", [B])
    ;   conectado(A, B, Distancia)
    ->  format("~w y ~w estan conectados directamente: ~2f km.~n",
               [A, B, Distancia])
    ;   format("~w y ~w NO estan conectados directamente por una conexion habilitada.~n",
               [A, B])
    ).

% [4.3.3] Pide origen y destino y muestra si el destino es alcanzable.
opcion_alcanzable :-
    writeln('\n--- ALCANZABILIDAD ---'),
    pedir_texto('Punto de origen: ', Origen),
    pedir_texto('Punto de destino: ', Destino),
    (   \+ existe_punto(Origen)
    ->  format("[ERROR] El punto de origen ~w no existe en la red.~n", [Origen])
    ;   \+ existe_punto(Destino)
    ->  format("[ERROR] El punto de destino ~w no existe en la red.~n", [Destino])
    ;   alcanzable(Origen, Destino)
    ->  format("Si se puede llegar de ~w a ~w por conexiones habilitadas.~n",
               [Origen, Destino])
    ;   format("NO se puede llegar de ~w a ~w por conexiones habilitadas.~n",
               [Origen, Destino])
    ).

% [4.3.4] Pide origen, destino y punto a evitar; lista las rutas que existen.
opcion_ruta_evitando :-
    writeln('\n--- RUTA EVITANDO UN PUNTO ---'),
    pedir_texto('Punto de origen: ', Origen),
    pedir_texto('Punto de destino: ', Destino),
    pedir_texto('Punto a evitar: ', Evitado),
    (   \+ existe_punto(Origen)
    ->  format("[ERROR] El punto de origen ~w no existe en la red.~n", [Origen])
    ;   \+ existe_punto(Destino)
    ->  format("[ERROR] El punto de destino ~w no existe en la red.~n", [Destino])
    ;   \+ existe_punto(Evitado)
    ->  format("[ERROR] El punto a evitar ~w no existe en la red.~n", [Evitado])
    ;   (   Origen == Evitado ; Destino == Evitado )
    ->  writeln('[ERROR] El punto a evitar no puede ser el origen ni el destino.')
    ;   findall(Ruta, ruta_evitando(Origen, Destino, Evitado, Ruta), Rutas),
        (   Rutas == []
        ->  format("No existe ruta de ~w a ~w evitando ~w.~n",
                   [Origen, Destino, Evitado])
        ;   length(Rutas, N),
            format("Rutas de ~w a ~w evitando ~w: ~w~n",
                   [Origen, Destino, Evitado, N]),
            mostrar_rutas(Rutas, 1)
        )
    ).

% Muestra las rutas numeradas: 1) p01 -> p02 -> p06
mostrar_rutas([], _).
mostrar_rutas([Ruta | Resto], N) :-
    atomic_list_concat(Ruta, ' -> ', Texto),
    format("  ~w) ~w~n", [N, Texto]),
    N1 is N + 1,
    mostrar_rutas(Resto, N1).

% [4.3.5] Pide una unidad y un pedido y verifica si la unidad puede atenderlo.
opcion_puede_atender :-
    writeln('\n--- ATENCION DE PEDIDOS ---'),
    pedir_texto('Codigo de la unidad (ejemplo u01): ', U),
    pedir_texto('Codigo del pedido (ejemplo d002): ', P),
    (   \+ unidad(U, _, _, _, _, _)
    ->  format("[ERROR] La unidad ~w no existe.~n", [U])
    ;   \+ pedido(P, _, _, _, _, _, _, _)
    ->  format("[ERROR] El pedido ~w no existe.~n", [P])
    ;   puede_atender(U, P)
    ->  format("La unidad ~w SI puede atender el pedido ~w.~n", [U, P])
    ;   unidad(U, _, _, _, Estado, _), Estado \== disponible
    ->  format("La unidad ~w NO puede atender el pedido ~w: esta ~w.~n",
               [U, P, Estado])
    ;   format("La unidad ~w NO puede atender el pedido ~w: capacidad insuficiente.~n",
               [U, P])
    ).

% [4.3.6] Lista los pedidos pendientes cuyo destino no es alcanzable.
opcion_inaccesibles :-
    writeln('\n--- PEDIDOS INACCESIBLES ---'),
    pedidos_inaccesibles(Pedidos),
    (   Pedidos == []
    ->  writeln('Todos los pedidos pendientes tienen una ruta habilitada.')
    ;   length(Pedidos, N),
        format("Pedidos pendientes sin ruta habilitada: ~w~n", [N]),
        forall(member(Cod, Pedidos),
               (   pedido(Cod, O, D, Carga, Prio, _, _, _),
                   format("  ~w (~w) ~w -> ~w | ~w kg~n", [Cod, Prio, O, D, Carga])
               ))
    ).

% FA - Autor: Juan
% Pide un pedido pendiente y lista los camiones compatibles.
opcion_compatibles :-
    writeln('\n--- CAMIONES COMPATIBLES CON UN PEDIDO ---'),
    pedir_texto('Codigo del pedido pendiente: ', P),
    (   \+ pedido(P, _, _, _, _, _, _, _)
    ->  format("[ERROR] El pedido ~w no existe.~n", [P])
    ;   \+ pedido(P, _, _, _, _, pendiente, _, _)
    ->  format("[AVISO] El pedido ~w no esta pendiente.~n", [P])
    ;   pedido(P, _, _, Carga, _, _, _, Tipo),
        camiones_compatibles(P, Unidades),
        unir(Unidades, Texto),
        format("Pedido ~w (~w, ~w kg) -> Camiones compatibles: ~w~n",
               [P, Tipo, Carga, Texto])
    ).

% FA - Autor: Juan
% Lista los camiones compatibles con cada pedido pendiente.
opcion_compatibles_todos :-
    writeln('\n--- COMPATIBILIDAD DE LOS PEDIDOS PENDIENTES ---'),
    (   pedido(_, _, _, _, _, pendiente, _, _)
    ->  forall(pedido(P, _, _, Carga, _, pendiente, _, Tipo),
               (   camiones_compatibles(P, Unidades),
                   unir(Unidades, Texto),
                   format("Pedido ~w (~w, ~w kg) -> Camiones compatibles: ~w~n",
                          [P, Tipo, Carga, Texto])
               ))
    ;   writeln('No hay pedidos pendientes.')
    ).


% ============================================================================
% 12. SUBMENUS Y PROGRAMA PRINCIPAL
% ============================================================================

% Asocia cada opcion del submenu de la red con su accion.
eleccion_red('1', opcion_conectado).
eleccion_red('2', opcion_alcanzable).
eleccion_red('3', opcion_ruta_evitando).

% Asocia cada opcion del submenu de pedidos con su accion.
eleccion_pedidos('1', opcion_puede_atender).
eleccion_pedidos('2', opcion_inaccesibles).

% Asocia cada opcion de funcionalidades adicionales con su accion.
eleccion_adicionales('1', opcion_compatibles).
eleccion_adicionales('2', opcion_compatibles_todos).
eleccion_adicionales('3', opcion_mantenimiento).
eleccion_adicionales('4', opcion_combustible).
eleccion_adicionales('5', opcion_riesgo_sanitario).

% Bucle del menu principal. Los CSV se vuelven a leer en cada vuelta, asi que
% los cambios hechos desde C++ se ven sin reiniciar el programa.
bucle_menu :-
    cargar_csv,
    mostrar_menu,
    pedir_texto('Seleccione una opcion: ', Opcion),
    (   Opcion == '0'
    ->  writeln('\nHasta pronto.')
    ;   ejecutar_opcion(Opcion),
        bucle_menu
    ).

% Ejecuta la opcion elegida en el menu principal.
ejecutar_opcion('1') :- !, opcion_mostrar_datos.
ejecutar_opcion('2') :- !, submenu(mostrar_menu_red, eleccion_red).
ejecutar_opcion('3') :- !, submenu(mostrar_menu_pedidos, eleccion_pedidos).
ejecutar_opcion('4') :- !, submenu(mostrar_menu_adicionales, eleccion_adicionales).
ejecutar_opcion(_)   :- writeln('[ERROR] Opcion no valida.').

% Punto de entrada: verifica los CSV e inicia el menu.
main :-
    verificar_archivos,
    bucle_menu.