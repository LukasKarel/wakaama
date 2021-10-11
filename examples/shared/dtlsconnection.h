#ifndef MBEDTLSCONNECTION_H_
#define MBEDTLSCONNECTION_H_

#include "connection.h"
#include "liblwm2m.h"

connection_t *dtlsconnection_create(lwm2m_connection_layer_t *connLayerP, uint16_t securityInstance, int sock,
                                    char *host, char *port, int addressFamily);

#endif