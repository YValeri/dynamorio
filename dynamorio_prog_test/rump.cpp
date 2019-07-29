#include <iostream>
#include <cmath>

typedef double FTYPE;

int main(int argc, char const *argv[])
{
    FTYPE x = 77617.0;
    FTYPE y = 33096.0;
    FTYPE res = ((((333.75*y*y*y*y*y*y)+(x*x)*(((11*(x*x)*(y*y)-(y*y*y*y*y*y))-121*(y*y*y*y))-2))+5.5*(y*y*y*y*y*y*y*y))+x/(2*y));
    std::cout << std::scientific << res << std::endl;
    return 0;
}
