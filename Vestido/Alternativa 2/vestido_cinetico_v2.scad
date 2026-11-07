// =====================================================================
//  VESTIDO CINÉTICO v2 — 7 servos + 7 costillas pasivas (14 costillas)
//  Inspirado en "Moments in Motion" (Lisa Jiang x Timothy A. Boll)
//
//  Idea: se alternan costillas MOTORIZADAS (servo MG90S) y PASIVAS
//  (bisagra + balancín + elástico). La organza cosida a todas las
//  costillas transmite el movimiento: la pasiva sigue a sus vecinas con
//  un pequeño retraso, y la onda se ve continua con la mitad de servos.
//
//  Marco local de cada módulo:
//     X = tangente al torso (a lo largo del arco)
//     Y = vertical (hacia arriba)
//     Z = radial hacia afuera (Z = 0 es la cara exterior de la placa)
//
//  IMPRESIÓN (sin soportes):
//   - "segmento_curvo": el segmento sale YA CURVO y DE PIE (el borde
//     inferior de la placa va sobre la cama). Todas las caras colgantes
//     están a ≤ 45° o son puentes cortos. Usa brim de 5–8 mm.
//   - "segmento_plano": alternativa para imprimir acostado y termoformar.
//   - "acople" y "balancin": acostados, sin soportes (imprime 7 + 7).
//
//  Animación: parte = "ensamble" o "detalle", Ver > Animar, FPS 10, Steps 60.
// =====================================================================

/* [Qué mostrar] */
parte = "ensamble"; // [ensamble, detalle, segmento_curvo, segmento_plano, kit, acople, balancin, bahia]
segmento_idx = 0;

/* [Distribución] */
segmentos = [4, 3]; // servos por segmento → 7 servos, 14 costillas. Cada segmento cabe en cama 220
paso      = 50;     // servo a servo; las costillas quedan cada paso/2 = 25 mm
margen    = 8;
gap_seg   = 4;
pasivas   = true;   // costilla pasiva entre cada par de servos

/* [Cuerpo] */
R_torso = 135; // radio del maniquí en la línea de servos (= contorno / 2π)
espuma  = 4;

/* [Placa] */
t_placa = 2.5;
y_min = -34;
y_max =  28;
r_esq = 4;
paso_costura = 8;
d_costura = 2;

/* [Servo MG90S — mide el tuyo] */
srv_L = 22.8;  srv_W = 12.2;  srv_H = 22.5;  srv_Htot = 28.5;
brida_x = 16;  brida_t = 2.5; brida_span = 32.5; agujero_sep = 27.8;
eje_off = 5.9; holgura = 0.2; d_tornillo = 1.6;

/* [Oreja del servo] */
oreja_t = 3;
oreja_y = 20;
oreja_z = 15.5;
chaflan = 3;    // refuerzos a 45° en la base de la oreja

/* [Túnel de cables] */
tunel_y = -23.5;
tunel_h = 5;
tunel_z = 4.5;

/* [Cuerno y acople (costilla activa)] */
cuerno_largo = 15; cuerno_t = 1.8; cuerno_w_base = 6; cuerno_w_punta = 4;
d_agujero_cuerno = 2; acople_t = 4; acople_ext = 12;
d_costilla = 1.5;
largo_costilla_act = 200;

/* [Costilla pasiva] */
piv_y = -28;       // pivote de la bisagra
piv_z = 10;
bis_r = 3.8;       // radio de la oreja de la bisagra alrededor del pivote
bis_t = 2.5;       // espesor de cada oreja de la bisagra
balancin_t = 4;
balancin_largo = 17;
cola = 5;          // cola del balancín donde se ata el elástico
d_pasador = 1.75;  // pasador = trozo de filamento de 1.75 mm
y_ancla = -19;     // agujero en la placa donde se ancla el elástico
largo_costilla_pas = 175;

/* [Bahía electrónica] */
pca_hole_dx = 56; pca_hole_dy = 19; pca_d_hole = 2.5;

/* [Animación] */
alfa0    = 25;   // reposo hacia afuera; con amplitud 20 nunca baja de 5°
amplitud = 20;
fase     = 40;   // desfase entre servos vecinos
seguimiento = 0.7; // cuánto sigue la pasiva a sus vecinas (lo da la tela)
retraso  = 25;     // retraso extra de la pasiva (°)
mostrar_tela  = true;
mostrar_hilos = true;

$fn = 32;

// ---------------------------------------------------------------------
//  Derivados
// ---------------------------------------------------------------------
n_servos = suma(segmentos);
x0    = -15;
eje_y = srv_L/2 - eje_off;
eje_z = srv_W/2;
x_cuerno = x0 + srv_Htot - 1.0;
xo    = x0 + brida_x - oreja_t;          // cara de la oreja del lado del cuerpo
x_act = x_cuerno + acople_t/2;           // plano de la costilla activa
x_pas = x_act - paso/2;                  // plano de la costilla pasiva

R_in  = R_torso + espuma;
R_out = R_in + t_placa;

function suma(v, i = 0) = i >= len(v) ? 0 : v[i] + suma(v, i + 1);
function L_seg(k) = 2*margen + segmentos[k]*paso;
function largo_total() = suma([for (k = [0:len(segmentos)-1]) L_seg(k)]) + gap_seg*(len(segmentos)-1);
function s_ini(k) = -largo_total()/2 + suma([for (i = [0:1:k-1]) L_seg(i) + gap_seg]);
function servos_antes(k) = suma([for (i = [0:1:k-1]) segmentos[i]]);
function centro_mod(j) = margen + (j + 0.5)*paso;

assert(paso >= (pasivas ? 50 : 36), "paso muy corto para el módulo (mín. 50 con pasivas, 36 sin)");

// Matriz de colocación (plano o sobre el cilindro del torso)
function M_col(k, xs) =
    $curvo ? let(phi = (s_ini(k) + xs)/R_out*180/PI, c = cos(phi), s = sin(phi))
             [[-s, 0, c, R_out*c], [c, 0, s, R_out*s], [0, 1, 0, 0], [0, 0, 0, 1]]
           : [[1, 0, 0, xs], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]];
function aplicar(M, p) = let(v = M*[p[0], p[1], p[2], 1]) [v[0], v[1], v[2]];
module colocar(k, xs) multmatrix(M_col(k, xs)) children();

// Extruye un perfil 2D dibujado en (Y, Z) a lo largo de +X, desde x, espesor t
module perfil_yz(x, t) translate([x, 0, 0]) rotate([90, 0, 90]) linear_extrude(t) children();
// Extruye un perfil 2D dibujado en (X, Z) a lo largo de -Y, desde y1 hasta y0
module perfil_xz(y0, y1) translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) children();

// ---------------------------------------------------------------------
//  Piezas del módulo
// ---------------------------------------------------------------------
module oreja(ext) {
    difference() {
        perfil_yz(xo, oreja_t) intersection() {
            union() {
                translate([-oreja_y, -ext]) square([2*oreja_y, oreja_z + ext]);
                // rampas a 45°: sin voladizo al imprimir de pie y más rigidez
                polygon([[-oreja_y, -ext], [-oreja_y, oreja_z], [-oreja_y - oreja_z, 0], [-oreja_y - oreja_z, -ext]]);
                polygon([[ oreja_y, -ext], [ oreja_y, oreja_z], [ oreja_y + oreja_z, 0], [ oreja_y + oreja_z, -ext]]);
            }
            translate([y_min, -ext]) square([y_max - y_min, 60]);
        }
        // Ventana por donde entra el servo (puente de 12.6 mm al imprimir de pie)
        translate([xo - 1, -(srv_L/2 + holgura), 0]) cube([oreja_t + 2, srv_L + 2*holgura, srv_W + 2*holgura]);
        // Agujeros para M2 autorroscante a través de la brida
        for (s = [-1, 1]) translate([xo - 1, s*agujero_sep/2, srv_W/2])
            rotate([0, 90, 0]) cylinder(d = d_tornillo, h = oreja_t + 2, $fn = 16);
    }
}

module chaflanes(ext) {
    // Triángulos a 45° en la base de la oreja, extruidos en Y (verticales al imprimir de pie)
    cuerpo = [[xo, -ext], [xo, chaflan], [xo - chaflan, 0], [xo - chaflan, -ext]];
    brida  = [[xo + oreja_t, -ext], [xo + oreja_t, chaflan], [xo + oreja_t + chaflan, 0], [xo + oreja_t + chaflan, -ext]];
    // Tramo inferior: arranca en la cama
    perfil_xz(y_min, -15) polygon(cuerpo);
    perfil_xz(y_min, -17) polygon(brida);
    // Tramo superior: su extremo de abajo termina en punta a 45° (no cuelga al imprimir de pie)
    hull() { perfil_xz(15 + chaflan, y_max - 1) polygon(cuerpo);
             translate([xo - 0.01, 15, -ext]) cube([0.01, 0.01, ext]); }
    hull() { perfil_xz(17 + chaflan, y_max - 1) polygon(brida);
             translate([xo + oreja_t, 17, -ext]) cube([0.01, 0.01, ext]); }
}

module tope(ext) {
    // Tope trasero del servo, achaflanado abajo
    perfil_yz(x0 - 2.5, 2.2) polygon([[-9, -ext], [5, -ext], [5, 4], [-5, 4], [-9, 0]]);
}

module bisagra(ext) {
    // Dos orejas con agujero para el pasador de filamento.
    // Apoyan sobre la cama en toda su base → sin voladizos.
    for (s = [-1, 1])
        perfil_yz(x_pas + s*(balancin_t/2 + 0.3 + bis_t/2) - bis_t/2, bis_t) difference() {
            hull() {
                translate([y_min, -ext]) square([piv_y - y_min, piv_z + bis_r + ext]);
                translate([piv_y, piv_z]) circle(r = bis_r);
                translate([piv_y + bis_r + 1, -ext]) square([0.01, ext]);
            }
            translate([piv_y, piv_z]) circle(d = d_pasador + 0.15, $fn = 20);
        }
}

module adiciones_modulo(ext) {
    oreja(ext);
    chaflanes(ext);
    tope(ext);
    if (pasivas) bisagra(ext);
}

module sustracciones_modulo() {
    // Correa de velcro / brida que abraza el servo
    for (s = [-1, 1]) translate([-12, s*(srv_L/2 + 2) - 1, -20]) cube([7, 2, 40]);
    // Túnel para el mazo de cables (atraviesa oreja, chaflanes y bisagra)
    translate([x0 - 3, tunel_y - tunel_h/2, 0])
        cube([xo + oreja_t + chaflan + 2 - (x0 - 3), tunel_h, tunel_z]);
    // Ranuras para bridas que sujetan el mazo
    for (y = [tunel_y + tunel_h/2 + 1.5, tunel_y - tunel_h/2 - 1.5])
        translate([5, y - 0.75, -20]) cube([4, 1.5, 40]);
    // Ancla del elástico de la costilla pasiva
    if (pasivas) translate([x_pas, y_ancla, 0]) cylinder(d = 1.6, h = 40, center = true, $fn = 12);
}

// Zonas (X local del módulo) donde NO van agujeros de costura
function libre(xs, k, y) =
    let(z = (y < 0) ? [x_pas - bis_t - 3.5, xo + oreja_t + chaflan + 1]
                    : [xo - chaflan - 1, xo + oreja_t + chaflan + 1])
    len([for (j = [0:segmentos[k]-1]) let(l = xs - centro_mod(j)) if (l > z[0] && l < z[1]) 1]) == 0;

// ---------------------------------------------------------------------
//  Segmento de placa
// ---------------------------------------------------------------------
module placa(k) {
    L = L_seg(k);
    if ($curvo) {
        phi0 = s_ini(k)/R_out*180/PI;
        dphi = L/R_out*180/PI;
        rotate([0, 0, phi0]) rotate_extrude(angle = dphi, $fn = 360)
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
                union() for (j = [0:n-1]) colocar(k, centro_mod(j)) adiciones_modulo(ext);
                if ($curvo) cylinder(r = R_in, h = 400, center = true, $fn = 240);
            }
        }
        for (j = [0:n-1]) colocar(k, centro_mod(j)) sustracciones_modulo();
        for (y = [y_min + 3.5, y_max - 3.5]) for (xs = [5:paso_costura:L - 5])
            if (libre(xs, k, y)) colocar(k, xs) translate([0, y, 0])
                cylinder(d = d_costura, h = 20, center = true, $fn = 12);
        for (xs = [4, L - 4]) colocar(k, xs) translate([0, -3, 0]) cube([3, 27, 20], center = true);
    }
}

// Segmento curvo en posición de impresión: de pie, borde inferior sobre la cama
module segmento_para_imprimir(k) {
    $curvo = true;
    phi_mid = (s_ini(k) + L_seg(k)/2)/R_out*180/PI;
    translate([-R_in, 0, -y_min]) rotate([0, 0, -phi_mid]) segmento(k);
}

// ---------------------------------------------------------------------
//  Acople cuerno–costilla (costilla activa). Imprimir acostado.
// ---------------------------------------------------------------------
module acople() {
    zc = acople_t/2;
    difference() {
        hull() {
            translate([0, 8, 0]) cylinder(d = 8, h = acople_t);
            translate([0, cuerno_largo + acople_ext, 0]) cylinder(d = 6, h = acople_t);
        }
        translate([0, 0, -0.01]) linear_extrude(cuerno_t + 0.2) hull() {
            translate([0, 2]) circle(d = cuerno_w_base + 0.3);
            translate([0, cuerno_largo - cuerno_w_punta/2]) circle(d = cuerno_w_punta + 0.3);
        }
        translate([0, cuerno_largo - 2, -1]) cylinder(d = d_agujero_cuerno + 0.2, h = acople_t + 2, $fn = 16);
        translate([0, cuerno_largo + 1, zc]) rotate([-90, 0, 0]) cylinder(d = d_costilla + 0.2, h = acople_ext + 5, $fn = 16);
        translate([0, cuerno_largo + acople_ext - 4, zc]) cylinder(d = 1.6, h = acople_t, $fn = 16);
    }
}

// ---------------------------------------------------------------------
//  Balancín de la costilla pasiva. Imprimir acostado.
//  Marco propio: pivote en el origen, brazo hacia +X, espesor en Z.
// ---------------------------------------------------------------------
module balancin() {
    difference() {
        linear_extrude(balancin_t) union() {
            hull() { circle(r = 3.6); translate([balancin_largo, 0]) circle(r = 2.6); }
            hull() { circle(r = 3.6); translate([-cola, 0]) circle(r = 2.2); }
        }
        translate([0, 0, -1]) cylinder(d = d_pasador + 0.45, h = balancin_t + 2, $fn = 20); // gira libre
        translate([-cola, 0, -1]) cylinder(d = 1.6, h = balancin_t + 2, $fn = 12);         // elástico
        translate([5, 0, balancin_t/2]) rotate([0, 90, 0]) cylinder(d = d_costilla + 0.2, h = balancin_largo, $fn = 16);
        translate([11, 0, balancin_t/2]) cylinder(d = 1.6, h = balancin_t, $fn = 12);      // prisionero M2
    }
}

// ---------------------------------------------------------------------
//  Bahía electrónica (espalda)
// ---------------------------------------------------------------------
module bahia() {
    W = 100; H = 70;
    difference() {
        union() {
            linear_extrude(t_placa) offset(r = 4) offset(delta = -4) square([W, H]);
            for (dx = [0, pca_hole_dx], dy = [0, pca_hole_dy])
                translate([8 + dx, H - 30 + dy, 0]) cylinder(d = 6, h = t_placa + 5);
        }
        for (dx = [0, pca_hole_dx], dy = [0, pca_hole_dy])
            translate([8 + dx, H - 30 + dy, -1]) cylinder(d = pca_d_hole - 0.4, h = 20, $fn = 16);
        for (x = [10, 40, 55, 90]) for (y = [6, 30]) translate([x, y, -1]) cube([4, 1.6, 10]);
        for (x = [3, W - 6]) translate([x, 15, -1]) cube([3, 40, 10]);
        for (x = [8:8:W - 8]) for (y = [3, H - 3]) translate([x, y, -1]) cylinder(d = d_costura, h = 10, $fn = 12);
    }
}

// ---------------------------------------------------------------------
//  Cinemática para la visualización
// ---------------------------------------------------------------------
function dir_cost(a) = [0, -cos(a), sin(a)];  // costilla colgando, inclinada a hacia afuera
function alfa_act(i) = alfa0 + amplitud*sin(360*$t - i*fase);
function alfa_pas(i) = alfa0 + seguimiento*amplitud*sin(360*$t - (i - 0.5)*fase - retraso);

// Lista ordenada de costillas a lo largo del arco: [k, xc, tipo(0=pasiva,1=activa), alfa]
function costillas(ks) = [for (k = ks) for (j = [0:segmentos[k]-1])
    let(i = servos_antes(k) + j, xc = centro_mod(j))
    each (pasivas ? [[k, xc, 0, alfa_pas(i)], [k, xc, 1, alfa_act(i)]] : [[k, xc, 1, alfa_act(i)]])];

// Punto de una costilla a la altura Y = yt (en coordenadas globales)
function punto_costilla(c, yt) =
    let(act = c[2] == 1,
        P = act ? [x_act, eje_y, eje_z] : [x_pas, piv_y, piv_z],
        d0 = act ? cuerno_largo + 1 : 5,
        L  = act ? largo_costilla_act : largo_costilla_pas,
        d  = min(max((P[1] - yt)/cos(c[3]), d0), d0 + L))
    aplicar(M_col(c[0], c[1]), P + d*dir_cost(c[3]));

module servo_dummy() {
    color("SteelBlue") {
        translate([x0, -srv_L/2, 0]) cube([srv_H, srv_L, srv_W]);
        translate([x0 + brida_x, -brida_span/2, 0.6]) cube([brida_t, brida_span, srv_W - 1.2]);
        translate([x0 + srv_H, eje_y, eje_z]) rotate([0, 90, 0]) cylinder(d = 11.8, h = 4);
    }
    color("Silver") for (s = [-1, 1]) translate([x0 + brida_x + brida_t, s*agujero_sep/2, srv_W/2])
        rotate([0, 90, 0]) cylinder(d = 3.5, h = 1.2, $fn = 12);   // cabezas de tornillo M2
}

module tren_activo(a) {
    translate([x_cuerno, eje_y, eje_z]) rotate([-a, 0, 0])
        multmatrix([[0, 0, 1, 0], [0, -1, 0, 0], [1, 0, 0, 0], [0, 0, 0, 1]]) {
            color("White") linear_extrude(cuerno_t) hull() {
                circle(d = 7);
                translate([0, cuerno_largo - cuerno_w_punta/2]) circle(d = cuerno_w_punta);
            }
            color("DimGray") acople();
            color("Black") translate([0, cuerno_largo + 1, acople_t/2]) rotate([-90, 0, 0])
                cylinder(d = d_costilla, h = largo_costilla_act, $fn = 8);
        }
}

module tren_pasivo(a) {
    // balancín + costilla
    translate([x_pas - balancin_t/2, piv_y, piv_z]) rotate([-a, 0, 0])
        multmatrix([[0, 0, 1, 0], [-1, 0, 0, 0], [0, -1, 0, 0], [0, 0, 0, 1]]) {
            color("Orange") balancin();
            color("Black") translate([5, 0, balancin_t/2]) rotate([0, 90, 0])
                cylinder(d = d_costilla, h = largo_costilla_pas, $fn = 8);
        }
    // pasador de filamento
    color("White") translate([x_pas - (balancin_t/2 + 0.3 + bis_t) - 0.5, piv_y, piv_z])
        rotate([0, 90, 0]) cylinder(d = d_pasador, h = balancin_t + 0.6 + 2*bis_t + 1, $fn = 12);
    // elástico de retorno: cola del balancín → ancla en la placa
    cola_p = [x_pas, piv_y + cola*cos(a), piv_z - cola*sin(a)];
    color("Gold") hull() { translate(cola_p) sphere(d = 1.4, $fn = 8); translate([x_pas, y_ancla, 0]) sphere(d = 1.4, $fn = 8); }
}

module tela_y_hilos(ks) {
    C = costillas(ks);
    // Paneles de organza entre costillas vecinas: es lo que acopla activa ↔ pasiva
    if (mostrar_tela) color("Crimson", 0.45) for (n = [0:len(C) - 2])
        hull() for (c = [C[n], C[n + 1]]) for (yt = [-45, -175])
            translate(punto_costilla(c, yt)) sphere(r = 0.4, $fn = 6);
    // Hilos elásticos opcionales entre costillas (refuerzan el acople)
    if (mostrar_hilos) color("Gold") for (n = [0:len(C) - 2]) for (yt = [-90, -140])
        hull() for (c = [C[n], C[n + 1]]) translate(punto_costilla(c, yt)) sphere(d = 0.8, $fn = 6);
}

module montaje(ks = [for (k = [0:len(segmentos) - 1]) k]) {
    for (k = ks) {
        color("Gainsboro") segmento(k);
        for (j = [0:segmentos[k] - 1]) {
            i = servos_antes(k) + j;
            colocar(k, centro_mod(j)) {
                servo_dummy();
                tren_activo(alfa_act(i));
                if (pasivas) tren_pasivo(alfa_pas(i));
            }
        }
    }
    tela_y_hilos(ks);
}

module ensamble() {
    $curvo = true;
    rotate([0, 0, -90]) {
        color("Wheat", 0.25) cylinder(r = R_torso, h = 320, center = true, $fn = 120);
        montaje();
    }
}

// ---------------------------------------------------------------------
//  Selector
// ---------------------------------------------------------------------
if (parte == "ensamble") ensamble();
else if (parte == "detalle") { $curvo = false; montaje([segmento_idx]); }  // un segmento, plano
else if (parte == "segmento_curvo") segmento_para_imprimir(segmento_idx);
else if (parte == "segmento_plano") { $curvo = false; segmento(segmento_idx); }
else if (parte == "acople") acople();
else if (parte == "balancin") balancin();
else if (parte == "bahia") bahia();
else if (parte == "kit") {
    for (k = [0:len(segmentos) - 1]) translate([k*240, 0, 0]) segmento_para_imprimir(k);
    for (i = [0:n_servos - 1]) {
        translate([i*12, -40, 0]) acople();
        translate([i*12 + 4, -75, 0]) rotate([0, 0, 90]) balancin();
    }
}
