/* Boot order: network (IFS), successful NTP, physical UID, trusted CMS. */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#ifndef BVSTK_UID_VERIFY_TIMEOUT_SECONDS
#define BVSTK_UID_VERIFY_TIMEOUT_SECONDS 90
#endif
#ifdef BVSTK_PLATFORM_NEUTRINO
#include "apps/neutrino/i2c/bvstk_i2c_ipc.h"
#endif

enum { VERIFIED, TIME_SYNC, CLOCK_INVALID, EEPROM_READ, MATERIALS, INVALID,
       CRL_EXPIRED, CERT_EXPIRED, SIGNER_PURPOSE, TIMEOUT, INTERNAL };
static const char *const reasons[] = {"ok", "time_sync_failed", "clock_invalid",
    "eeprom_read_failed", "security_files_invalid", "signature_or_certificate_invalid",
    "crl_expired", "certificate_time_invalid", "signer_purpose_invalid",
    "verification_timeout", "internal_error"};
static int chain_error;
static int verify_callback(int ok, X509_STORE_CTX *ctx)
{
    if (!ok) chain_error = X509_STORE_CTX_get_error(ctx);
    return ok;
}
static int suffix(const char *name, const char *ext)
{
    size_t n = strlen(name), e = strlen(ext);
    return n > e && !strcmp(name + n - e, ext);
}
static int load_directory(X509_STORE *store, const char *root, int crls)
{
    char directory[1024], path[1280];
    if (snprintf(directory, sizeof(directory), "%s/trust/%s", root,
                 crls ? "crl" : "anchors") >= (int)sizeof(directory)) return 0;
    DIR *dir = opendir(directory);
    if (!dir) return 0;
    struct dirent *entry;
    int count = 0, ok = 1;
    while ((entry = readdir(dir))) {
        if (!suffix(entry->d_name, crls ? ".crl" : ".pem")) continue;
        if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) {
            ok = 0; break;
        }
        BIO *bio = BIO_new_file(path, "rb");
        if (!bio) { ok = 0; break; }
        if (crls) {
            X509_CRL *crl = d2i_X509_CRL_bio(bio, NULL);
            ok = crl && X509_STORE_add_crl(store, crl);
            X509_CRL_free(crl);
        } else {
            X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
            ok = cert && X509_STORE_add_cert(store, cert);
            X509_free(cert);
        }
        BIO_free(bio);
        if (!ok) break;
        ++count;
    }
    closedir(dir);
    return ok && count > 0;
}
static int verify_uid(const char *root, const uint8_t uid[16])
{
    int result = MATERIALS;
    char path[1024];
    BIO *signature = NULL, *content = NULL, *output = NULL;
    CMS_ContentInfo *cms = NULL;
    X509_STORE *store = NULL;
    STACK_OF(X509) *signers = NULL;
    if (!OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL)) return INTERNAL;
    if (snprintf(path, sizeof(path), "%s/signatures/uid/banana.uuid.sig", root) >= (int)sizeof(path)) goto done;
    signature = BIO_new_file(path, "rb");
    if (!signature || !(cms = d2i_CMS_bio(signature, NULL)) || !CMS_is_detached(cms)) goto done;
    store = X509_STORE_new();
    if (!store || !load_directory(store, root, 0) || !load_directory(store, root, 1)) goto done;
    X509_STORE_set_verify_cb(store, verify_callback);
    if (!X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) ||
        !X509_VERIFY_PARAM_set_purpose(X509_STORE_get0_param(store), X509_PURPOSE_ANY)) goto done;
    content = BIO_new_mem_buf(uid, 16);
    output = BIO_new(BIO_s_null());
    if (!content || !output) { result = INTERNAL; goto done; }
    chain_error = 0;
    if (!CMS_verify(cms, NULL, store, content, output, CMS_BINARY)) {
        result = INVALID;
        if (chain_error == X509_V_ERR_CRL_HAS_EXPIRED) result = CRL_EXPIRED;
        if (chain_error == X509_V_ERR_CERT_HAS_EXPIRED || chain_error == X509_V_ERR_CERT_NOT_YET_VALID)
            result = CERT_EXPIRED;
        goto done;
    }
    signers = CMS_get0_signers(cms);
    result = SIGNER_PURPOSE;
    if (!signers || sk_X509_num(signers) != 1) goto done;
    /* Require an explicit Code Signing EKU, not unrestricted usage. */
    EXTENDED_KEY_USAGE *eku = X509_get_ext_d2i(sk_X509_value(signers, 0), NID_ext_key_usage, NULL, NULL);
    if (eku) {
        for (int i = 0; i < sk_ASN1_OBJECT_num(eku); ++i)
            if (OBJ_obj2nid(sk_ASN1_OBJECT_value(eku, i)) == NID_code_sign) result = VERIFIED;
        EXTENDED_KEY_USAGE_free(eku);
    }
done:
    if (result != VERIFIED) ERR_print_errors_fp(stderr);
    sk_X509_free(signers);
    X509_STORE_free(store);
    CMS_ContentInfo_free(cms);
    BIO_free(signature); BIO_free(content); BIO_free(output);
    return result;
}
static int read_uid(uint8_t uid[16])
{
#ifdef BVSTK_PLATFORM_NEUTRINO
    bvstk_i2c_uid_message_t message = {0};
    message.version = BVSTK_I2C_IPC_VERSION;
    strcpy(message.profile, "p24c02a");
    int fd = open(BVSTK_I2C_DEVICE_PATH, O_RDWR);
    if (fd < 0) return 0;
    int rc = devctl(fd, BVSTK_DCMD_I2C_READ_UID, &message, sizeof(message), NULL);
    close(fd);
    if (rc || message.version != BVSTK_I2C_IPC_VERSION || message.status || message.completed != 16) return 0;
    memcpy(uid, message.uid, 16);
    return 1;
#else
    (void)uid;
    return 0;
#endif
}
static int synchronize(void)
{
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (!pid) {
        execl("/proc/boot/ksh", "ksh", "/etc/ntp-sync-start", (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
static int check(int boot, const char *root, const char *uid_file)
{
    if (boot && !synchronize()) return TIME_SYNC;
    if (time(NULL) < 1704067200) return CLOCK_INVALID;
    uint8_t uid[16];
    if (uid_file) {
        FILE *file = fopen(uid_file, "rb");
        if (!file) return EEPROM_READ;
        int ok = fread(uid, 1, 16, file) == 16 && fgetc(file) == EOF && !ferror(file);
        fclose(file);
        if (!ok) return EEPROM_READ;
    } else if (!read_uid(uid)) return EEPROM_READ;
    return verify_uid(root, uid);
}
static double monotonic_seconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
    return now.tv_sec + now.tv_nsec / 1e9;
}
int main(int argc, char **argv)
{
    int boot = argc == 2 && !strcmp(argv[1], "--boot");
    int files = argc == 4 && !strcmp(argv[1], "--files");
    if (argc != 1 && !boot && !files) {
        fprintf(stderr, "Usage: bvstk-uid-verify [--boot | --files security-directory uid-file]\n");
        return 2;
    }
    fflush(NULL);
    pid_t pid = fork();
    int result = INTERNAL;
    if (!pid) {
        if (setsid() < 0) _exit(INTERNAL);
        _exit(check(boot, files ? argv[2] : "/flash/security", files ? argv[3] : NULL));
    }
    if (pid > 0) {
        double started = monotonic_seconds();
        for (;;) {
            int status;
            pid_t finished = waitpid(pid, &status, WNOHANG);
            if (finished == pid) {
                if (WIFEXITED(status)) result = WEXITSTATUS(status);
                break;
            }
            if (finished < 0 && errno != EINTR) break;
            double now = monotonic_seconds();
            if (started < 0 || now < 0 || now - started >= BVSTK_UID_VERIFY_TIMEOUT_SECONDS) {
                kill(-pid, SIGKILL); kill(pid, SIGKILL);
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
                result = TIMEOUT; break;
            }
            struct timespec delay = {0, 100000000};
            nanosleep(&delay, NULL);
        }
    }
    if (result < VERIFIED || result > INTERNAL) result = INTERNAL;
    printf("verify: %s\n", result == VERIFIED ? "true" : "false");
    if (result) printf("verify_reason: %s\n", reasons[result]);
    fflush(stdout);
    /* Diagnostic snapshot for SSH; UART receives the same final result. */
    if (boot) {
        /* /dev/shmem supports flat files but not filesystem rename. Remove
         * the previous snapshot, then create exclusively (never follow links). */
        const char *path = "/tmp/bvstk-uid-verify.status";
        (void)unlink(path);
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            char text[128];
            int n = snprintf(text, sizeof(text), "verify: %s\nverify_reason: %s\n",
                result == VERIFIED ? "true" : "false", reasons[result]);
            int written = write(fd, text, (size_t)n) == n;
            close(fd);
            if (!written) (void)unlink(path);
        }
    }
    return result == VERIFIED ? 0 : 1;
}
