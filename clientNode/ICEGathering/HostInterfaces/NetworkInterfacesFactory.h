#ifndef NETWORK_INTERFACES_FACTORY_H
#define NETWORK_INTERFACES_FACTORY_H

#include <memory>
#include "NetworkInterfaces.h"

std::unique_ptr<NetworkInterfaces> createNetworkInterfaces();

#endif // NETWORK_INTERFACES_FACTORY_H

