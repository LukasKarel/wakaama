#include "oscore/oscore.h"
#include "er-coap-13/er-coap-13.h"
#include <string.h>


static void ntworder(uint8_t * buffer, void * insert, size_t const size) {
#ifdef LWM2M_BIG_ENDIAN
    mempcy(buffer, insert, size);
#else
    for (size_t i = 0; i < size; i++)
    {
        buffer[i] = ((uint8_t *)insert)[size - 1 - i];
    }
#endif
}

int coap_set_header_oscore(void * packet, uint8_t const * partialIV, uint8_t partialIVLen, uint8_t const * kidContext, uint8_t kidContextLen, uint8_t const * kid, uint8_t kidLen) {
    coap_packet_t *const coap_pkt = (coap_packet_t *) packet;
    size_t maxLength = 1 + partialIVLen;
    if(partialIVLen > OSCORE_PARTIALIV_MAXLEN){
        return 0;
    }
    if(kidContextLen != 0){
        maxLength += 1 + kidContextLen;
    }
    maxLength += kidLen;
    if(maxLength > OSCORE_OPTION_VALUE_MAXLEN) {
        return 0;
    }
    coap_pkt->oscore_partialIV = partialIV;
    coap_pkt->oscore_partialIVLen = partialIVLen;
    coap_pkt->oscore_kidContext = kidContext;
    coap_pkt->oscore_kidContextLen = kidContextLen;
    coap_pkt->oscore_kid = kid;
    coap_pkt->oscore_kidLen = kidLen;
    SET_OPTION(coap_pkt, COAP_OPTION_OSCORE);

    return 1;
}

int coap_get_header_oscore(void * packet, uint8_t const ** partialIV, uint8_t * partialIVLen, uint8_t const ** kidContext, uint8_t * kidContextLen, uint8_t const ** kid, uint8_t *kidLen) {
    coap_packet_t *const coap_pkt = (coap_packet_t *) packet;

    if(partialIV != NULL && partialIVLen != NULL){
        *partialIV = coap_pkt->oscore_partialIV;
        *partialIVLen = coap_pkt->oscore_partialIVLen;
    }
    if(kidContext != NULL && kidContextLen != NULL){
        *kidContext = coap_pkt->oscore_kidContext;
        *kidContextLen = coap_pkt->oscore_kidContextLen;
    }
    if(kid != NULL && kidLen != NULL){
        *kid = coap_pkt->oscore_kid;
        *kidLen = coap_pkt->oscore_kidLen;
    }
    
    return 1;
}

int coap_parse_oscore_option(void * packet, uint8_t const * value, uint32_t const optionLength) {
    coap_packet_t *const coap_pkt = (coap_packet_t *) packet;
    int kidLen = optionLength - 1;
    // first header byte must be added as well
    uint32_t maxLength = 1;
    int idx = 0;

    if(optionLength > OSCORE_OPTION_VALUE_MAXLEN) {
        return BAD_OPTION_4_02;
    }
    coap_pkt->oscore_partialIVLen = (value[0] & 0x7);
    kidLen -= coap_pkt->oscore_partialIVLen;
    maxLength += coap_pkt->oscore_partialIVLen;
    if((value[0] & 0x08)) { // kid context available
        coap_pkt->oscore_kidContextLen = value[1+coap_pkt->oscore_partialIVLen];
        kidLen -= (coap_pkt->oscore_kidContextLen + 1);
        maxLength += (coap_pkt->oscore_kidContextLen + 1);
    }
    if((value[0] & 0x10)) { // kid available
        if(kidLen < 0) {
            return BAD_OPTION_4_02;
        }
        coap_pkt->oscore_kidLen = kidLen;
        maxLength += coap_pkt->oscore_kidLen;
    }

    if(maxLength > optionLength) { //wrongly encoded optionvalue
        return BAD_OPTION_4_02;
    }

    idx = 1;
    if(coap_pkt->oscore_partialIVLen > 0){
        coap_pkt->oscore_partialIV = value + idx;
        idx += coap_pkt->oscore_partialIVLen;
    }
    if(coap_pkt->oscore_kidContextLen > 0) {
        coap_pkt->oscore_kidContext = value + idx + 1;
        idx += coap_pkt->oscore_kidContextLen;
    }
    if((value[0] & 0x08)) { // s could be 0
        idx++;
    }
    if(coap_pkt->oscore_kidLen > 0) {
        coap_pkt->oscore_kid = value + idx;
    }

    return 0;
}


int oscore_additional_authenticated_data_get_size(cn_cbor const * algo, uint8_t const * kid, uint8_t const kidLen, uint8_t const * partialIV, uint8_t const partialIVLen) {
    return oscore_additional_authenticated_data_serialize(NULL, 0, algo, kid, kidLen, partialIV, partialIVLen);
}

int oscore_additional_authenticated_data_serialize(uint8_t * buffer, size_t const length, cn_cbor const * algo, uint8_t const * kid, uint8_t const kidLen, uint8_t const * partialIV, uint8_t const partialIVLen) {
    static uint8_t const prefix[] = {
        0x83, 0x68, 0x45, 0x6e,
        0x63, 0x72, 0x79, 0x70,
        0x74, 0x30, 0x40
    };
    cn_cbor * algorithms = NULL;
    cn_cbor algorithmsArray;
    cn_cbor oscoreVersion;
    cn_cbor kidcbor;
    cn_cbor partialIVcbor;
    cn_cbor options;
    cn_cbor ad;
    memset(&oscoreVersion, 0, sizeof(cn_cbor));
    memset(&algorithmsArray, 0, sizeof(cn_cbor));
    memset(&kidcbor, 0, sizeof(cn_cbor));
    memset(&partialIVcbor, 0, sizeof(cn_cbor));
    memset(&options, 0, sizeof(cn_cbor));
    memset(&ad, 0, sizeof(cn_cbor));
    
    oscoreVersion.type = CN_CBOR_UINT;
    oscoreVersion.v.uint = 1;

    if(algo->type == CN_CBOR_ARRAY) {
        algorithms = (cn_cbor*)algo;
    }
    else {
        algorithmsArray.type = CN_CBOR_ARRAY;
	    algorithmsArray.flags |= CN_CBOR_FL_COUNT;
        cn_cbor_array_append(&algorithmsArray, (cn_cbor*)algo, NULL);
        algorithms = &algorithmsArray;
    }
    kidcbor.type = CN_CBOR_BYTES;
    kidcbor.v.bytes = kid;
    kidcbor.length = kidLen;

    partialIVcbor.type = CN_CBOR_BYTES;
    partialIVcbor.v.bytes = partialIV;
    partialIVcbor.length = partialIVLen;

    options.type = CN_CBOR_BYTES;

    ad.type = CN_CBOR_ARRAY;
	ad.flags |= CN_CBOR_FL_COUNT;

    cn_cbor_array_append(&ad, &oscoreVersion, NULL);
    cn_cbor_array_append(&ad, algorithms, NULL);
    cn_cbor_array_append(&ad, &kidcbor, NULL);
    cn_cbor_array_append(&ad, &partialIVcbor, NULL);
    cn_cbor_array_append(&ad, &options, NULL);
    
    int aadLen = cn_cbor_encoder_write(buffer, 0, length, &ad);
    if(aadLen < 0){
        return -1;
    }
    size_t extraLength = 1; // length needed for cbor byte string encoding
    if(aadLen >= 24) {
        if(aadLen <= UINT8_MAX) { // cbor bytestring length encoding in following byte
            extraLength += 1;
        }
        else if(aadLen <= UINT16_MAX) { // cbor bytestring length encoding in following two bytes
            extraLength += 2;
        }
        else {
            // this could lead to out of memory. Do not support larger arrays.
            return -1;
        }
    }
    if(buffer != NULL){
        if(length < aadLen + extraLength + sizeof(prefix)){
            return -1;
        }
        memmove(buffer + sizeof(prefix) + extraLength, buffer, aadLen);
        buffer[sizeof(prefix)] = 0x40;
        uint16_t const len2Byte = aadLen;
        uint8_t const len1Byte = aadLen;
        if(aadLen < 24) {
            buffer[sizeof(prefix)] |= aadLen;
        }
        else if(aadLen <= UINT8_MAX) {
            buffer[sizeof(prefix)+1] = len1Byte;
        }
        else if(aadLen <= UINT16_MAX) {
            ntworder(buffer+sizeof(prefix)+1, (void*)&len2Byte, 2);
        }
        memcpy(buffer, prefix, sizeof(prefix));
    }


    // one byte for cbor bytestring major tag + 11 bytes for ENC Structure
    return aadLen + extraLength + sizeof(prefix);
}