#include "lean/lean.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

/* Global SSL context */
SSL_CTX *ctx;

#define DEFAULT_BUF_SIZE 64

void handle_error(const char *file, int lineno, const char *msg)
{
  fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
  ERR_print_errors_fp(stderr);
  exit(-1);
}

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)

void die(const char *msg)
{
  perror(msg);
  exit(1);
}

void print_unencrypted_data(char *buf, size_t len)
{
  printf("%.*s", (int)len, buf);
}

/* An instance of this object is created each time a client connection is
 * accepted. It stores the client file descriptor, the SSL objects, and data
 * which is waiting to be either written to socket or encrypted. */
struct ssl_client
{
  int fd;

  SSL *ssl;

  BIO *rbio; /* SSL reads from, we write to. */
  BIO *wbio; /* SSL writes to, we read from. */

  /* Bytes waiting to be written to socket. This is data that has been generated
   * by the SSL object, either due to encryption of user input, or, writes
   * requires due to peer-requested SSL renegotiation. */
  char *write_buf;
  size_t write_len;

  /* Bytes waiting to be encrypted by the SSL object. */
  char *encrypt_buf;
  size_t encrypt_len;

  /* Method to invoke when unencrypted bytes are available. */
  void (*io_on_read)(char *buf, size_t len);
} client;

/* This enum contols whether the SSL connection needs to initiate the SSL
 * handshake. */
enum ssl_mode
{
  SSLMODE_SERVER,
  SSLMODE_CLIENT
};

void ssl_client_init(struct ssl_client *p,
                     int fd,
                     enum ssl_mode mode)
{
  memset(p, 0, sizeof(struct ssl_client));

  p->fd = fd;

  p->rbio = BIO_new(BIO_s_mem());
  p->wbio = BIO_new(BIO_s_mem());
  p->ssl = SSL_new(ctx);

  if (mode == SSLMODE_SERVER)
    SSL_set_accept_state(p->ssl); /* ssl server mode */
  else if (mode == SSLMODE_CLIENT)
    SSL_set_connect_state(p->ssl); /* ssl client mode */

  SSL_set_bio(p->ssl, p->rbio, p->wbio);

  p->io_on_read = print_unencrypted_data;
}

// inline static void noop(void *mod, b_lean_obj_arg fn) {}

// inline static void ssl_client_cleanup(void *p)
// {
//   SSL_free(((struct ssl_client *)p)->ssl); /* free the SSL object and its BIO's */
//   free(((struct ssl_client *)p)->write_buf);
//   free(((struct ssl_client *)p)->encrypt_buf);
// }

// static lean_external_class *g_ssl_client_class = NULL;

// static lean_external_class *get_ssl_client_class()
// {
//   if (g_ssl_client_class == NULL)
//   {
//     g_ssl_client_class = lean_register_external_class(
//         ssl_client_cleanup, noop);
//   }
//   return g_ssl_client_class;
// }

// /**
//  * Initialize a hasher.
//  */
// lean_obj_res lean_ssl_client_init()
// {
//   struct ssl_client *a = malloc(sizeof(struct ssl_client));
//   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//   if (sockfd < 0)
//     die("socket()");

//   ssl_client_init(a, sockfd, SSLMODE_CLIENT);
//   return lean_alloc_external(get_ssl_client_class(), a);
// }

// int ssl_client_want_write(struct ssl_client *cp)
// {
//   return (cp->write_len > 0);
// }

// /* Obtain the return value of an SSL operation and convert into a simplified
//  * error code, which is easier to examine for failure. */
// enum sslstatus
// {
//   SSLSTATUS_OK,
//   SSLSTATUS_WANT_IO,
//   SSLSTATUS_FAIL
// };

// static enum sslstatus get_sslstatus(SSL *ssl, int n)
// {
//   switch (SSL_get_error(ssl, n))
//   {
//   case SSL_ERROR_NONE:
//     return SSLSTATUS_OK;
//   case SSL_ERROR_WANT_WRITE:
//   case SSL_ERROR_WANT_READ:
//     return SSLSTATUS_WANT_IO;
//   case SSL_ERROR_ZERO_RETURN:
//   case SSL_ERROR_SYSCALL:
//   default:
//     return SSLSTATUS_FAIL;
//   }
// }

// /* Handle request to send unencrypted data to the SSL.  All we do here is just
//  * queue the data into the encrypt_buf for later processing by the SSL
//  * object. */
// void send_unencrypted_bytes(const char *buf, size_t len)
// {
//   client.encrypt_buf = (char *)realloc(client.encrypt_buf, client.encrypt_len + len);
//   memcpy(client.encrypt_buf + client.encrypt_len, buf, len);
//   client.encrypt_len += len;
// }

// /* Queue encrypted bytes. Should only be used when the SSL object has requested a
//  * write operation. */
// void queue_encrypted_bytes(const char *buf, size_t len)
// {
//   client.write_buf = (char *)realloc(client.write_buf, client.write_len + len);
//   memcpy(client.write_buf + client.write_len, buf, len);
//   client.write_len += len;
// }

// enum sslstatus do_ssl_handshake()
// {
//   char buf[DEFAULT_BUF_SIZE];
//   enum sslstatus status;

//   int n = SSL_do_handshake(client.ssl);
//   status = get_sslstatus(client.ssl, n);

//   /* Did SSL request to write bytes? */
//   if (status == SSLSTATUS_WANT_IO)
//     do
//     {
//       n = BIO_read(client.wbio, buf, sizeof(buf));
//       if (n > 0)
//         queue_encrypted_bytes(buf, n);
//       else if (!BIO_should_retry(client.wbio))
//         return SSLSTATUS_FAIL;
//     } while (n > 0);

//   return status;
// }

// /* Process SSL bytes received from the peer. The data needs to be fed into the
//    SSL object to be unencrypted.  On success, returns 0, on SSL error -1. */
// int on_read_cb(char *src, size_t len)
// {
//   char buf[DEFAULT_BUF_SIZE];
//   enum sslstatus status;
//   int n;

//   while (len > 0)
//   {
//     n = BIO_write(client.rbio, src, len);

//     if (n <= 0)
//       return -1; /* assume bio write failure is unrecoverable */

//     src += n;
//     len -= n;

//     if (!SSL_is_init_finished(client.ssl))
//     {
//       if (do_ssl_handshake() == SSLSTATUS_FAIL)
//         return -1;
//       if (!SSL_is_init_finished(client.ssl))
//         return 0;
//     }

//     /* The encrypted data is now in the input bio so now we can perform actual
//      * read of unencrypted data. */

//     do
//     {
//       n = SSL_read(client.ssl, buf, sizeof(buf));
//       if (n > 0)
//         client.io_on_read(buf, (size_t)n);
//     } while (n > 0);

//     status = get_sslstatus(client.ssl, n);

//     /* Did SSL request to write bytes? This can happen if peer has requested SSL
//      * renegotiation. */
//     if (status == SSLSTATUS_WANT_IO)
//       do
//       {
//         n = BIO_read(client.wbio, buf, sizeof(buf));
//         if (n > 0)
//           queue_encrypted_bytes(buf, n);
//         else if (!BIO_should_retry(client.wbio))
//           return -1;
//       } while (n > 0);

//     if (status == SSLSTATUS_FAIL)
//       return -1;
//   }

//   return 0;
// }

// /* Process outbound unencrypted data that is waiting to be encrypted.  The
//  * waiting data resides in encrypt_buf.  It needs to be passed into the SSL
//  * object for encryption, which in turn generates the encrypted bytes that then
//  * will be queued for later socket write. */
// int do_encrypt()
// {
//   char buf[DEFAULT_BUF_SIZE];
//   enum sslstatus status;

//   if (!SSL_is_init_finished(client.ssl))
//     return 0;

//   while (client.encrypt_len > 0)
//   {
//     int n = SSL_write(client.ssl, client.encrypt_buf, client.encrypt_len);
//     status = get_sslstatus(client.ssl, n);

//     if (n > 0)
//     {
//       /* consume the waiting bytes that have been used by SSL */
//       if ((size_t)n < client.encrypt_len)
//         memmove(client.encrypt_buf, client.encrypt_buf + n, client.encrypt_len - n);
//       client.encrypt_len -= n;
//       client.encrypt_buf = (char *)realloc(client.encrypt_buf, client.encrypt_len);

//       /* take the output of the SSL object and queue it for socket write */
//       do
//       {
//         n = BIO_read(client.wbio, buf, sizeof(buf));
//         if (n > 0)
//           queue_encrypted_bytes(buf, n);
//         else if (!BIO_should_retry(client.wbio))
//           return -1;
//       } while (n > 0);
//     }

//     if (status == SSLSTATUS_FAIL)
//       return -1;

//     if (n == 0)
//       break;
//   }
//   return 0;
// }

// /* Read bytes from stdin and queue for later encryption. */
// void do_stdin_read()
// {
//   char buf[DEFAULT_BUF_SIZE];
//   ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
//   if (n > 0)
//     send_unencrypted_bytes(buf, (size_t)n);
// }

// /* Read encrypted bytes from socket. */
// int do_sock_read()
// {
//   char buf[DEFAULT_BUF_SIZE];
//   ssize_t n = read(client.fd, buf, sizeof(buf));

//   if (n > 0)
//     return on_read_cb(buf, (size_t)n);
//   else
//     return -1;
// }

// /* Write encrypted bytes to the socket. */
// int do_sock_write()
// {
//   ssize_t n = write(client.fd, client.write_buf, client.write_len);
//   if (n > 0)
//   {
//     if ((size_t)n < client.write_len)
//       memmove(client.write_buf, client.write_buf + n, client.write_len - n);
//     client.write_len -= n;
//     client.write_buf = (char *)realloc(client.write_buf, client.write_len);
//     return 0;
//   }
//   else
//     return -1;
// }

void ssl_init(const char *certfile, const char *keyfile)
{
  printf("initialising SSL\n");

  /* SSL library initialisation */
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  ERR_load_crypto_strings();

  /* create the SSL server context */
  ctx = SSL_CTX_new(SSLv23_method());
  if (!ctx)
    die("SSL_CTX_new()");

  /* Load certificate and private key files, and check consistency */
  if (certfile && keyfile)
  {
    if (SSL_CTX_use_certificate_file(ctx, certfile, SSL_FILETYPE_PEM) != 1)
      int_error("SSL_CTX_use_certificate_file failed");

    if (SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) != 1)
      int_error("SSL_CTX_use_PrivateKey_file failed");

    /* Make sure the key and certificate file match. */
    if (SSL_CTX_check_private_key(ctx) != 1)
      int_error("SSL_CTX_check_private_key failed");
    else
      printf("certificate and private key loaded and verified\n");
  }

  /* Recommended to avoid SSLv2 & SSLv3 */
  SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
}

// void lean_ssl_init(b_lean_obj_arg b_certfile, b_lean_obj_arg b_keyfile)
// {
//   const char *certfile = lean_string_cstr(b_certfile);
//   const char *keyfile = lean_string_cstr(b_keyfile);
//   ssl_init(certfile, keyfile);
// }

// /**
//  * Ensure the hasher is exclusive.
//  */
// /* static inline lean_obj_res lean_ensure_exclusive_blake3_hasher(lean_obj_arg a) { */
// /*   if (lean_is_exclusive(a)) */
// /*     return a; */
// /*   return blake3_hasher_copy(a); */
// /* } */

// /* lean_obj_res lean_blake3_hasher_update(lean_obj_arg self, b_lean_obj_arg input, */
// /*                                        size_t input_len) { */
// /* #ifdef DEBUG */
// /*   printf("lean_blake3_hasher_update"); */
// /* #endif */
// /*   lean_object *a = lean_ensure_exclusive_blake3_hasher(self); */
// /*   blake3_hasher_update(lean_get_external_data(a), lean_sarray_cptr(input), */
// /*                        input_len); */
// /*   return a; */
// /* } */
