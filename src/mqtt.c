#include <stdlib.h>
#include <string.h>
#include "mqtt.h"
#include "pack.h"

static size_t unpack_mqtt_connect(
    const unsigned char *,
    union mqtt_header *,
    union mqtt_packet *);

static size_t unpack_mqtt_publish(
    const unsigned char *,
    union mqtt_header *,
    union mqtt_packet *);

static size_t unpack_mqtt_subscribe(
    const unsigned char *,
    union mqtt_header *,
    union mqtt_packet *);

static size_t unpack_mqtt_unsubscribe(
    const unsigned char *,
    union mqtt_header *,
    union mqtt_packet *);

static size_t unpack_mqtt_ack(
    const unsigned char *,
    union mqtt_header *,
    union mqtt_packet *);

static unsigned char *pack_mqtt_header(const union mqtt_header *);
static unsigned char *pack_mqtt_ack(const union mqtt_header *);
static unsigned char *pack_mqtt_connack(const union mqtt_header *);
static unsigned char *pack_mqtt_suback(const union mqtt_header *);
static unsigned char *pack_mqtt_publish(const union mqtt_header *);

/*
 * MQTT v3.1.1 standard, Remaining length field on the fixed header can be at
 * most 4 bytes.
 */
static const int MAX_LEN_BYTES = 4;

/*
 * Encode Remaining Length on a MQTT packet header, comprised of Variable
 * Header and Payload if present. It does not take into account the bytes
 * required to store itself. Refer to MQTT v3.1.1 algorithm for the
 * implementation.
 */
int mqtt_encode_length(unsigned char *buf, size_t len)
{
    int bytes = 0;
    do
    {
        if (bytes + 1 > MAX_LEN_BYTES)
            return bytes;
        short d = len % 128;
        len /= 128;
        /*If there are more digits to encode, set the top bit of this digit*/
        if (len > 0)
            d |= 128;
        buf[bytes++] = d;
    } while (len > 0);
    return bytes;
}

/*
 * Decode Remaining Length comprised of Variable Header and Payload if
 * present. It does not take into account the bytes for storing length. Refer
 * to MQTT v3.1.1 algorithm for the implementation suggestion.
 *
 * TODO Handle case where multiplier > 128 * 128 * 128
 */
unsigned long long mqtt_decode_length(const unsigned char **buf)
{
    char c;
    int multipler = 1;
    unsigned long long value = 0LL;
    do
    {
        c = **buf;
        value += (c & 127) * multipler;
        multipler *= 128;
        (*buf)++;
    } while ((c & 128) != 0);

    return value;
}

/**
 * @brief MQTT unpacking functions
 */

static size_t unpack_mqtt_connect(const unsigned char *buf,
                                  union mqtt_header *hdr,
                                  union mqtt_packet *pkt)
{
    struct mqtt_connect connect =
        {
            .header = *hdr}; // How?
    pkt->connect = connect;
    const unsigned char *init = buf;
    /*
     * Second byte of the fixed header, contains the length of remaining bytes
     * of the connect packet
     */
    size_t len = mqtt_decode_length(&buf);
    /*
     * For now we ignore checks on protocol name and reserved bits, just skip
     * to the 8th byte
     */
    buf = init + 8;
    /* Read variable header byte flags */
    pkt->connect.byte = unpack_u8((const uint8_t **)&buf);
    /* Read keepalive MSB and LSB (2 bytes word) */
    pkt->connect.payload.keepalive = unpack_u16((const uint8_t **)&buf);
    /* Read CID length (2 bytes word) */
    uint16_t cid_len = unpack_u16((const uint8_t **)&buf);
    /* Read the client id */
    if (cid_len > 0)
    {
        pkt->connect.payload.client_id = malloc(cid_len + 1);
        unpack_bytes((const uint8_t **)&buf, cid_len,
                     pkt->connect.payload.client_id);
    }

    /* Read the will topic and message if will is set on flags */
    if (pkt->connect.bits.will = 1)
    {
        unpack_string16(&buf, &pkt->connect.payload.will_topic);
        unpack_string16(&buf, &pkt->connect.payload.will_message);
    }

    /* Read the username if username flag is set */
    if (pkt->connect.bits.username == 1)
        unpack_string16(&buf, &pkt->connect.payload.username);

    /* Read the password if password flag is set */
    if(pkt->connect.bits.password == 1)
        unpack_string16(&buf, &pkt->connect.payload.password);

    return len;
}

// COTINUE FROM static size_t unpack_mqtt_publish implementation