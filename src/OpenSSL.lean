
namespace OpenSSL

constant SslClientPointed : NonemptyType

def SslClient : Type := SslClientPointed.type

instance : Nonempty SslClient := SslClientPointed.property

@[extern "ssl_init"]
constant sslInit : (certfile : @& String) → (keyfile : @& String) → IO Unit

@[extern "ssl_client_init"]
constant sslClientInit : IO SslClient
