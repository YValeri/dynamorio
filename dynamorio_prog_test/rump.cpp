#include <iostream>
#include <cmath>

typedef float FTYPE;

int main(int argc, char const *argv[])
{
    FTYPE x = 77617.0;
    FTYPE y = 33096.0;
    FTYPE res = 333.75*std::pow(y,6)+(x*x)*(11*(x*x)*(y*y)-std::pow(y, 6)-121*std::pow(y,4)-2)+5.5*std::pow(y,8)+x/(2*y);
    std::cout << std::scientific << res << std::endl;
    return 0;
}
