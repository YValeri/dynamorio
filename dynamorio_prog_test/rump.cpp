#include <iostream>
#include <cmath>
 
typedef double FTYPE;
 
int main(int argc, char const *argv[]) {
    FTYPE x = 77617.0;
    FTYPE y = 33096.0;

    FTYPE y8 = y*y*y*y*y*y*y*y;
    FTYPE y6 = y*y*y*y*y*y;
    FTYPE y4 = y*y*y*y;
    FTYPE y2 = y*y;

    FTYPE x2 = x*x;

    FTYPE res = ((FTYPE)(333.75)*(y6))+((x2)*((FTYPE)(11.0)*(x2)*(y2)-(y6)-(FTYPE)(121)*(y4)-(FTYPE)(2.0)))+((FTYPE)(5.5)*(y8))+(x/((FTYPE)(2.0)*y));
    std::cout << std::scientific << res << std::endl;
    std::cout << res << std::endl;
    return 0;
}