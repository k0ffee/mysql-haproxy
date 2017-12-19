#include <stdio.h>
#include <stdlib.h>

int main() {
    double loadavg;
    static const double maxload = 150;

    /*
     * 1-minute sample:
     *     double loadavg;
     *     if (getloadavg(&loadavg, 1) == 1)
     *     if (loadavg < maxload) ...
     * 5-minute sample (using second element returned by getloadavg):
     *     double loadavg[2];
     *     if (getloadavg(loadavg, 2) == 2)
     *     if (loadavg[1] < maxload) ...
     */

    if (getloadavg(&loadavg, 1) == 1) {

        if (loadavg < maxload) {
            printf("%f < %f\n", loadavg, maxload);
        } else if (loadavg >= maxload) {
            printf("%f >= %f\n", loadavg, maxload);
        }
    }

    return 0;
}
