// Comment or uncomment this line to select local or hosted
#define HOSTED

#ifdef HOSTED
#define NW_HTTP std::string("https://noteworthy.howdoesthiseven.work")
#define NW_WS std::string("wss://nw-ws.howdoesthiseven.work")
#else
#define NW_HTTP std::string("http://localhost:8080")
#define NW_WS std::string("ws://localhost:8081")
#endif

// PDF import needs NW_ENABLE_PDF=1 on the server. Keep this at 0 for the
// public demo so the upload button is hidden.
#ifndef NW_ENABLE_PDF
#define NW_ENABLE_PDF 0
#endif
