// =====================================================================
//  VESTIDO CINÉTICO — versión con MOTORREDUCTOR N20 + ÁRBOL DE LEVAS
//  Placa PLANA (peto frontal). Un solo motor mueve 14 costillas.
//
//  Mecanismo (visto de lado, plano Y–Z):
//    - Eje de levas de acero 3 mm con un plano (eje "D"), girado por un N20.
//    - Cada leva es un disco EXCÉNTRICO; cada una va desfasada `fase` grados
//      → la onda viaja a lo largo del panel.
//    - Encima del eje de pivote (acero 2 mm) cuelga un BALANCÍN por costilla:
//      su brazo plano se apoya debajo de la leva (seguidor de cara plana).
//      Cuando la leva empuja el brazo hacia abajo, la costilla sale hacia afuera.
//    - El peso de la costilla + un elástico hacia la placa mantienen el brazo
//      siempre apoyado en la leva.
//
//  Marco: X = a lo largo del panel, Y = vertical (arriba), Z = hacia afuera.
//  Todo se imprime SIN SOPORTES:
//    placa (acostada), levas y balancines (cara grande sobre la cama, cubo hacia
//    arriba), separadores, acople (de pie), plantilla de fase.
//
//  Animación: parte = "ensamble", Ver > Animar, FPS 15, Steps 72.
// =====================================================================

/* [Qué mostrar] */
parte = "ensamble"; // [ensamble, placa, levas, balancines, separadores, acople_motor, plantilla]

/* [Distribución] */
n    = 14;        // costillas = levas = balancines
paso = 11.5;      // separación entre costillas
cada = 4;         // un soporte de ejes cada `cada` costillas
fase = 360/14;    // desfase entre levas vecinas (360/n = una onda completa en el panel)

/* [Placa] */
t_placa = 2.5;
y_min = -34;
y_max = 20;
margen = 3;
r_esq = 4;
y_ancla = -24;    // agujeros para anclar el elástico de cada costilla

/* [Eje de levas] */
d_eje  = 3;       // varilla de acero plata 3 mm
eje_D  = true;    // true: eje con un plano limado (D). false: eje redondo + CA con la plantilla
d_plano = 0.5;    // profundidad del plano (eje D estándar de 3 mm)
zc     = 12;      // altura del eje de levas sobre la placa

/* [Levas excéntricas] */
R_leva = 7.5;     // radio del disco
e_leva = 3;       // excentricidad → carrera = 2·e
t_leva = 4;
r_cubo_leva = 3.5;

/* [Balancín (seguidor)] */
d_piv  = 2;       // eje de pivote: varilla 2 mm (un radio de bicicleta sirve)
L_piv  = 13;      // distancia en Z del eje de levas al pivote
t_bal  = 4;
d_brazo = 1.5;    // semiespesor del brazo que toca la leva
L_brazo = 16;
r_cubo_bal = 3;
alfa_b = 8;       // inclinación de la costilla hacia afuera con la leva en reposo
L_cost_brazo = 14;
w_cost_brazo = 5;

/* [Soportes] */
t_sup = 3;
rodamientos = false; // true: alojamiento para rodamiento 683 (3x7x3) en el eje de levas

/* [Motor N20] */
n20_w = 12; n20_h = 10; n20_L = 24;   // cuerpo + reductora
n20_eje = 10;                         // largo del eje D de 3 mm
n20_sep = 9;                          // agujeros M1.6 en la cara de la reductora
ac_L = 14; ac_D = 8;                  // acople motor → eje

/* [Costillas y tela] */
d_costilla = 1.5;
largo_costilla = 170;
mostrar_tela = true;

$fn = 40;

// ---------------------------------------------------------------------
//  Derivados
// ---------------------------------------------------------------------
P_y = -(R_leva + d_brazo - e_leva);   // pivote: con la leva en reposo el brazo toca justo
P_z = zc + L_piv;

x_m_back = margen;
x_face   = margen + n20_L;
ac_x0    = x_face + 2.5;
ac_x1    = ac_x0 + ac_L;
b0       = ac_x1 + 0.5 + t_sup/2;
function b(j)   = b0 + j*paso;               // fronteras entre celdas (aquí van los soportes)
function xc(i)  = b0 + (i + 0.5)*paso;       // plano de la leva/balancín i
function sup(j) = (j % cada == 0) || j == n;
L_placa  = b(n) + t_sup/2 + margen;

// Cubo a la izquierda de cada leva/balancín: llena la celda (se autoespacian)
function cubo(i) = sup(i) ? paso/2 - t_leva/2 - t_sup/2 - 0.3 : paso - t_leva - 0.4;
g_sep = paso/2 - t_leva/2 - t_sup/2 - 0.4;   // largo de los separadores junto a cada soporte

x_eje0 = ac_x0 + ac_L/2 + 0.5;
x_eje1 = b(n) + t_sup/2 + 2;
x_piv0 = b(0) - t_sup/2 - 1;

assert(norm([0 - P_y, zc - P_z]) >= R_leva + e_leva + r_cubo_bal + 0.5, "la leva choca con el cubo del balancín: sube L_piv");
assert(zc >= R_leva + e_leva + 1.5, "la leva toca la placa: sube zc");
assert(paso >= t_leva + t_sup + 2, "paso muy corto para leva + soporte");
assert(L_placa <= 218, str("la placa mide ", L_placa, " mm: no cabe en cama de 220"));

// Ángulo exacto del balancín (seguidor de cara plana sobre leva excéntrica)
function theta(phi) =
    let(Ey = e_leva*cos(phi), Ez = zc + e_leva*sin(phi),
        D = [Ey - P_y, Ez - P_z], m = norm(D))
    atan2(D[1], D[0]) + acos((R_leva + d_brazo)/m);

// ---------------------------------------------------------------------
//  Utilidades
// ---------------------------------------------------------------------
// Extruye un perfil 2D dibujado en (Y, Z) a lo largo de +X, desde x, espesor t
module perfil_yz(x, t) translate([x, 0, 0]) rotate([90, 0, 90]) linear_extrude(t) children();

module agujero_eje(ang, h = 0.15) {
    if (eje_D) intersection() {
        circle(d = d_eje + h, $fn = 32);
        rotate(ang) translate([-10 + d_eje/2 - d_plano + 0.05, -10]) square([10, 20]);
    } else circle(d = d_eje + 0.2, $fn = 32);
}

// ---------------------------------------------------------------------
//  Leva i (marco de montaje: eje en el origen, excéntrica hacia +Y,
//  plano del eje D a i·fase; cubo hacia -X). Número grabado en la cara.
// ---------------------------------------------------------------------
module leva(i, cubo_izq, holg = 0) {
    difference() {
        union() {
            perfil_yz(-t_leva/2 - holg, t_leva + 2*holg) translate([e_leva, 0]) circle(r = R_leva + holg, $fn = 64);
            perfil_yz(-t_leva/2 - cubo_izq, cubo_izq + 0.01) circle(r = r_cubo_leva + holg);
        }
        if (holg == 0) {
            perfil_yz(-t_leva/2 - cubo_izq - 1, t_leva + cubo_izq + 2) agujero_eje(i*fase);
            perfil_yz(-t_leva/2 - 0.01, 0.5) translate([7.2, 0]) rotate(90)
                text(str(i + 1), size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
        }
    }
}

// ---------------------------------------------------------------------
//  Balancín (marco: pivote en el origen; brazo de leva hacia -Z con su
//  cara superior a +d_brazo; brazo de costilla hacia abajo-afuera)
// ---------------------------------------------------------------------
module balancin(cubo_izq) {
    difference() {
        union() {
            perfil_yz(-t_bal/2, t_bal) {
                circle(r = r_cubo_bal);
                translate([-d_brazo, -L_brazo]) square([2*d_brazo, L_brazo]);
                hull() {
                    circle(r = r_cubo_bal);
                    rotate(180 - alfa_b) translate([L_cost_brazo, 0]) circle(d = w_cost_brazo);
                }
            }
            perfil_yz(-t_bal/2 - cubo_izq, cubo_izq + 0.01) circle(r = r_cubo_bal);
        }
        perfil_yz(-t_bal/2 - cubo_izq - 1, t_bal + cubo_izq + 2) circle(d = d_piv + 0.3, $fn = 24);
        // alojamiento de la costilla (abierto en la punta)
        rotate([180 - alfa_b, 0, 0]) translate([0, 4.5, 0]) rotate([-90, 0, 0])
            cylinder(d = d_costilla + 0.2, h = L_cost_brazo + 5, $fn = 16);
        // prisionero M2
        rotate([180 - alfa_b, 0, 0]) translate([-t_bal, 10, 0]) rotate([0, 90, 0])
            cylinder(d = 1.6, h = 2*t_bal, $fn = 12);
    }
}

// ---------------------------------------------------------------------
//  Soporte de ejes (pared vertical sobre la placa)
// ---------------------------------------------------------------------
module soporte(ext) {
    difference() {
        perfil_yz(-t_sup/2, t_sup) hull() {
            translate([-12, -ext]) square([20, ext]);
            translate([0, zc]) circle(r = 4.5);
            translate([P_y, P_z]) circle(r = 3.5);
        }
        perfil_yz(-t_sup/2 - 1, t_sup + 2) translate([0, zc])
            circle(d = rodamientos ? 7.1 : d_eje + 0.35, $fn = 32);
        perfil_yz(-t_sup/2 - 1, t_sup + 2) translate([P_y, P_z]) circle(d = d_piv + 0.1, $fn = 24);
    }
}

// ---------------------------------------------------------------------
//  Soporte del motor N20: dos cunas en U + pared frontal atornillada
// ---------------------------------------------------------------------
module soporte_motor(ext) {
    for (x = [x_m_back + 3, x_face - 8]) perfil_yz(x, 3) difference() {
        translate([-n20_w/2 - 2, -ext]) square([n20_w + 4, zc + n20_h/2 - 2 + ext]);
        translate([-n20_w/2 - 0.2, zc - n20_h/2 - 0.2]) square([n20_w + 0.4, n20_h + 5]);
    }
    difference() {
        perfil_yz(x_face, 2) translate([-n20_w/2 - 2, -ext]) square([n20_w + 4, zc + n20_h/2 + 2 + ext]);
        perfil_yz(x_face - 1, 4) translate([0, zc]) circle(d = 4.5);
        for (s = [-1, 1]) perfil_yz(x_face - 1, 4) translate([s*n20_sep/2, zc]) circle(d = 1.7, $fn = 16);
    }
}

// ---------------------------------------------------------------------
//  Placa
// ---------------------------------------------------------------------
module placa() {
    difference() {
        union() {
            translate([0, y_min, -t_placa]) linear_extrude(t_placa)
                offset(r = r_esq) offset(delta = -r_esq) square([L_placa, y_max - y_min]);
            soporte_motor(t_placa);
            for (j = [0:n]) if (sup(j)) translate([b(j), 0, 0]) soporte(t_placa);
        }
        // anclas del elástico de cada costilla
        for (i = [0:n-1]) translate([xc(i), y_ancla, 0]) cylinder(d = 1.6, h = 20, center = true, $fn = 12);
        // bridas que abrazan el motor
        for (x = [x_m_back + 8, x_face - 13]) for (s = [-1, 1])
            translate([x, s*(n20_w/2 + 1.8) - 0.8, -10]) cube([3, 1.6, 20]);
        // costura perimetral
        for (y = [y_min + 3.5, y_max - 3.5]) for (x = [5:8:L_placa - 5])
            translate([x, y, 0]) cylinder(d = 2, h = 20, center = true, $fn = 12);
        // ranuras para velcro al corsé
        for (x = [L_placa*0.3, L_placa*0.75]) translate([x, y_max - 8, 0]) cube([20, 3, 20], center = true);
    }
}

// ---------------------------------------------------------------------
//  Piezas sueltas
// ---------------------------------------------------------------------
module acople_motor() {   // eje del acople en +Z (se imprime de pie)
    difference() {
        cylinder(d = ac_D, h = ac_L);
        translate([0, 0, -1]) linear_extrude(ac_L/2 + 1) agujero_eje(0);
        translate([0, 0, ac_L/2]) linear_extrude(ac_L/2 + 1) agujero_eje(0);
        for (z = [ac_L*0.25, ac_L*0.75]) translate([0, 0, z]) rotate([0, 90, 0]) cylinder(d = 1.6, h = ac_D, $fn = 12);
    }
}

module separador(d_in, d_out) difference() {
    cylinder(d = d_out, h = g_sep);
    translate([0, 0, -1]) cylinder(d = d_in, h = g_sep + 2, $fn = 24);
}

// Plantilla: bloque con la huella de todas las levas en su fase correcta
// (sólo si usas eje redondo y pegas las levas con CA)
module plantilla() {
    difference() {
        translate([b(0) - t_sup/2, -13, zc - 13]) cube([b(n) - b(0) + t_sup, 26, 13]);
        for (i = [0:n-1]) translate([xc(i), 0, zc]) rotate([-i*fase, 0, 0]) leva(i, cubo(i), 0.3);
        translate([b(0) - 5, 0, zc]) rotate([0, 90, 0]) cylinder(d = d_eje + 0.4, h = L_placa);
        // marcas de número de leva
        for (i = [0:n-1]) translate([xc(i), -12, zc - 0.6]) linear_extrude(1)
            text(str(i + 1), size = 3, halign = "center", font = "Liberation Sans:style=Bold");
    }
}

// Orientación de impresión: cara grande sobre la cama, cubo hacia arriba
module para_imprimir(t) rotate([0, 90, 0]) translate([-t/2, 0, 0]) children();

// ---------------------------------------------------------------------
//  Ensamble animado
// ---------------------------------------------------------------------
function rot_yz(v, a) = [v[0]*cos(a) - v[1]*sin(a), v[0]*sin(a) + v[1]*cos(a)];
function punto_costilla(i, yt) =
    let(th = theta(360*$t - i*fase),
        dv = rot_yz([-cos(alfa_b), sin(alfa_b)], th),
        d0 = 4.5, d = min(max((P_y - yt)/(-dv[0]), d0), d0 + largo_costilla))
    [xc(i), P_y + d*dv[0], P_z + d*dv[1]];

module ensamble() {
    psi = 360*$t;
    color("Gainsboro") placa();
    // motor N20
    color("Silver") translate([x_m_back, -n20_w/2, zc - n20_h/2]) cube([n20_L - 9, n20_w, n20_h]);
    color("Goldenrod") translate([x_face - 9, -n20_w/2, zc - n20_h/2]) cube([9, n20_w, n20_h]);
    color("Silver") translate([x_face, 0, zc]) rotate([psi, 0, 0]) rotate([0, 90, 0]) cylinder(d = d_eje, h = n20_eje);
    color("DimGray") translate([ac_x0, 0, zc]) rotate([psi, 0, 0]) rotate([0, 90, 0]) acople_motor();
    // ejes de acero
    color("LightSteelBlue") translate([x_eje0, 0, zc]) rotate([0, 90, 0]) cylinder(d = d_eje, h = x_eje1 - x_eje0);
    color("LightSteelBlue") translate([x_piv0, P_y, P_z]) rotate([0, 90, 0]) cylinder(d = d_piv, h = x_eje1 - x_piv0);
    for (i = [0:n-1]) {
        phi = psi - i*fase;
        th  = theta(phi);
        color([0.95, 0.55 + 0.35*cos(i*fase), 0.15]) translate([xc(i), 0, zc]) rotate([phi, 0, 0]) leva(i, cubo(i));
        translate([xc(i), P_y, P_z]) rotate([th, 0, 0]) {
            color("SteelBlue") balancin(cubo(i));
            color("Black") rotate([180 - alfa_b, 0, 0]) translate([0, 4.5, 0]) rotate([-90, 0, 0])
                cylinder(d = d_costilla, h = largo_costilla, $fn = 8);
        }
        // elástico: costilla → ancla en la placa
        color("Gold") hull() {
            translate(punto_costilla(i, y_ancla - 2)) sphere(d = 1, $fn = 8);
            translate([xc(i), y_ancla, 0]) sphere(d = 1, $fn = 8);
        }
    }
    // separadores junto a los soportes
    color("White") for (j = [1:n]) if (sup(j)) {
        translate([b(j) - t_sup/2 - 0.2 - g_sep, 0, zc]) rotate([0, 90, 0]) separador(d_eje + 0.3, 7);
        translate([b(j) - t_sup/2 - 0.2 - g_sep, P_y, P_z]) rotate([0, 90, 0]) separador(d_piv + 0.3, 5.5);
    }
    // organza entre costillas
    if (mostrar_tela) color("Crimson", 0.45) for (i = [0:n-2])
        hull() for (k = [i, i + 1]) for (yt = [-45, -175]) translate(punto_costilla(k, yt)) sphere(r = 0.4, $fn = 6);
}

// Para comprobar choques: levas contra todo lo demás (debe dar volumen ~0)
module levas_montadas() for (i = [0:n-1]) translate([xc(i), 0, zc]) rotate([360*$t - i*fase, 0, 0]) leva(i, cubo(i));
module resto_montado() {
    placa();
    for (i = [0:n-1]) translate([xc(i), P_y, P_z]) rotate([theta(360*$t - i*fase), 0, 0]) balancin(cubo(i));
}

// ---------------------------------------------------------------------
//  Selector
// ---------------------------------------------------------------------
// Corte lateral: una sola celda sin soportes, para entender el mecanismo
module corte(i = 1) {
    psi = 360*$t; phi = psi - i*fase; th = theta(phi);
    color("Gainsboro") intersection() { placa(); translate([xc(i) - 3, -100, -10]) cube([6, 200, 100]); }
    color("LightSteelBlue") translate([xc(i) - 6, 0, zc]) rotate([0, 90, 0]) cylinder(d = d_eje, h = 12);
    color("LightSteelBlue") translate([xc(i) - 6, P_y, P_z]) rotate([0, 90, 0]) cylinder(d = d_piv, h = 12);
    color("OrangeRed") translate([xc(i), 0, zc]) rotate([phi, 0, 0]) leva(i, cubo(i));
    translate([xc(i), P_y, P_z]) rotate([th, 0, 0]) {
        color("SteelBlue") balancin(cubo(i));
        color("Black") rotate([180 - alfa_b, 0, 0]) translate([0, 4.5, 0]) rotate([-90, 0, 0]) cylinder(d = d_costilla, h = 60, $fn = 8);
    }
    color("Gold") hull() { translate(punto_costilla(i, y_ancla - 2)) sphere(d = 1, $fn = 8); translate([xc(i), y_ancla, 0]) sphere(d = 1, $fn = 8); }
}

if (parte == "corte") corte();
else if (parte == "test_choques") intersection() { levas_montadas(); resto_montado(); }
else if (parte == "test_balancines") intersection() {   // balancines contra placa y soportes
    placa();
    for (i = [0:n-1]) translate([xc(i), P_y, P_z]) rotate([theta(360*$t - i*fase), 0, 0]) balancin(cubo(i));
}
else if (parte == "ensamble") ensamble();
else if (parte == "placa") translate([0, 0, t_placa]) placa();
else if (parte == "levas")
    for (i = [0:n-1]) translate([(i % 7)*26, floor(i/7)*26, 0]) para_imprimir(t_leva) leva(i, cubo(i));
else if (parte == "balancines")
    for (i = [0:n-1]) translate([(i % 7)*24, floor(i/7)*30, 0]) para_imprimir(t_bal) balancin(cubo(i));
else if (parte == "separadores") {
    for (j = [1:n]) if (sup(j)) translate([j*4, 0, 0]) {
        separador(d_eje + 0.3, 7);
        translate([0, 10, 0]) separador(d_piv + 0.3, 5.5);
    }
}
else if (parte == "acople_motor") acople_motor();
else if (parte == "plantilla") translate([0, 0, 13 - zc]) plantilla();
