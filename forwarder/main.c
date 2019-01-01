/*
 * Copyright (C) 2018 Zhiyi Zhang, Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "ndn-lite/encode/interest.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/face/direct-face.h"
#include "ndn-lite/face/dummy-face.h"
#include <stdio.h>

static uint8_t private[] = {
  0x00, 0x79, 0xD8, 0x8A, 0x5E, 0x4A, 0xF3, 0x2D,
  0x36, 0x03, 0x89, 0xC7, 0x92, 0x3B, 0x2E, 0x50,
  0x7C, 0xF7, 0x6E, 0x60, 0xB0, 0xAF, 0x26, 0xE4,
  0x42, 0x9D, 0xC8, 0xCE, 0xF0, 0xDE, 0x75, 0xB3
};

static uint8_t public[] = {
  0xB2, 0xFC, 0x62, 0x14, 0x78, 0xDC, 0x10, 0xEA,
  0x61, 0x42, 0xB9, 0x34, 0x67, 0xE6, 0xDD, 0xE3,
  0x3D, 0x35, 0xAA, 0x5B, 0xA4, 0x24, 0x6C, 0xD4,
  0xB4, 0xED, 0xD8, 0xA4, 0x59, 0xA7, 0x32, 0x12,
  0x57, 0x37, 0x90, 0x5D, 0xED, 0x37, 0xC8, 0xE8,
  0x6A, 0x81, 0xE5, 0x8F, 0xBE, 0x6B, 0xD3, 0x27,
  0x20, 0xBB, 0x16, 0x2A, 0xD3, 0x2F, 0xB5, 0x11,
  0x1B, 0xD1, 0xAF, 0x76, 0xDB, 0xAD, 0xB8, 0xCE
}; // this is secp160r1 key*/

int
on_data_callback(const uint8_t* data, uint32_t data_size)
{
  printf("application receives a Data\n");
  ndn_data_t data_check;
  ndn_ecc_pub_t pub_key;
  ndn_ecc_pub_init(&pub_key, public, sizeof(public), NDN_ECDSA_CURVE_SECP160R1, 1234);
  int result = ndn_data_tlv_decode_ecdsa_verify(&data_check, data, data_size, &pub_key);
  if (result == 0) {
    printf("data encoding and ecdsa sig verification succeeded\n");
  }
  else
    printf("result: %d\n", result);
  return 0;
}

int
on_interest_timeout_callback(const uint8_t* interest, uint32_t interest_size)
{
  (void)interest;
  (void)interest_size;
  return 0;
}

int
on_interest(const uint8_t* interest, uint32_t interest_size)
{
  (void)interest;
  (void)interest_size;
  printf("application receives an Interest\n");
  return 0;
}

int main(void)
{
  // The forwarder unit test model
  /*
   *  +----+       +---------+     +------------+
   *  |app /ndn -- |forwarder| -- /aaa dummyface|
   *  +----+       +---------+     +------------+
   *
   *        -----I: /aaa/bbb/ccc/ddd --->
   *        <----I: /ndn/hello ----------
   *        <----D: /aaa/bbb/ccc/ddd ----
   */

  // tests start
  ndn_forwarder_t* forwarder = ndn_forwarder_init();
  ndn_direct_face_t* direct_face = ndn_direct_face_construct(124);
  ndn_dummy_face_t dummy_face;
  ndn_dummy_face_construct(&dummy_face, 125);

  // add FIB entry
  printf("\n***Add dummy face to FIB with prefix /aaa***\n");
  char prefix_string[] = "/aaa";
  ndn_name_t prefix;
  ndn_name_from_string(&prefix, prefix_string, sizeof(prefix_string));
  ndn_forwarder_fib_insert(&prefix, &dummy_face.intf, 1);

  // create interest
  ndn_interest_t interest;
  ndn_interest_init(&interest);
  char name_string[] = "/aaa/bbb/ccc/ddd";
  ndn_name_from_string(&interest.name, name_string, sizeof(name_string));
  uint8_t interest_block[256] = {0};
  ndn_encoder_t encoder;
  encoder_init(&encoder, interest_block, 256);
  ndn_interest_tlv_encode(&encoder, &interest);

  // express Interest
  printf("\n***Express Interest /aaa/bbb/ccc/ddd***\n");
  ndn_direct_face_express_interest(&interest.name,
                                   interest_block, encoder.offset,
                                   on_data_callback, on_interest_timeout_callback);

  // register a prefix
  printf("\n***Register the Interest Prefix /ndn***\n");
  char prefix_string2[] = "/ndn";
  ndn_name_t prefix2;
  ndn_name_from_string(&prefix2, prefix_string2, sizeof(prefix_string2));
  ndn_direct_face_register_prefix(&prefix2, on_interest);

  // receive an Interest
  char name_string2[] = "/ndn/hello";
  ndn_name_from_string(&interest.name, name_string2, sizeof(name_string2));
  encoder_init(&encoder, interest_block, 256);
  ndn_interest_tlv_encode(&encoder, &interest);
  printf("\n***Dummy Face receives an Interest /aaa/bbb/ccc/ddd***\n");
  ndn_face_receive(&dummy_face.intf, interest_block, encoder.offset);

  // prepare Data content and Data packet
  uint8_t buf[10] = {2,2,2,2,2,2,2,2,2,2};
  uint8_t block_value[1024];
  ndn_data_t data;
  ndn_data_set_content(&data, buf, sizeof(buf));

  // set name, metainfo
  char data_name_string[] = "/aaa/bbb/ccc/ddd";
  ndn_name_from_string(&data.name, data_name_string, sizeof(data_name_string));
  ndn_metainfo_init(&data.metainfo);
  ndn_metainfo_set_content_type(&data.metainfo, NDN_CONTENT_TYPE_BLOB);

  // sign the packet
  ndn_ecc_prv_t prv_key;
  ndn_ecc_prv_init(&prv_key, private, sizeof(private), NDN_ECDSA_CURVE_SECP160R1, 1234);
  char id_string[] = "/ndn/zhiyi";
  ndn_name_t identity;
  ndn_name_from_string(&identity, id_string, sizeof(id_string));
  encoder_init(&encoder, block_value, 1024);
  ndn_data_tlv_encode_ecdsa_sign(&encoder, &data, &identity, &prv_key);

  // receive the Data packet
  printf("\n***Dummy Face receives an Data /aaa/bbb/ccc/ddd***\n");
  ndn_face_receive(&dummy_face.intf, block_value, encoder.offset);

  (void)forwarder;
  (void)direct_face;
  return 0;
}
