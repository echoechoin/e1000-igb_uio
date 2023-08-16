#include <stdio.h>
#include "e1000.h"

int main()
{
    struct eth_device *dev = eth_device_get("0000:02:04.0");
    if (!dev) {
        printf("eth_device_get failed\n");
        return -1;
    }
    e1000_init(dev);
    return 0;
}