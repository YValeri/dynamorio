#pragma once

#include "../backend_verrou/interflop_verrou.h"

namespace Interflop {
// Base backend operations are only defined for the IEEE-754 single- and
// double-precision binary formats
template <typename PREC>
class Op {};

template <>
struct Op<double> {
  static double add (double a, double b) {
    double res;
    interflop_verrou_add_double(a,b,&res , NULL);
    return res;
  }

  static double sub (double a, double b) {
    double res;
    interflop_verrou_sub_double(a,b,&res , NULL);
    return res;
  }

  static double mul (double a, double b) {
    double res;
    interflop_verrou_mul_double(a,b,&res , NULL);
    return res;
  }

  static double div (double a, double b) {
    double res;
    interflop_verrou_div_double(a,b,&res , NULL);
    return res;
  }
};


template <>
struct Op<float> {
  static float add (float a, float b) {
    float res;
    interflop_verrou_add_float(a,b,&res , NULL);
    return res;
  }

  static float sub (float a, float b) {
    float res;
    interflop_verrou_sub_float(a,b,&res , NULL);
    return res;
  }

  static float mul (float a, float b) {
    float res;
    interflop_verrou_mul_float(a,b,&res , NULL);
    return res;
  }

  static float div (float a, float b) {
    float res;
    interflop_verrou_div_float(a,b,&res , NULL);
    return res;
  }
};

}
