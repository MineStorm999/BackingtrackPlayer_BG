#include <cstring>
#include <iostream>
#include <libwebsockets.h>
#include <string>
#include <vector>

// Hilfsfunktion: Sendet HTTP-Header + JSON-Body sauber über libwebsockets
static int send_http_json(struct lws *wsi, int status,
                          const std::string &json) {
  unsigned char buf[LWS_PRE + 2048];
  unsigned char *start = &buf[LWS_PRE];
  unsigned char *p = start;
  unsigned char *end = &buf[sizeof(buf) - 1];

  // 1. HTTP-Status und Header schreiben
  if (lws_add_http_common_headers(wsi, status, "application/json", json.size(),
                                  &p, end))
    return 1;

  // Optional: CORS-Header erlauben (für Zugriff aus Browser-JS)
  if (lws_add_http_header_by_name(
          wsi, (const unsigned char *)"Access-Control-Allow-Origin:",
          (const unsigned char *)"*", 1, &p, end))
    return 1;

  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  // 2. JSON-Body direkt hinterher senden
  std::memcpy(start, json.c_str(), json.size());
  lws_write(wsi, start, json.size(), LWS_WRITE_HTTP_FINAL);

  // Verbindung sauber schließen (oder für Keep-Alive recyceln)
  if (lws_http_transaction_completed(wsi))
    return -1;

  return 0;
}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
  if (reason == LWS_CALLBACK_HTTP) {
    // 'in' enthält den angefragten Pfad, z. B. "/names" oder "/start"
    std::string path(static_cast<const char *>(in), len);

    if (path == "/names") {
      std::string json = R"({"names":["Max","Anna","Lukas"]})";
      return send_http_json(wsi, HTTP_STATUS_OK, json);
    } else if (path == "/start") {
      std::cout << "Signal empfangen: START\n";
      return send_http_json(wsi, HTTP_STATUS_OK, R"({"status":"started"})");
    } else if (path == "/stop") {
      std::cout << "Signal empfangen: STOP\n";
      return send_http_json(wsi, HTTP_STATUS_OK, R"({"status":"stopped"})");
    } else {
      return send_http_json(wsi, HTTP_STATUS_NOT_FOUND,
                            R"({"error":"not found"})");
    }
  }
  return 0;
}

static struct lws_protocols protocols[] = {{"http-only", callback_http, 0, 0},
                                           {nullptr, nullptr, 0, 0}};

int main() {
  struct lws_context_creation_info info{};
  info.port = 8080;
  info.protocols = protocols;

  struct lws_context *context = lws_create_context(&info);
  if (!context)
    return 1;

  std::cout << "LWS HTTP-Server läuft auf http://0.0.0\n";

  while (true) {
    lws_service(context, 50);
  }

  lws_context_destroy(context);
  return 0;
}
