// =====================================================================
//  Placa rígida para vestido cinético de 14 servos independientes
//  Inspirado en "Moments in Motion" (Lisa Jiang x Timothy A. Boll)
//  Servos MG90S montados de canto, eje tangente al torso,
//  costillas de fibra de carbono que oscilan hacia afuera del cuerpo.
//
//  Unidades: mm.  OpenSCAD 2021+.
//
//  CÓMO USAR
//   - Cambia `parte` para ver o exportar cada pieza:
//       "ensamble"        -> vista en el torso, con servos, costillas y volantes
//                            (Ver > Animar, FPS 10, Steps 60 para ver la onda)
//       "segmento"        -> segmento plano para imprimir (elige segmento_idx)
//       "plano_completo"  -> todos los segmentos planos + acoples, para revisar
//       "acople"          -> acople cuerno–costilla (imprimir 14 + repuestos)
//       "bahia"           -> soporte para PCA9685 + buck + ESP32 (espalda)
//   - Los segmentos se imprimen PLANOS (orejas hacia arriba, sin soportes)
//     y luego se termoforman al radio del maniquí (PETG: agua a ~85 °C o
//     pistola de calor sobre un tubo/maniquí del mismo radio).
//   - Cada segmento se cose/velcro al corsé por separado: así el conjunto
//     sigue la respiración y el movimiento del torso.
// =====================================================================

/* [Qué mostrar] */
parte = "ensamble"; // [ensamble, segmento, plano_completo, acople, bahia]
segmento_idx = 0;   // 0..len(segmentos)-1

/* [Distribución] */
n_servos   = 14;
segmentos  = [5, 4, 5]; // servos por segmento (debe sumar n_servos). 5 x 38 + 16 = 206 mm < cama 220
paso       = 38;        // separación entre servos a lo largo del arco (mín. ~36)
margen     = 8;         // margen en los extremos de cada segmento
gap_seg    = 4;         // separación entre segmentos sobre el cuerpo

/* [Cuerpo] */
R_torso = 135;  // radio del maniquí en la línea de servos (= contorno / 2π)
espuma  = 4;    // foam entre placa y cuerpo

/* [Placa] */
t_placa = 2.5;  // PETG
y_min   = -34;  // borde inferior (respecto al centro del servo)
y_max   =  28;  // borde superior
r_esq   = 4;
paso_costura = 8;
d_costura    = 2;

/* [Servo MG90S — mide el tuyo] */
srv_L  = 22.8;  // largo del cuerpo (vertical en el vestido)
srv_W  = 12.2;  // ancho (normal a la placa → lo que sobresale)
srv_H  = 22.5;  // alto de la carcasa (a lo largo del eje)
srv_Htot = 28.5;// alto total hasta la punta del estriado
brida_x  = 16;  // desde el fondo del servo hasta la cara inferior de la brida
brida_t  = 2.5;
brida_span = 32.5;
agujero_sep = 27.8;
eje_off  = 5.9; // distancia del eje al extremo del cuerpo
holgura  = 0.2; // por lado en la ventana (ajusta a tu impresora)
d_tornillo = 1.6; // para M2 autorroscante en PETG

/* [Oreja y cartelas] */
oreja_t = 3;
oreja_y = 20;   // semiancho
oreja_z = 15.5; // alto desde la placa
cartela_t = 3;

/* [Cuerno y acople] */
cuerno_largo   = 15;  // centro del eje → agujero exterior del cuerno (MIDE el tuyo)
cuerno_t       = 1.8;
cuerno_w_base  = 6;
cuerno_w_punta = 4;
d_agujero_cuerno = 2;
acople_t   = 4;
acople_ext = 12;     // tramo que sostiene la costilla
d_costilla = 1.5;    // varilla de fibra de carbono
largo_costilla = 170;

/* [Bahía electrónica] */
pca_hole_dx = 56;   // separación de agujeros PCA9685 (verifica tu placa)
pca_hole_dy = 19;
pca_d_hole  = 2.5;

/* [Animación (ensamble)] */
alfa0    = 25;  // inclinación de reposo hacia afuera: con amplitud 20, nunca baja de 5° (la costilla no roza la placa ni el cuerpo)
amplitud = 20;  // ± grados
fase     = 40;  // desfase entre servos vecinos (30–45° = onda continua)
mostrar_volantes = true;

$fn = 32;

// ---------------------------------------------------------------------
//  Derivados
// ---------------------------------------------------------------------
x0    = -15;                 // fondo del servo en el marco del módulo
eje_y = srv_L/2 - eje_off;   // eje desplazado hacia arriba → el cuerno cuelga
eje_z = srv_W/2;
x_cuerno = x0 + srv_Htot - 1.0; // plano inferior del brazo del cuerno

R_in  = R_torso + espuma;    // cara interior de la placa
R_out = R_in + t_placa;      // cara exterior (Z = 0 local)

function suma(v, i = 0) = i >= len(v) ? 0 : v[i] + suma(v, i + 1);
function L_seg(k) = 2*margen + segmentos[k]*paso;
function largo_total() = suma([for (k = [0:len(segmentos)-1]) L_seg(k)]) + gap_seg*(len(segmentos)-1);
function s_ini(k) = -largo_total()/2 + suma([for (i = [0:1:k-1]) L_seg(i) + gap_seg]);
function servos_antes(k) = suma([for (i = [0:1:k-1]) segmentos[i]]);

assert(suma(segmentos) == n_servos, "segmentos debe sumar n_servos");
assert(paso >= 36, "paso < 36 mm: los cuernos chocan con el servo vecino");

// ---------------------------------------------------------------------
//  Colocación: plano o sobre el cilindro del torso
//  Marco local: X tangente (a lo largo del arco), Y vertical, Z radial hacia afuera
// ---------------------------------------------------------------------
module colocar(k, xs) {
    if ($curvo) {
        phi = (s_ini(k) + xs) / R_out * 180 / PI;
        u = [cos(phi), sin(phi), 0];
        t = [-sin(phi), cos(phi), 0];
        multmatrix([[t[0], 0, u[0], R_out*u[0]],
                    [t[1], 0, u[1], R_out*u[1]],
                    [0,    1, 0,    0         ],
                    [0,    0, 0,    1         ]]) children();
    } else {
        translate([xs, 0, 0]) children();
    }
}

// ---------------------------------------------------------------------
//  Geometría de un módulo (un servo), centrado en X = 0
// ---------------------------------------------------------------------
module cartela(xb, dx, h, ext) {
    rotate([90, 0, 0]) linear_extrude(cartela_t, center = true)
        polygon([[xb, -ext], [xb, h], [xb + dx, 0], [xb + dx, -ext]]);
}

module adiciones_modulo(ext) {
    xo = x0 + brida_x - oreja_t;   // cara de la oreja del lado del cuerpo
    // Oreja con ventana: el servo entra por aquí y la brida se apoya en ella
    difference() {
        translate([xo, -oreja_y, -ext]) cube([oreja_t, 2*oreja_y, oreja_z + ext]);
        translate([xo - 1, -(srv_L/2 + holgura), 0])
            cube([oreja_t + 2, srv_L + 2*holgura, srv_W + 2*holgura]);
        for (s = [-1, 1])
            translate([xo - 1, s*agujero_sep/2, srv_W/2])
                rotate([0, 90, 0]) cylinder(d = d_tornillo, h = oreja_t + 2, $fn = 16);
    }
    // Cartelas: rigidizan la oreja contra el torque
    for (s = [-1, 1]) translate([0, s*(oreja_y - cartela_t/2), 0]) {
        cartela(xo, -8, oreja_z - 2, ext);            // lado cuerpo
        cartela(xo + oreja_t, 6, 10, ext);            // lado brida (deja paso al destornillador)
    }
    // Tope trasero: registra el fondo del servo
    translate([x0 - 2.5, -5, -ext]) cube([2.2, 10, 4 + ext]);
}

module sustracciones_modulo() {
    // Ranuras para la correa de velcro / brida plástica que abraza el servo
    for (s = [-1, 1])
        translate([-12, s*(srv_L/2 + 2) - 1, -20]) cube([7, 2, 40]);
    // Ranuras para bridas plásticas del mazo de cables (corre a Y ≈ -23.5)
    for (y = [-19.75, -27.25])
        translate([-9, y - 0.75, -20]) cube([4, 1.5, 40]);
}

// ---------------------------------------------------------------------
//  Segmento de placa
// ---------------------------------------------------------------------
module placa(k) {
    L = L_seg(k);
    if ($curvo) {
        phi0 = s_ini(k) / R_out * 180 / PI;
        dphi = L / R_out * 180 / PI;
        rotate([0, 0, phi0]) rotate_extrude(angle = dphi, $fn = 180)
            translate([R_in, y_min]) square([t_placa, y_max - y_min]);
    } else {
        translate([0, y_min, -t_placa]) linear_extrude(t_placa)
            offset(r = r_esq) offset(delta = -r_esq) square([L, y_max - y_min]);
    }
}

module segmento(k) {
    n = segmentos[k];
    L = L_seg(k);
    ext = $curvo ? 3 : t_placa;
    difference() {
        union() {
            placa(k);
            difference() {
                union() for (j = [0:n-1]) colocar(k, margen + (j + 0.5)*paso) adiciones_modulo(ext);
                if ($curvo) cylinder(r = R_in, h = 400, center = true, $fn = 180);
            }
        }
        for (j = [0:n-1]) colocar(k, margen + (j + 0.5)*paso) sustracciones_modulo();
        // Agujeros de costura arriba y abajo
        for (xs = [5:paso_costura:L - 5]) for (y = [y_min + 3.5, y_max - 3.5])
            colocar(k, xs) translate([0, y, 0]) cylinder(d = d_costura, h = 20, center = true, $fn = 12);
        // Ranuras de extremo para correas de velcro de 25 mm al corsé
        for (xs = [4, L - 4]) colocar(k, xs) translate([0, -3, 0]) cube([3, 27, 20], center = true);
    }
}

// ---------------------------------------------------------------------
//  Acople cuerno–costilla (marco propio: brazo del cuerno a lo largo de +Y,
//  cara que toca el cuerno en z = 0). Imprimir acostado, sin soportes.
// ---------------------------------------------------------------------
module acople() {
    zc = acople_t / 2;
    difference() {
        hull() {
            translate([0, 8, 0]) cylinder(d = 8, h = acople_t);
            translate([0, cuerno_largo + acople_ext, 0]) cylinder(d = 6, h = acople_t);
        }
        // Bolsillo con la forma del brazo del cuerno
        translate([0, 0, -0.01]) linear_extrude(cuerno_t + 0.2) hull() {
            translate([0, 2]) circle(d = cuerno_w_base + 0.3);
            translate([0, cuerno_largo - cuerno_w_punta/2]) circle(d = cuerno_w_punta + 0.3);
        }
        // Tornillo M2 por el agujero exterior del cuerno
        translate([0, cuerno_largo - 2, -1]) cylinder(d = d_agujero_cuerno + 0.2, h = acople_t + 2, $fn = 16);
        // Alojamiento de la costilla (abierto en la punta)
        translate([0, cuerno_largo + 1, zc]) rotate([-90, 0, 0])
            cylinder(d = d_costilla + 0.2, h = acople_ext + 5, $fn = 16);
        // Prisionero M2 que aprieta la costilla
        translate([0, cuerno_largo + acople_ext - 4, zc]) cylinder(d = 1.6, h = acople_t, $fn = 16);
    }
}

// ---------------------------------------------------------------------
//  Bahía electrónica (va en la espalda): PCA9685 + buck 5–6 V + ESP32
// ---------------------------------------------------------------------
module bahia() {
    W = 100; H = 70;
    difference() {
        union() {
            linear_extrude(t_placa) offset(r = 4) offset(delta = -4) square([W, H]);
            // Separadores para la PCA9685
            for (dx = [0, pca_hole_dx], dy = [0, pca_hole_dy])
                translate([8 + dx, H - 30 + dy, 0]) cylinder(d = 6, h = t_placa + 5);
        }
        for (dx = [0, pca_hole_dx], dy = [0, pca_hole_dy])
            translate([8 + dx, H - 30 + dy, -1]) cylinder(d = pca_d_hole - 0.4, h = 20, $fn = 16);
        // Ranuras para bridas: buck (izq.) y ESP32 (der.), sin depender de sus agujeros
        for (x = [10, 40]) for (y = [6, 30]) translate([x, y, -1]) cube([4, 1.6, 10]);
        for (x = [55, 90]) for (y = [6, 30]) translate([x, y, -1]) cube([4, 1.6, 10]);
        // Ranuras laterales para velcro al corsé
        for (x = [3, W - 6]) translate([x, 15, -1]) cube([3, 40, 10]);
        // Costura perimetral
        for (x = [8:8:W - 8]) for (y = [3, H - 3]) translate([x, y, -1]) cylinder(d = d_costura, h = 10, $fn = 12);
    }
}

// ---------------------------------------------------------------------
//  Piezas de visualización
// ---------------------------------------------------------------------
module servo_dummy() {
    color("SteelBlue") {
        translate([x0, -srv_L/2, 0]) cube([srv_H, srv_L, srv_W]);
        translate([x0 + brida_x, -brida_span/2, 0.6]) cube([brida_t, brida_span, srv_W - 1.2]);
        translate([x0 + srv_H, eje_y, eje_z]) rotate([0, 90, 0]) cylinder(d = 11.8, h = 4);
    }
    color("Silver") translate([x0 + srv_H + 4, eje_y, eje_z]) rotate([0, 90, 0]) cylinder(d = 4.8, h = 2);
}

module tren_costilla(alfa) {
    // Marco del eje: rota alrededor de X; el cuerno cuelga hacia -Y
    translate([x_cuerno, eje_y, eje_z]) rotate([-alfa, 0, 0])
        multmatrix([[0, 0, 1, 0], [0, -1, 0, 0], [1, 0, 0, 0], [0, 0, 0, 1]]) {
            color("White") linear_extrude(cuerno_t) hull() {
                circle(d = 7);
                translate([0, cuerno_largo - cuerno_w_punta/2]) circle(d = cuerno_w_punta);
            }
            color("DimGray") acople();
            color("Black") translate([0, cuerno_largo + 1, acople_t/2]) rotate([-90, 0, 0])
                cylinder(d = d_costilla, h = largo_costilla, $fn = 8);
            if (mostrar_volantes)
                color("Crimson", 0.55) for (i = [0:3])
                    translate([0, cuerno_largo + 30 + i*35, acople_t/2])
                        rotate([0, 0, 0]) scale([0.15, 1, 1]) rotate([0, 90, 0])
                            linear_extrude(1, center = true) hull() {
                                circle(d = 4);
                                translate([12 - 4*i, 25]) circle(d = 4);
                                translate([-14 + 3*i, 28]) circle(d = 4);
                            }
        }
}

module ensamble() {
    $curvo = true;
    rotate([0, 0, -90]) {   // frente del torso hacia el observador (-Y)
        color("Wheat", 0.25) cylinder(r = R_torso, h = 300, center = true, $fn = 120);
        for (k = [0:len(segmentos) - 1]) {
            color("Gainsboro") segmento(k);
            for (j = [0:segmentos[k] - 1]) {
                i = servos_antes(k) + j;
                alfa = alfa0 + amplitud * sin(360*$t - i*fase);
                colocar(k, margen + (j + 0.5)*paso) {
                    servo_dummy();
                    tren_costilla(alfa);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------
//  Selector
// ---------------------------------------------------------------------
if (parte == "ensamble") ensamble();
else if (parte == "segmento") { $curvo = false; segmento(segmento_idx); }
else if (parte == "plano_completo") {
    $curvo = false;
    for (k = [0:len(segmentos) - 1]) translate([0, k*(y_max - y_min + 10), 0]) segmento(k);
    for (i = [0:n_servos - 1]) translate([i*10 + 5, -50, 0]) acople();
}
else if (parte == "acople") acople();
else if (parte == "bahia") bahia();
