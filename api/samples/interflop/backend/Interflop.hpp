#pragma once
#include "interflop_backend_interface.h"

#include "interflop_verrou.h"

namespace Interflop
{
    class Backend{
    public:
    static Backend& getInstance(){
        static Backend instance;
        return instance;
    }


    private:
    Backend(){interface = interflop_verrou_init(&context);};
    ~Backend()= default;
    Backend(const Backend&)= delete;
    Backend& operator=(const Backend&)= delete;

    template<typename T>
    friend class Op;


    // Context & Interface, as defined by the Interflop standard
    void *context;
    interflop_backend_interface_t interface;
    
    };
    template<typename FTYPE>
    struct Op{};

    template<>
    struct Op<double>{
        static double add(double a, double b)
        {
            double res;
            interflop_verrou_add_double(a,b,&res, NULL);
            return res;
        }
        static double sub(double a, double b)
        {
            double res;
            interflop_verrou_sub_double(a,b,&res,NULL);
            return res;
        }
        static double mul(double a, double b)
        {
            double res;
            interflop_verrou_mul_double(a,b,&res,NULL);
            return res;
        }
        static double div(double a, double b)
        {
            double res;
            interflop_verrou_div_double(a,b,&res,NULL);
            return res;
        }
    };

    template<>
    struct Op<float>{
        static float add(float a, float b)
        {
            float res;
            interflop_verrou_add_float(a,b,&res,NULL);
            return res;
        }
        static float sub(float a, float b)
        {
            float res;
            interflop_verrou_sub_float(a,b,&res,NULL);
            return res;
        }
        static float mul(float a, float b)
        {
            float res;
            interflop_verrou_mul_float(a,b,&res,NULL);
            return res;
        }
        static float div(float a, float b)
        {
            float res;
            interflop_verrou_div_float(a,b,&res,NULL);
            return res;
        }
    };
}