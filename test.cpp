// player_demo.cpp – Beispiel für die Player-Logik nach Protokoll btp.v1
//
// Zeigt: Nachrichten empfangen -> Logik entscheidet -> Antworten/Zustand
// senden. Ohne Abhängigkeiten: nur C++17 und POSIX-Sockets (Linux, Raspberry Pi
// OS, macOS). Wiedergabe ist simuliert (Timer statt Audio). An den markierten
// Stellen würde der echte Player ALSA bedienen.
//
// Bauen:   g++ -std=c++17 -O2 -Wall -o player_demo player_demo.cpp
// Starten: ./player_demo           WebSocket-Server ws://<host>:8080/ws
// (Subprotocol btp.v1)
//          ./player_demo --stdin   zusätzlich: eine JSON-Nachricht pro Zeile
//          über die Tastatur
//                                  (so verhält sich später auch der USB-Weg
//                                  über die panel-bridge)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

// ============================================================================
// 1) Kleine Helfer: Log, SHA-1, Base64 (nur für den WebSocket-Handshake)
// ============================================================================

static void logln(const std::string &s) {
  std::cerr << "[player] " << s << std::endl;
}

static uint32_t rol(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

static std::string sha1(const std::string &in) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  std::string m = in;
  uint64_t bits = (uint64_t)in.size() * 8;
  m += (char)0x80;
  while (m.size() % 64 != 56)
    m += (char)0x00;
  for (int i = 7; i >= 0; --i)
    m += (char)((bits >> (i * 8)) & 0xFF);
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t)(uint8_t)m[off + 4 * i] << 24 |
             (uint32_t)(uint8_t)m[off + 4 * i + 1] << 16 |
             (uint32_t)(uint8_t)m[off + 4 * i + 2] << 8 |
             (uint32_t)(uint8_t)m[off + 4 * i + 3];
    for (int i = 16; i < 80; ++i)
      w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rol(b, 30);
      b = a;
      a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  std::string out;
  for (uint32_t v : h)
    for (int i = 3; i >= 0; --i)
      out += (char)((v >> (i * 8)) & 0xFF);
  return out;
}

static std::string base64(const std::string &in) {
  static const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  for (; i + 2 < in.size(); i += 3) {
    uint32_t n =
        (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 | (uint8_t)in[i + 2];
    out += t[n >> 18];
    out += t[(n >> 12) & 63];
    out += t[(n >> 6) & 63];
    out += t[n & 63];
  }
  if (i + 1 == in.size()) {
    uint32_t n = (uint8_t)in[i] << 16;
    out += t[n >> 18];
    out += t[(n >> 12) & 63];
    out += "==";
  } else if (i + 2 == in.size()) {
    uint32_t n = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8;
    out += t[n >> 18];
    out += t[(n >> 12) & 63];
    out += t[(n >> 6) & 63];
    out += '=';
  }
  return out;
}

// ============================================================================
// 2) Mini-JSON: liest ein flaches Objekt {"key": wert, ...}, schreibt Strings
// sicher
//    (reicht für unser Protokoll; verschachtelte Werte werden roh übernommen)
// ============================================================================

using JsonObj = std::map<std::string, std::string>;

static void skipWs(const std::string &s, size_t &i) {
  while (i < s.size() &&
         (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
    ++i;
}

static bool parseString(const std::string &s, size_t &i, std::string &out) {
  if (i >= s.size() || s[i] != '"')
    return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"')
      return true;
    if (c == '\\') {
      if (i >= s.size())
        return false;
      char e = s[i++];
      switch (e) {
      case 'n':
        out += '\n';
        break;
      case 't':
        out += '\t';
        break;
      case 'r':
        out += '\r';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'u': {
        if (i + 4 > s.size())
          return false;
        unsigned v = std::stoul(s.substr(i, 4), nullptr, 16);
        i += 4;
        out += (v < 0x80) ? (char)v : '?'; // Demo: Nicht-ASCII vereinfacht
        break;
      }
      default:
        out += e; // \" \\ \/
      }
    } else {
      out += c;
    }
  }
  return false;
}

static bool parseFlatJson(const std::string &s, JsonObj &out) {
  size_t i = 0;
  skipWs(s, i);
  if (i >= s.size() || s[i] != '{')
    return false;
  ++i;
  while (true) {
    skipWs(s, i);
    if (i < s.size() && s[i] == '}')
      return true;
    std::string key, val;
    if (!parseString(s, i, key))
      return false;
    skipWs(s, i);
    if (i >= s.size() || s[i] != ':')
      return false;
    ++i;
    skipWs(s, i);
    if (i >= s.size())
      return false;
    if (s[i] == '"') {
      if (!parseString(s, i, val))
        return false;
    } else if (s[i] == '{' || s[i] == '[') { // verschachtelt: roh merken
      size_t start = i;
      int depth = 0;
      bool inStr = false;
      for (; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) {
          if (c == '\\')
            ++i;
          else if (c == '"')
            inStr = false;
          continue;
        }
        if (c == '"')
          inStr = true;
        else if (c == '{' || c == '[')
          ++depth;
        else if (c == '}' || c == ']') {
          if (--depth == 0) {
            ++i;
            break;
          }
        }
      }
      val = s.substr(start, i - start);
    } else { // Zahl, true, false, null
      size_t start = i;
      while (i < s.size() && s[i] != ',' && s[i] != '}')
        ++i;
      val = s.substr(start, i - start);
      while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
        val.pop_back();
    }
    out[key] = val;
    skipWs(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      continue;
    }
    if (i < s.size() && s[i] == '}')
      return true;
    return false;
  }
}

static std::string getStr(const JsonObj &o, const std::string &k,
                          const std::string &def = "") {
  auto it = o.find(k);
  return it == o.end() ? def : it->second;
}

static bool getInt(const JsonObj &o, const std::string &k, long &v) {
  auto it = o.find(k);
  if (it == o.end())
    return false;
  char *end = nullptr;
  v = std::strtol(it->second.c_str(), &end, 10);
  return end && *end == '\0' && !it->second.empty();
}

static std::string
q(const std::string &s) { // als JSON-String mit Anführungszeichen
  std::string o = "\"";
  for (char c : s) {
    switch (c) {
    case '"':
      o += "\\\"";
      break;
    case '\\':
      o += "\\\\";
      break;
    case '\n':
      o += "\\n";
      break;
    case '\r':
      o += "\\r";
      break;
    case '\t':
      o += "\\t";
      break;
    default:
      if ((unsigned char)c < 0x20) {
        char b[8];
        snprintf(b, sizeof b, "\\u%04x", c);
        o += b;
      } else
        o += c;
    }
  }
  return o + "\"";
}

static std::string num1(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// ============================================================================
// 3) Der Player-Zustand (die "einzige Wahrheit")
// ============================================================================

enum class Status { Loading, Ready, Playing, Stopped, ShowEnd, Error };

static const char *statusName(Status s) {
  switch (s) {
  case Status::Loading:
    return "loading";
  case Status::Ready:
    return "ready";
  case Status::Playing:
    return "playing";
  case Status::Stopped:
    return "stopped";
  case Status::ShowEnd:
    return "show_end";
  case Status::Error:
    return "error";
  }
  return "error";
}

struct Song {
  std::string name;
  double dur;
};

struct Player {
  // Demo-Show mit kurzen Dauern, damit man Songende und "Show beendet" schnell
  // sieht. Der echte Player liest das von /mnt/shows.
  std::string show = "Demo-Show";
  std::vector<Song> songs = {
      {"Intro", 8.0}, {"Song-A", 12.0}, {"Song-B", 10.0}};
  int index = 1; // 1-basiert, wie im Protokoll
  Status status = Status::Ready;
  double pos =
      0.0; // Zeigt: Nachrichten empfangen -> Logik entscheidet ->
           // Antworten/Zustand senden.        // Sekunden im aktuellen Song
  long rev = 1;                // steigt bei jeder Zustandsänderung
  Clock::time_point playStart; // für die simulierte Wiedergabe
  Clock::time_point lastProgress;
  int count() const { return (int)songs.size(); }
};

// ============================================================================
// 4) Verbindungen (WebSocket-Clients und optional stdin)
// ============================================================================

struct Client {
  int fd = -1;
  bool isWs = true; // false = stdin/stdout, eine Nachricht pro Zeile
  bool handshakeDone = false;
  bool dead = false;
  std::string inBuf;
  std::string name; // aus "hello": rack, drum, web
  Clock::time_point lastSeen = Clock::now();
  std::deque<std::string> recentIds; // Duplikaterkennung, letzte 32 IDs
};

static Player P;
static std::map<int, Client> clients;

static void writeAll(int fd, const std::string &data) {
  size_t off = 0;
  while (off < data.size()) {
#ifdef MSG_NOSIGNAL
    ssize_t n =
        (fd == STDOUT_FILENO)
            ? write(fd, data.data() + off, data.size() - off)
            : send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
#else
    ssize_t n = write(fd, data.data() + off, data.size() - off);
#endif
    if (n <= 0)
      return;
    off += (size_t)n;
  }
}

static std::string wsFrame(uint8_t opcode, const std::string &payload) {
  std::string f;
  f += (char)(0x80 | opcode); // FIN + Opcode; Server sendet unmaskiert
  size_t n = payload.size();
  if (n < 126) {
    f += (char)n;
  } else if (n < 65536) {
    f += (char)126;
    f += (char)(n >> 8);
    f += (char)(n & 0xFF);
  } else {
    f += (char)127;
    for (int i = 7; i >= 0; --i)
      f += (char)(((uint64_t)n >> (i * 8)) & 0xFF);
  }
  return f + payload;
}

static void sendTo(Client &c, const std::string &json) {
  if (c.dead)
    return;
  if (c.isWs)
    writeAll(c.fd, wsFrame(0x1, json));
  else
    writeAll(STDOUT_FILENO, json + "\n");
}

// ============================================================================
// 5) Nachrichten bauen (Pi -> Client)
// ============================================================================

static bool clientConnected(const std::string &name) {
  for (auto &kv : clients)
    if (!kv.second.dead && kv.second.name == name)
      return true;
  return false;
}

static std::string stateJson() {
  const Song *s =
      (P.index >= 1 && P.index <= P.count()) ? &P.songs[P.index - 1] : nullptr;
  std::ostringstream o;
  o << "{\"type\":\"state\",\"rev\":" << P.rev
    << ",\"status\":" << q(statusName(P.status)) << ",\"index\":" << P.index
    << ",\"count\":" << P.count() << ",\"show\":" << q(P.show)
    << ",\"song\":" << q(s ? s->name : "")
    << ",\"dur\":" << num1(s ? s->dur : 0.0)
    << ",\"clients\":{\"rack\":" << (clientConnected("rack") ? "true" : "false")
    << ",\"drum\":" << (clientConnected("drum") ? "true" : "false")
    << ",\"web\":" << (clientConnected("web") ? "true" : "false") << "}"
    << ",\"error\":null}";
  return o.str();
}

static std::string setlistJson() {
  std::ostringstream o;
  o << "{\"type\":\"setlist\",\"show\":" << q(P.show) << ",\"songs\":[";
  for (int i = 0; i < P.count(); ++i) {
    if (i)
      o << ",";
    o << "{\"i\":" << (i + 1) << ",\"name\":" << q(P.songs[i].name)
      << ",\"dur\":" << num1(P.songs[i].dur) << "}";
  }
  o << "]}";
  return o.str();
}

static std::string ackJson(const std::string &id, bool ok,
                           const std::string &err = "") {
  std::string s = "{\"type\":\"ack\",\"id\":" + q(id) +
                  ",\"ok\":" + (ok ? "true" : "false");
  if (!ok)
    s += ",\"error\":" + q(err);
  return s + "}";
}

static void broadcast(const std::string &json) {
  for (auto &kv : clients)
    if (kv.second.handshakeDone && !kv.second.dead)
      sendTo(kv.second, json);
}

static void stateChanged(const std::string &why) {
  ++P.rev;
  logln("Zustand rev " + std::to_string(P.rev) + ": " + statusName(P.status) +
        ", Song " + std::to_string(P.index) + " (" + why + ")");
  broadcast(stateJson());
}

// ============================================================================
// 6) DIE LOGIK: was passiert bei welchem Befehl?
// ============================================================================

struct Result {
  bool ok;
  std::string error;
  bool changed;
};

static Result doCommand(const std::string &action, const JsonObj &m) {
  if (action == "get_state") {
    return {true, "", false};
  }

  if (action == "select_song") {
    if (P.status == Status::Playing)
      return {false, "not_allowed_while_playing", false};
    long idx;
    if (!getInt(m, "index", idx) || idx < 1 || idx > P.count())
      return {false, "bad_index", false};
    if (idx == P.index && P.status == Status::Ready)
      return {true, "", false}; // nichts zu tun
    P.index = (int)idx;
    P.pos = 0.0;
    P.status = Status::Ready;
    // Echter Player: Song-Datei öffnen und Puffer vorladen.
    return {true, "", true};
  }

  if (action == "play") {
    if (P.status == Status::Playing)
      return {true, "", false}; // schon an: kein Fehler, keine Änderung
    if (P.status == Status::ShowEnd)
      return {false, "show_ended", false};
    if (P.status == Status::Loading || P.status == Status::Error)
      return {false, "no_show", false};
    P.status = Status::Playing;
    P.pos = 0.0;
    P.playStart = Clock::now();
    P.lastProgress = P.playStart;
    // Echter Player: ab hier Samples des Songs statt Stille in den offenen
    // ALSA-Strom schreiben.
    return {true, "", true};
  }

  if (action == "stop") {
    if (P.status != Status::Playing)
      return {true, "", false}; // nichts läuft: ohne Wirkung
    // Offene Entscheidung im Projekt: Pause oder zurück zum Songanfang.
    // Hier: zurück zum Anfang desselben Songs, Status "stopped".
    P.status = Status::Stopped;
    P.pos = 0.0;
    // Echter Player: wieder Stille in den ALSA-Strom schreiben (Strom NICHT
    // schließen).
    return {true, "", true};
  }

  return {false, "unknown_action", false};
}

static bool isDuplicate(Client &c, const std::string &id) {
  if (std::find(c.recentIds.begin(), c.recentIds.end(), id) !=
      c.recentIds.end())
    return true;
  c.recentIds.push_back(id);
  if (c.recentIds.size() > 32)
    c.recentIds.pop_front();
  return false;
}

static void handleMessage(Client &c, const std::string &text) {
  c.lastSeen = Clock::now();
  JsonObj m;
  if (!parseFlatJson(text, m)) {
    sendTo(c, "{\"type\":\"error\",\"error\":\"bad_message\"}");
    return;
  }
  const std::string type = getStr(m, "type");

  if (type == "ping") {
    sendTo(c, "{\"type\":\"pong\"}");
    return;
  }
  if (type == "pong") {
    return;
  }

  if (type == "hello") {
    long proto = 0;
    if (!getInt(m, "proto", proto) || proto != 1) {
      sendTo(c, "{\"type\":\"error\",\"error\":\"unsupported_proto\"}");
      return;
    }
    c.name = getStr(m, "client", "unknown");
    logln("hello von " + c.name + " (fw " + getStr(m, "fw", "?") + ")");
    sendTo(c,
           "{\"type\":\"welcome\",\"proto\":1,\"server\":\"player_demo 0.1\"}");
    sendTo(c, setlistJson());
    stateChanged(c.name +
                 " verbunden"); // alle sehen den neuen Client in "clients"
    return;
  }

  if (type == "command") {
    const std::string id = getStr(m, "id");
    const std::string action = getStr(m, "action");
    if (id.empty()) {
      sendTo(c, "{\"type\":\"error\",\"error\":\"missing_id\"}");
      return;
    }
    if (isDuplicate(c, id)) {
      logln("Duplikat ignoriert: " + id);
      sendTo(c, ackJson(id, true)); // bestätigen, aber nicht nochmal ausführen
      return;
    }
    Result r = doCommand(action, m);
    logln("Befehl " + action + " (" + id + ") -> " + (r.ok ? "ok" : r.error));
    sendTo(c, ackJson(id, r.ok, r.error));
    if (r.changed)
      stateChanged(action);
    else if (action == "get_state")
      sendTo(c, stateJson());
    return;
  }

  sendTo(c, "{\"type\":\"error\",\"error\":\"unknown_type\"}");
}

// Läuft ca. alle 50 ms: simulierte Wiedergabe, Fortschritt, Songende.
static void tick() {
  if (P.status != Status::Playing)
    return;
  auto now = Clock::now();
  P.pos = std::chrono::duration<double>(now - P.playStart).count();
  const double dur = P.songs[P.index - 1].dur;

  if (P.pos >= dur) {
    // Songende: zum nächsten Song vorrücken, aber NIE automatisch starten.
    if (P.index < P.count()) {
      ++P.index;
      P.status = Status::Ready;
    } else {
      P.status = Status::ShowEnd;
    }
    P.pos = 0.0;
    stateChanged("Songende");
    return;
  }

  if (now - P.lastProgress >= std::chrono::milliseconds(500)) {
    P.lastProgress = now;
    broadcast("{\"type\":\"progress\",\"rev\":" + std::to_string(P.rev) +
              ",\"pos\":" + num1(P.pos) + "}");
  }
}

// ============================================================================
// 7) WebSocket-Transport: Handshake und Frames (RFC 6455, nur das Nötige)
// ============================================================================

static std::string headerValue(const std::string &req,
                               const std::string &name) {
  std::istringstream in(req);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    std::string k = line.substr(0, colon);
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
    if (k == name) {
      std::string v = line.substr(colon + 1);
      v.erase(0, v.find_first_not_of(" \t"));
      return v;
    }
  }
  return "";
}

static void closeClient(Client &c, const std::string &why) {
  if (c.dead)
    return;
  logln("Verbindung " + (c.name.empty() ? std::to_string(c.fd) : c.name) +
        " getrennt: " + why);
  c.dead = true;
}

static bool doHandshake(Client &c) {
  size_t end = c.inBuf.find("\r\n\r\n");
  if (end == std::string::npos) {
    if (c.inBuf.size() > 8192)
      closeClient(c, "Handshake zu groß");
    return false;
  }
  std::string req = c.inBuf.substr(0, end + 4);
  c.inBuf.erase(0, end + 4);

  std::string path;
  {
    std::istringstream rl(req);
    std::string method;
    rl >> method >> path;
  }
  std::string key = headerValue(req, "sec-websocket-key");
  std::string proto = headerValue(req, "sec-websocket-protocol");

  if (path != "/ws" || key.empty() ||
      proto.find("btp.v1") == std::string::npos) {
    writeAll(c.fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
    closeClient(c,
                "falscher Handshake (Pfad /ws und Subprotocol btp.v1 nötig)");
    return false;
  }
  std::string accept =
      base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
  writeAll(c.fd, "HTTP/1.1 101 Switching Protocols\r\n"
                 "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: " +
                     accept +
                     "\r\n"
                     "Sec-WebSocket-Protocol: btp.v1\r\n\r\n");
  c.handshakeDone = true;
  c.lastSeen = Clock::now();
  logln("WebSocket verbunden (fd " + std::to_string(c.fd) + ")");
  return true;
}

static void processFrames(Client &c) {
  auto &b = c.inBuf;
  while (!c.dead && b.size() >= 2) {
    uint8_t b0 = (uint8_t)b[0], b1 = (uint8_t)b[1];
    bool fin = b0 & 0x80;
    int op = b0 & 0x0F;
    bool masked = b1 & 0x80;
    uint64_t len = b1 & 0x7F;
    size_t hdr = 2;
    if (len == 126) {
      if (b.size() < 4)
        return;
      len = (uint64_t)(uint8_t)b[2] << 8 | (uint8_t)b[3];
      hdr = 4;
    } else if (len == 127) {
      if (b.size() < 10)
        return;
      len = 0;
      for (int i = 0; i < 8; ++i)
        len = (len << 8) | (uint8_t)b[2 + i];
      hdr = 10;
    }
    if (!masked) {
      closeClient(c, "Client-Frame nicht maskiert");
      return;
    }
    if (len > 64 * 1024) {
      closeClient(c, "Nachricht zu groß");
      return;
    }
    if (b.size() < hdr + 4 + len)
      return; // Frame noch nicht komplett

    std::string mask = b.substr(hdr, 4);
    std::string payload = b.substr(hdr + 4, (size_t)len);
    for (size_t i = 0; i < payload.size(); ++i)
      payload[i] ^= mask[i % 4];
    b.erase(0, hdr + 4 + (size_t)len);

    if (!fin || op == 0x0) {
      closeClient(c, "fragmentierte Nachrichten nicht unterstützt");
      return;
    }
    switch (op) {
    case 0x1:
      handleMessage(c, payload);
      break; // Text = JSON
    case 0x8:
      writeAll(c.fd, wsFrame(0x8, ""));
      closeClient(c, "Close vom Client");
      return;
    case 0x9:
      writeAll(c.fd, wsFrame(0xA, payload));
      c.lastSeen = Clock::now();
      break; // Ping
    case 0xA:
      c.lastSeen = Clock::now();
      break; // Pong
    default:
      closeClient(c, "Binärframes nicht erlaubt");
      return;
    }
  }
}

static void onReadable(Client &c) {
  char buf[4096];
  ssize_t n = read(c.fd, buf, sizeof buf);
  if (n <= 0) {
    if (!c.isWs) {
      logln("stdin beendet");
      exit(0);
    }
    closeClient(c, "Socket geschlossen");
    return;
  }
  c.inBuf.append(buf, (size_t)n);

  if (!c.isWs) { // stdin: eine JSON-Nachricht pro Zeile
    size_t nl;
    while ((nl = c.inBuf.find('\n')) != std::string::npos) {
      std::string line = c.inBuf.substr(0, nl);
      c.inBuf.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (!line.empty())
        handleMessage(c, line);
    }
    return;
  }
  if (!c.handshakeDone && !doHandshake(c))
    return;
  processFrames(c);
}

// ============================================================================
// 8) Hauptschleife
// ============================================================================

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);
  const bool withStdin = argc > 1 && std::string(argv[1]) == "--stdin";
  const int port = 8080;

  int lfd = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(lfd, (sockaddr *)&addr, sizeof addr) < 0 || listen(lfd, 8) < 0) {
    perror("bind/listen");
    return 1;
  }
  logln("lauscht auf ws://0.0.0.0:" + std::to_string(port) +
        "/ws (Subprotocol btp.v1)");

  if (withStdin) {
    Client c;
    c.fd = STDIN_FILENO;
    c.isWs = false;
    c.handshakeDone = true;
    clients[STDIN_FILENO] = c;
    logln("stdin aktiv: eine JSON-Nachricht pro Zeile eingeben");
  }

  // Echter Player: hier ALSA-Gerät öffnen, Stille ausgeben, dann systemd
  // "READY=1" melden.

  while (true) {
    std::vector<pollfd> fds;
    fds.push_back({lfd, POLLIN, 0});
    for (auto &kv : clients)
      fds.push_back({kv.first, POLLIN, 0});

    poll(fds.data(), fds.size(), 50); // 50 ms: Takt für tick()

    if (fds[0].revents & POLLIN) {
      int cfd = accept(lfd, nullptr, nullptr);
      if (cfd >= 0) {
        Client c;
        c.fd = cfd;
        clients[cfd] = c;
      }
    }
    for (size_t i = 1; i < fds.size(); ++i) {
      if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
        continue;
      auto it = clients.find(fds[i].fd);
      if (it != clients.end() && !it->second.dead)
        onReadable(it->second);
    }

    tick();

    // Heartbeat: Clients senden jede Sekunde "ping". 5 s Stille = Verbindung
    // tot.
    auto now = Clock::now();
    for (auto &kv : clients) {
      Client &c = kv.second;
      if (c.isWs && !c.dead && now - c.lastSeen > std::chrono::seconds(5))
        closeClient(c, "Heartbeat-Timeout");
    }

    // Tote Verbindungen entfernen; wenn ein benannter Client weg ist, Zustand
    // an alle.
    bool someoneLeft = false;
    for (auto it = clients.begin(); it != clients.end();) {
      if (it->second.dead) {
        if (!it->second.name.empty())
          someoneLeft = true;
        close(it->first);
        it = clients.erase(it);
      } else {
        ++it;
      }
    }
    if (someoneLeft)
      stateChanged("Client getrennt");
  }
}