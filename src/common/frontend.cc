#include "lightning/base/frontend.h"

namespace lightning::websocket {
DEFINE_double(ws_global_cps_limit_capacity, 10000, "CPS rate limiter capacity");
DEFINE_double(ws_global_cps_limit_rate_s, 5000, "CPS limit");
DEFINE_int32(ws_connection_count_limit, 100000, "max concurrent connections handled by this server");
DEFINE_double(ws_connection_uplink_qps_limit_capacity, 100, "uplink limit bucket size for each connection");
DEFINE_double(ws_connection_uplink_qps_limit_rate_s, 10, "uplink limit token increase rate for each connection");

}
