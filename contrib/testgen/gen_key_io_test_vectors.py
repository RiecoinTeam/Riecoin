#!/usr/bin/env python3
# Copyright (c) 2012-2022 The Bitcoin Core developers
# Copyright (c) 2013-2023 The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Generate valid and invalid base58/bech32(m) address and private key test vectors.
'''

from itertools import islice
import os
import random
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../../test/functional'))

from test_framework.address import base58_to_byte, byte_to_base58, b58chars  # noqa: E402
from test_framework.script import OP_0, OP_1, OP_2, OP_3, OP_16, OP_DUP, OP_EQUAL, OP_EQUALVERIFY, OP_HASH160, OP_CHECKSIG  # noqa: E402
from test_framework.segwit_addr import bech32_encode, decode_segwit_address, convertbits, CHARSET, Encoding  # noqa: E402

# key types
PUBKEY_ADDRESS = 60
SCRIPT_ADDRESS = 65
PUBKEY_ADDRESS_TEST = 122
SCRIPT_ADDRESS_TEST = 127
PUBKEY_ADDRESS_REGTEST = 122
SCRIPT_ADDRESS_REGTEST = 127

# script
pubkey_prefix = (OP_DUP, OP_HASH160, 20)
pubkey_suffix = (OP_EQUALVERIFY, OP_CHECKSIG)
script_prefix = (OP_HASH160, 20)
script_suffix = (OP_EQUAL,)
p2wpkh_prefix = (OP_0, 20)
p2wsh_prefix = (OP_0, 32)
p2tr_prefix = (OP_1, 32)

metadata_keys = ['isPrivkey', 'chain', 'isCompressed', 'tryCaseFlip']
# templates for valid sequences
templates = [
  # prefix, payload_size, suffix, metadata, output_prefix, output_suffix
  #                                  None = N/A
  ((PUBKEY_ADDRESS,),         20, (),   (False, 'main',    None,  None), pubkey_prefix, pubkey_suffix),
  ((SCRIPT_ADDRESS,),         20, (),   (False, 'main',    None,  None), script_prefix, script_suffix),
  ((PUBKEY_ADDRESS_TEST,),    20, (),   (False, 'test',    None,  None), pubkey_prefix, pubkey_suffix),
  ((SCRIPT_ADDRESS_TEST,),    20, (),   (False, 'test',    None,  None), script_prefix, script_suffix),
  ((PUBKEY_ADDRESS_REGTEST,), 20, (),   (False, 'regtest', None,  None), pubkey_prefix, pubkey_suffix),
  ((SCRIPT_ADDRESS_REGTEST,), 20, (),   (False, 'regtest', None,  None), script_prefix, script_suffix),
  ('prv',                     32, (1,), (True,  'main',    True,  None), (),            ()),
  ('prv',                     32, (1,), (True,  'test',    True,  None), (),            ()),
  ('prv',                     32, (1,), (True,  'regtest', True,  None), (),            ())
]
# templates for valid bech32 sequences
bech32_templates = [
  # hrp, version, witprog_size, metadata, encoding, output_prefix
  ('ric',   0, 20, (False, 'main',    None, True), Encoding.BECH32,  p2wpkh_prefix),
  ('ric',   0, 32, (False, 'main',    None, True), Encoding.BECH32,  p2wsh_prefix),
  ('ric',   1, 32, (False, 'main',    None, True), Encoding.BECH32M, p2tr_prefix),
  ('ric',   2,  2, (False, 'main',    None, True), Encoding.BECH32M, (OP_2, 2)),
  ('tric',  0, 20, (False, 'test',    None, True), Encoding.BECH32,  p2wpkh_prefix),
  ('tric',  0, 32, (False, 'test',    None, True), Encoding.BECH32,  p2wsh_prefix),
  ('tric',  1, 32, (False, 'test',    None, True), Encoding.BECH32M, p2tr_prefix),
  ('tric',  3, 16, (False, 'test',    None, True), Encoding.BECH32M, (OP_3, 16)),
  ('rric',  0, 20, (False, 'regtest', None, True), Encoding.BECH32,  p2wpkh_prefix),
  ('rric',  0, 32, (False, 'regtest', None, True), Encoding.BECH32,  p2wsh_prefix),
  ('rric',  1, 32, (False, 'regtest', None, True), Encoding.BECH32M, p2tr_prefix),
  ('rric', 16, 40, (False, 'regtest', None, True), Encoding.BECH32M, (OP_16, 40))
]
# templates for invalid bech32 sequences
bech32_ng_templates = [
  # hrp, version, witprog_size, encoding, invalid_bech32, invalid_checksum, invalid_char
  ('tc',    0, 20, Encoding.BECH32,  False, False, False),
  ('bt',    1, 32, Encoding.BECH32M, False, False, False),
  ('tric', 17, 32, Encoding.BECH32M, False, False, False),
  ('rric',  3,  1, Encoding.BECH32M, False, False, False),
  ('ric',  15, 41, Encoding.BECH32M, False, False, False),
  ('tric',  0, 16, Encoding.BECH32,  False, False, False),
  ('rric',  0, 32, Encoding.BECH32,  True,  False, False),
  ('ric',   0, 16, Encoding.BECH32,  True,  False, False),
  ('tric',  0, 32, Encoding.BECH32,  False, True,  False),
  ('rric',  0, 20, Encoding.BECH32,  False, False, True),
  ('ric',   0, 20, Encoding.BECH32M, False, False, False),
  ('tric',  0, 32, Encoding.BECH32M, False, False, False),
  ('rric',  0, 20, Encoding.BECH32M, False, False, False),
  ('ric',   1, 32, Encoding.BECH32,  False, False, False),
  ('tric',  2, 16, Encoding.BECH32,  False, False, False),
  ('rric', 16, 20, Encoding.BECH32,  False, False, False),
]

def is_valid(v):
    '''Check vector v for validity'''
    if len(v) == 67:
        if v[:3] == 'prv':
            return True
        else:
            return False
    if len(set(v) - set(b58chars)) > 0:
        return is_valid_bech32(v)
    try:
        payload, version = base58_to_byte(v)
        result = bytes([version]) + payload
    except ValueError:  # thrown if checksum doesn't match
        return is_valid_bech32(v)
    for template in templates:
        if template[0] == 'prv':
            return False;
        prefix = bytearray(template[0])
        suffix = bytearray(template[2])
        if result.startswith(prefix) and result.endswith(suffix):
            if (len(result) - len(prefix) - len(suffix)) == template[1]:
                return True
    return is_valid_bech32(v)

def is_valid_bech32(v):
    '''Check vector v for bech32 validity'''
    for hrp in ['ric', 'tric', 'rric']:
        if decode_segwit_address(hrp, v) != (None, None):
            return True
    return False

def gen_valid_base58_vector(template):
    '''Generate valid base58 vector'''
    prefix = template[0]
    payload = rand_bytes(size=template[1])
    suffix = bytearray(template[2])
    dst_prefix = bytearray(template[4])
    dst_suffix = bytearray(template[5])
    if isinstance(prefix[0], int):
        prefix = bytearray(prefix)
        rv = byte_to_base58(payload + suffix, prefix[0])
    else:
        rv = prefix + payload.hex()
    return rv, dst_prefix + payload + dst_suffix

def gen_valid_bech32_vector(template):
    '''Generate valid bech32 vector'''
    hrp = template[0]
    witver = template[1]
    witprog = rand_bytes(size=template[2])
    encoding = template[4]
    dst_prefix = bytearray(template[5])
    rv = bech32_encode(encoding, hrp, [witver] + convertbits(witprog, 8, 5))
    return rv, dst_prefix + witprog

def gen_valid_vectors():
    '''Generate valid test vectors'''
    glist = [gen_valid_base58_vector, gen_valid_bech32_vector]
    tlist = [templates, bech32_templates]
    while True:
        for template, valid_vector_generator in [(t, g) for g, l in zip(glist, tlist) for t in l]:
            rv, payload = valid_vector_generator(template)
            assert is_valid(rv)
            metadata = {x: y for x, y in zip(metadata_keys,template[3]) if y is not None}
            hexrepr = payload.hex()
            yield (rv, hexrepr, metadata)

def gen_invalid_base58_vector(template):
    '''Generate possibly invalid vector'''
    # kinds of invalid vectors:
    #   invalid prefix
    #   invalid payload length
    #   invalid (randomized) suffix (add random data)
    #   corrupt checksum
    corrupt_prefix = randbool(0.2)
    randomize_payload_size = randbool(0.2)
    corrupt_suffix = randbool(0.2)

    if corrupt_prefix:
        prefix = rand_bytes(size=len(template[0]))
    else:
        prefix = template[0]

    if randomize_payload_size:
        payload = rand_bytes(size=max(int(random.expovariate(0.5)), 50))
    else:
        payload = rand_bytes(size=template[1])

    if corrupt_suffix:
        suffix = rand_bytes(size=len(template[2]))
    else:
        suffix = bytearray(template[2])

    if isinstance(prefix[0], int):
        val = byte_to_base58(payload + suffix, prefix[0])
    else:
        val = prefix + payload.hex()
    if random.randint(0,10)<1: # line corruption
        if randbool(): # add random character to end
            val += random.choice(b58chars)
        else: # replace random character in the middle
            n = random.randint(0, len(val))
            val = val[0:n] + random.choice(b58chars) + val[n+1:]

    return val

def gen_invalid_bech32_vector(template):
    '''Generate possibly invalid bech32 vector'''
    no_data = randbool(0.1)
    to_upper = randbool(0.1)
    hrp = template[0]
    witver = template[1]
    witprog = rand_bytes(size=template[2])
    encoding = template[3]

    if no_data:
        rv = bech32_encode(encoding, hrp, [])
    else:
        data = [witver] + convertbits(witprog, 8, 5)
        if template[4] and not no_data:
            if template[2] % 5 in {2, 4}:
                data[-1] |= 1
            else:
                data.append(0)
        rv = bech32_encode(encoding, hrp, data)

    if template[5]:
        i = len(rv) - random.randrange(1, 7)
        rv = rv[:i] + random.choice(CHARSET.replace(rv[i], '')) + rv[i + 1:]
    if template[6]:
        i = len(hrp) + 1 + random.randrange(0, len(rv) - len(hrp) - 4)
        rv = rv[:i] + rv[i:i + 4].upper() + rv[i + 4:]

    if to_upper:
        rv = rv.swapcase()

    return rv

def randbool(p = 0.5):
    '''Return True with P(p)'''
    return random.random() < p

def rand_bytes(*, size):
    return bytearray(random.getrandbits(8) for _ in range(size))

def gen_invalid_vectors():
    '''Generate invalid test vectors'''
    # start with some manual edge-cases
    yield "",
    yield "x",
    glist = [gen_invalid_base58_vector, gen_invalid_bech32_vector]
    tlist = [templates, bech32_ng_templates]
    while True:
        for template, invalid_vector_generator in [(t, g) for g, l in zip(glist, tlist) for t in l]:
            val = invalid_vector_generator(template)
            if not is_valid(val):
                yield val,

if __name__ == '__main__':
    import json
    iters = {'valid':gen_valid_vectors, 'invalid':gen_invalid_vectors}
    random.seed(42)
    try:
        uiter = iters[sys.argv[1]]
    except IndexError:
        uiter = gen_valid_vectors
    try:
        count = int(sys.argv[2])
    except IndexError:
        count = 0

    data = list(islice(uiter(), count))
    json.dump(data, sys.stdout, sort_keys=True, indent=4)
    sys.stdout.write('\n')

