#pragma once

#include <string>

#define NW_HTTP std::string(NW_HTTP_URL)
#define NW_WS std::string(NW_WS_URL)

// PDF import needs NW_ENABLE_PDF=1 on the server. Keep this at 0 for the
// public demo so the upload button is hidden.
#ifndef NW_ENABLE_PDF
#define NW_ENABLE_PDF 0
#endif
