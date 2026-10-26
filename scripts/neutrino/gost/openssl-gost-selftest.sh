#!/bin/sh
# Run on the target. Neutrino's /tmp points to /dev/shmem, which supports
# regular files but does not implement mkdir. Use a flat set of private files.
set -eu
OPENSSL_BIN=${OPENSSL_BIN:-/usr/bin/openssl}
OPENSSL_CONF=${OPENSSL_CONF:-/etc/openssl/openssl.cnf}
export OPENSSL_CONF
umask 077
work=${TMPDIR:-/tmp}/openssl-gost-selftest.$$
created_files=

cleanup() {
    for name in $created_files; do
        rm -f "$work.$name"
    done
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

# Reserve each name exclusively. If one already exists, remove only files
# created by this invocation. umask 077 also protects the temporary keys.
for name in message plain expected cipher decrypted key.pem public.pem signature changed negative.log; do
    if (set -C; : > "$work.$name") 2>/dev/null; then
        created_files="$created_files $name"
    else
        fail "cannot create temporary file $work.$name (check TMPDIR and existing files)"
    fi
done

"$OPENSSL_BIN" version
engine=$("$OPENSSL_BIN" engine -t -c gost)
printf '%s\n' "$engine"
case "$engine" in
    *'[ available ]'*) ;;
    *) fail 'GOST ENGINE is unavailable' ;;
esac

# GOST R 34.11-2012 M1, also used by upstream test_digest.c.
printf '%s' '012345678901234567890123456789012345678901234567890123456789012' > "$work.message"
digest=$("$OPENSSL_BIN" dgst -md_gost12_256 "$work.message")
[ "${digest##* }" = 9d151eefd8590b89daa6ba6cb74af9275dd051026bb149a452fd84e5e57b5500 ] || fail 'Streebog-256 vector'
echo 'PASS: Streebog-256 known-answer test (automatic engine loading)'
digest=$("$OPENSSL_BIN" dgst -md_gost12_512 "$work.message")
[ "${digest##* }" = 1b54d01a4af5b9d5cc3d86d68d285462b19abc2475222f35c085122be4ba1ffa00ad30f8767b3a82384c6574f024c311e2a481332b08ef7f41797891c1646f48 ] || fail 'Streebog-512 vector'
echo 'PASS: Streebog-512 known-answer test'

# GOST R 34.12-2015 Kuznyechik single-block vector. This fixed key and ECB
# mode are solely test data; they are not an example for encrypting files.
key=8899aabbccddeeff0011223344556677fedcba98765432100123456789abcdef
printf '\021\042\063\104\125\146\167\000\377\356\335\314\273\252\231\210' > "$work.plain"
printf '\177\147\235\220\276\274\044\060\132\106\215\102\271\324\355\315' > "$work.expected"
"$OPENSSL_BIN" enc -grasshopper-ecb -e -K "$key" -nosalt -nopad -in "$work.plain" -out "$work.cipher"
cmp -s "$work.expected" "$work.cipher" || fail 'Kuznyechik encryption vector'
"$OPENSSL_BIN" enc -grasshopper-ecb -d -K "$key" -nosalt -nopad -in "$work.cipher" -out "$work.decrypted"
cmp -s "$work.plain" "$work.decrypted" || fail 'Kuznyechik decryption'
echo 'PASS: Kuznyechik encryption vector and decryption'

# GOST R 34.12-2015 Magma vector (RFC 8891 A.4/A.5). A zero CBC IV
# makes the first block identical to the cipher's single-block test.
key=ffeeddccbbaa99887766554433221100f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
printf '\376\334\272\230\166\124\062\020' > "$work.plain"
printf '\116\351\001\345\302\330\312\075' > "$work.expected"
"$OPENSSL_BIN" enc -magma-cbc -e -K "$key" -iv 0000000000000000 -nosalt -nopad -in "$work.plain" -out "$work.cipher"
cmp -s "$work.expected" "$work.cipher" || fail 'Magma encryption vector'
"$OPENSSL_BIN" enc -magma-cbc -d -K "$key" -iv 0000000000000000 -nosalt -nopad -in "$work.cipher" -out "$work.decrypted"
cmp -s "$work.plain" "$work.decrypted" || fail 'Magma decryption'
echo 'PASS: Magma encryption vector and decryption'

for bits in 256 512; do
    "$OPENSSL_BIN" genpkey -algorithm "gost2012_$bits" -pkeyopt paramset:A -out "$work.key.pem"
    "$OPENSSL_BIN" pkey -in "$work.key.pem" -pubout -out "$work.public.pem"
    "$OPENSSL_BIN" dgst "-md_gost12_$bits" -sign "$work.key.pem" -out "$work.signature" "$work.message"
    "$OPENSSL_BIN" dgst "-md_gost12_$bits" -verify "$work.public.pem" -signature "$work.signature" "$work.message"
    printf '%s' 'changed message' > "$work.changed"
    if "$OPENSSL_BIN" dgst "-md_gost12_$bits" -verify "$work.public.pem" -signature "$work.signature" "$work.changed" > "$work.negative.log" 2>&1; then
        fail "GOST 2012-$bits accepted a changed message"
    fi
    echo "PASS: GOST 2012-$bits signing, verification and rejection of changed data"
done
echo 'PASS: OpenSSL GOST self-test'
