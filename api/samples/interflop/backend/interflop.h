/* interflop backend interface */
#ifndef INTERFLOP_BACKEND_INTERFACE
#define INTERFLOP_BACKEND_INTERFACE
struct interflop_backend_interface_t {
  void (*interflop_add_float)(float, float, float*, void*);
  void (*interflop_sub_float)(float, float, float*, void*);
  void (*interflop_mul_float)(float, float, float*, void*);
  void (*interflop_div_float)(float, float, float*, void*);
  void (*interflop_fma_float)(float, float, float, float*, void*);

  void (*interflop_add_double)(double, double, double*, void*);
  void (*interflop_sub_double)(double, double, double*, void*);
  void (*interflop_mul_double)(double, double, double*, void*);
  void (*interflop_div_double)(double, double, double*, void*);
  void (*interflop_fma_double)(double, double, double, double*, void*);
};

/* interflop_init: called at initialization before using a backend.
 * It returns an interflop_backend_interface_t structure with callbacks
 * for each of the numerical instrument hooks */

struct interflop_backend_interface_t interflop_init(void ** context);
#endif