#include "interflop_backend_interface.h"

namespace Interflop
{
    struct interflop_backend_interface_t config;
    void* context;
    template<typename FTYPE>
    struct Op{};

    template<>
    struct Op<double>{
        static double add(double a, double b)
        {
            double res;
            config.interflop_add_double(a,b,&res, context);
            return res;
        }
        static double sub(double a, double b)
        {
            double res;
            config.interflop_sub_double(a,b,&res, context);
            return res;
        }
        static double mul(double a, double b)
        {
            double res;
            config.interflop_mul_double(a,b,&res, context);
            return res;
        }
        static double div(double a, double b)
        {
            double res;
            config.interflop_div_double(a,b,&res, context);
            return res;
        }
    };

    template<>
    struct Op<float>{
        static float add(float a, float b)
        {
            float res;
            config.interflop_add_float(a,b,&res, context);
            return res;
        }
        static float sub(float a, float b)
        {
            float res;
            config.interflop_sub_float(a,b,&res, context);
            return res;
        }
        static float mul(float a, float b)
        {
            float res;
            config.interflop_mul_float(a,b,&res, context);
            return res;
        }
        static float div(float a, float b)
        {
            float res;
            config.interflop_div_float(a,b,&res, context);
            return res;
        }
    };
}