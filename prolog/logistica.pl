% ============================================================================
% Modulo Prolog - Planificacion de Rutas y Logistica (Santiago de Surco)
% Funcionalidad adicional (Juan David Morales Rabelly):
% Compatibilidad logica de camiones especializados y capacidad por pedido.
% ============================================================================

:- use_module(library(csv)).

:- dynamic pedido/8.
:- dynamic unidad/6.

% ----------------------------------------------------------------------------
% Hechos base iniciales (respaldo si no se cargan desde data/*.csv)
% pedido(CodPedido, Origen, Destino, CargaKg, Prioridad, Estado, UnidadAsignada, TipoResiduo).
% unidad(CodUnidad, TipoCamion, CapacidadKg, CostoKm, Estado, Especialidad).
% ----------------------------------------------------------------------------

unidad(u01, 'Camion Compactador Grande', 5000, 4.50, ocupada,    domiciliario).
unidad(u02, 'Camion Baranda Reciclaje',  3000, 3.20, disponible, reciclable).
unidad(u03, 'Camion Volquete Pesado',    6000, 5.80, disponible, desmonte).

pedido(r01, p01, p08, 1800, alta,  asignado,  u01, domiciliario).
pedido(r02, p04, p06,  950, alta,  pendiente, '',  reciclable).
pedido(r03, p05, p07, 1200, media, entregado, u02, reciclable).
pedido(r04, p02, p08, 2100, alta,  asignado,  u01, domiciliario).
pedido(r05, p06, p08, 2500, baja,  pendiente, '',  desmonte).
pedido(r06, p03, p07,  600, media, entregado, u02, reciclable).

% ----------------------------------------------------------------------------
% Regla principal de compatibilidad (especialidad, disponibilidad y capacidad)
% ----------------------------------------------------------------------------

camion_compatible(CodPedido, CodUnidad) :-
    pedido(CodPedido, _, _, Carga, _, pendiente, _, Tipo),
    unidad(CodUnidad, _, Capacidad, _, disponible, Tipo),
    Capacidad >= Carga.

% Consulta auxiliar para obtener todas las unidades compatibles como lista
camiones_compatibles(CodPedido, ListaUnidades) :-
    findall(CodUnidad, camion_compatible(CodPedido, CodUnidad), ListaUnidades).

% ----------------------------------------------------------------------------
% Resolucion de rutas CSV y carga dinamica desde data/
% ----------------------------------------------------------------------------

resolver_ruta_csv(Archivo, Ruta) :-
    member(Prefijo, ['data/', '../data/', './']),
    atom_concat(Prefijo, Archivo, Ruta),
    exists_file(Ruta),
    !.

normalizar_atomo(Valor, Atomo) :-
    (   atom(Valor) -> Raw = Valor
    ;   string(Valor) -> atom_string(Raw, Valor)
    ;   term_to_atom(Valor, Raw)
    ),
    downcase_atom(Raw, Atomo).

cargar_unidades_csv(Ruta) :-
    csv_read_file(Ruta, [_|Filas], [strip(true)]),
    retractall(unidad(_, _, _, _, _, _)),
    forall(member(Fila, Filas), insertar_unidad_fila(Fila)).

insertar_unidad_fila(row(Cod, Tipo, Cap, Costo, Est, Esp)) :-
    normalizar_atomo(Cod, CodA),
    normalizar_atomo(Est, EstA),
    normalizar_atomo(Esp, EspA),
    assertz(unidad(CodA, Tipo, Cap, Costo, EstA, EspA)).
insertar_unidad_fila(row(Cod, Tipo, Cap, Costo, Est)) :-
    normalizar_atomo(Cod, CodA),
    normalizar_atomo(Est, EstA),
    assertz(unidad(CodA, Tipo, Cap, Costo, EstA, sin_definir)).

cargar_pedidos_csv(Ruta) :-
    csv_read_file(Ruta, [_|Filas], [strip(true)]),
    retractall(pedido(_, _, _, _, _, _, _, _)),
    forall(member(Fila, Filas), insertar_pedido_fila(Fila)).

insertar_pedido_fila(row(Cod, Orig, Dest, Carga, Prio, Est, Uni, TipoRes)) :-
    normalizar_atomo(Cod, CodA),
    normalizar_atomo(Orig, OrigA),
    normalizar_atomo(Dest, DestA),
    normalizar_atomo(Prio, PrioA),
    normalizar_atomo(Est, EstA),
    normalizar_atomo(Uni, UniA),
    normalizar_atomo(TipoRes, TipoA),
    assertz(pedido(CodA, OrigA, DestA, Carga, PrioA, EstA, UniA, TipoA)).
insertar_pedido_fila(row(Cod, Orig, Dest, Carga, Prio, Est, Uni)) :-
    normalizar_atomo(Cod, CodA),
    normalizar_atomo(Orig, OrigA),
    normalizar_atomo(Dest, DestA),
    normalizar_atomo(Prio, PrioA),
    normalizar_atomo(Est, EstA),
    normalizar_atomo(Uni, UniA),
    assertz(pedido(CodA, OrigA, DestA, Carga, PrioA, EstA, UniA, sin_definir)).

cargar_csv :-
    (   resolver_ruta_csv('unidades.csv', RutaUni)
    ->  cargar_unidades_csv(RutaUni)
    ;   true
    ),
    (   resolver_ruta_csv('pedidos.csv', RutaPed)
    ->  cargar_pedidos_csv(RutaPed)
    ;   true
    ).

:- initialization(cargar_csv, now).

% Demostracion rapida no interactiva: swipl -q -s prolog/logistica.pl -g demo -t halt
demo :-
    forall(
        pedido(CodP, _, _, Carga, _, pendiente, _, Tipo),
        (
            camiones_compatibles(CodP, Unidades),
            format('Pedido pendiente ~w (~w, ~w kg) -> Camiones compatibles: ~w~n',
                   [CodP, Tipo, Carga, Unidades])
        )
    ).
