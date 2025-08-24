#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

int main(void) 
{
    SHealth shealth;
    shealth.CalculateBmi("shealth.dat");

    return 0;
}
