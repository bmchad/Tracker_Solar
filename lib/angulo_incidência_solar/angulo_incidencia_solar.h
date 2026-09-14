#ifndef ANGULO_INCIDENCIA_SOLAR_H
#define ANGULO_INCIDENCIA_SOLAR_H

#include <stdio.h>
#include <math.h>

double clamp_dominio(double x);   /* D-04: satura em [-1,+1] antes de acos/asin */
double deg2rad(double deg);
double rad2deg(double rad);
double arredondar_para_multiplo_de_5(double theta);
double calcular_theta(double HL, double gama_p);

#endif
