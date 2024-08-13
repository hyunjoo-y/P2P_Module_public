#include "NATTraversalHelper.h"

#include <iostream>

int main(){
    NATTraversalHelper helper;

    helper.installCoturnServer();
    helper.setConfig(3478, 49152, 65535);
    helper.startCoturnServer();
    //helper.stopCoturnServer();
    return 0;
}
