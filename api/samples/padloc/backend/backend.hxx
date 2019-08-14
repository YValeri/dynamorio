

#ifndef BACKEND_INTERFLOP
#define BACKEND_INTERFLOP

#include "../backend_verrou/interflop_verrou.h"

namespace Interflop {

    static void *verrou_context;

    /**
     * \brief Init verrou backend with the random rounding mode
     */
    static void verrou_prepare(){
        interflop_verrou_init(&verrou_context);
        interflop_verrou_configure(VR_RANDOM, verrou_context);
    }

    /**
     * \brief End of use of verrou backend
     */
    static void verrou_end(){
        interflop_verrou_finalyze(verrou_context);
    }


    /**
     * \brief Class containing the implementations of the overloaded operations : add, sub, mul, div, fmadd, fmsub
     * The operations are defined for IEEE-754 single and double precision binary formats
     */
    template<typename PREC>
    class Op{
    };

    template<>
    struct Op<double>{
        
        /**
         * \brief add double precision
         * Redirection to verrou implementation
         */
        static double add(double a, double b){
            double res;
            interflop_verrou_add_double(a, b, &res, nullptr);
            return res;
        }

        /**
         * \brief sub double precision
         * Redirection to verrou implementation
         */
        static double sub(double a, double b){
            double res;
            interflop_verrou_sub_double(a, b, &res, nullptr);
            return res;
        }

        /**
         * \brief mul double precision
         * Redirection to verrou implementation
         */
        static double mul(double a, double b){
            double res;
            interflop_verrou_mul_double(a, b, &res, nullptr);
            return res;
        }

        /**
         * \brief div double precision
         * Redirection to verrou implementation
         */
        static double div(double a, double b){
            double res;
            interflop_verrou_div_double(a, b, &res, nullptr);
            return res;
        }

        /**
         * \brief fmadd double precision
         * if USE_VERROU_FMA is defined then the verrou fmadd function is called. Otherwise, use a combination of verrou_add and verrou_mul
         */
        static double fmadd(double a, double b, double c){
            double res;

            #ifdef USE_VERROU_FMA
                interflop_verrou_madd_double(a, b, c, &res, nullptr);
            #else
                double coeff;
                interflop_verrou_mul_double(a, b, &coeff, nullptr);
                interflop_verrou_add_double(coeff, c, &res, nullptr);
            #endif

            return res;
        }
        

        /**
         * \brief fmsub double precision
         * if USE_VERROU_FMA is defined then the verrou madd function is called. Otherwise, use a combination of verrou_sub and verrou_mul
         */
        static double fmsub(double a, double b, double c){
            double res;

            #ifdef USE_VERROU_FMA
                interflop_verrou_madd_double(a, b, -1*c, &res, nullptr);
            #else
                double coeff;
                interflop_verrou_mul_double(a, b, &coeff, nullptr);
                interflop_verrou_sub_double(coeff, c, &res, nullptr);
            #endif

            return res;
        }
        
         /**
         * \brief nfmadd double precision
         * if USE_VERROU_FMA is defined then the verrou madd function is called. Otherwise, use a combination of verrou_add and verrou_mul
         */
        static double nfmadd(double a, double b, double c){
            double res;

            #ifdef USE_VERROU_FMA
                interflop_verrou_madd_double(-a, b, c, &res, nullptr);
            #else
                double coeff;
                interflop_verrou_mul_double(a, b, &coeff, nullptr);
                interflop_verrou_add_double(-coeff, c, &res, nullptr);
            #endif
            return res;
        }


         /**
         * \brief nfmsub double precision
         * if USE_VERROU_FMA is defined then the verrou madd function is called. Otherwise, use a combination of verrou_sub and verrou_mul
         */
        static double nfmsub(double a, double b, double c){
            double res;

            #ifdef USE_VERROU_FMA
                interflop_verrou_madd_double(-a, b, -c, &res, nullptr);
            #else
                double coeff;
                interflop_verrou_mul_double(a, b, &coeff, nullptr);
                interflop_verrou_sub_double(-coeff, c, &res, nullptr);
            #endif

            return res;
        }
    };


    template<>
    struct Op<float>{

        /**
         * \brief add single precision
         * Redirection to verrou implementation
         */
        static float add(float a, float b){
            float res;
            interflop_verrou_add_float(a, b, &res, nullptr);
            return res;
        }

         /**
         * \brief sub single precision
         * Redirection to verrou implementation
         */
        static float sub(float a, float b){
            float res;
            interflop_verrou_sub_float(a, b, &res, nullptr);
            return res;
        }

         /**
         * \brief mul single precision
         * Redirection to verrou implementation
         */
        static float mul(float a, float b){
            float res;
            interflop_verrou_mul_float(a, b, &res, nullptr);
            return res;
        }

         /**
         * \brief div single precision
         * Redirection to verrou implementation
         */
        static float div(float a, float b){
            float res;
            interflop_verrou_div_float(a, b, &res, nullptr);
            return res;
        }

        /**
         * \brief fmadd single precision
         * if USE_VERROU_FMA is defined then the verrou fmadd function is called. Otherwise, use a combination of verrou_add and verrou_mul
         */
        static float fmadd(float a, float b, float c){
            float res;

        #ifdef USE_VERROU_FMA
            interflop_verrou_madd_float(a, b, c, &res, nullptr);
        #else
            float coeff;
            interflop_verrou_mul_float(a, b, &coeff, nullptr);
            interflop_verrou_add_float(coeff, c, &res, nullptr);
        #endif

            return res;
        }

        /**
         * \brief fmsub single precision
         * if USE_VERROU_FMA is defined then the verrou fmadd function is called. Otherwise, use a combination of verrou_sub and verrou_mul
         */
        static float fmsub(float a, float b, float c){
            float res;

        #ifdef USE_VERROU_FMA
            interflop_verrou_madd_float(a, b, -1*c, &res, nullptr);
        #else
            float coeff;
            interflop_verrou_mul_float(a, b, &coeff, nullptr);
            interflop_verrou_sub_float(coeff, c, &res, nullptr);
        #endif

            return res;
        }

        /**
         * \brief nfmadd single precision
         * if USE_VERROU_FMA is defined then the verrou fmadd function is called. Otherwise, use a combination of verrou_add and verrou_mul
         */
        static float nfmadd(float a, float b, float c){
            float res;

        #ifdef USE_VERROU_FMA
            interflop_verrou_madd_float(-a, b, c, &res, nullptr);
        #else
            float coeff;
            interflop_verrou_mul_float(a, b, &coeff, nullptr);
            interflop_verrou_add_float(-coeff, c, &res, nullptr);
        #endif
            return res;
        }
        
        /**
         * \brief nfmsub single precision
         * if USE_VERROU_FMA is defined then the verrou fmadd function is called. Otherwise, use a combination of verrou_sub and verrou_mul
         */
        static float nfmsub(float a, float b, float c){
            float res;

        #ifdef USE_VERROU_FMA
            interflop_verrou_madd_float(-a, b, -c, &res, nullptr);
        #else
            float coeff;
            interflop_verrou_mul_float(a, b, &coeff, nullptr);
            interflop_verrou_sub_float(-coeff, c, &res, nullptr);
        #endif

            return res;
        }
    };
}

#endif