#ifndef MCP_LINK_H
#define MCP_LINK_H

#include "mcp_api.h"
#include "mcp_websocket.h"


/* Letsencrypt root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is namedfreee
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
// extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
// extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

// static const char *REQUEST = "POST " AUTH_ENDPOINT "?email=" CONFIG_USERNAME    "&password=" CONFIG_PASSWORD "\r\n"
//     "Host: " WEB_SERVER "\r\n"
//     "Content-Type: application/x-www-form-urlencoded\r\n"
//     "\r\n";

class MCPLink 
{
    private:
        bool use_websockets;

        int authenticate_nfc(char *nfc_id);

    public:
        MCPLink();

        void ConnectWebsocket();
        int DisconnectWebsocket();
        bool WebsocketConnected();

        int AuthenticateNFC(char *nfc_id);
};


#endif