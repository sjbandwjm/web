#include "spsc_executor.h"

namespace jiayou::base {

DEFINE_bool(keyed_spsc_worker_pthread, false, "use pthread or bthread");
DEFINE_int32(keyed_spsc_worker_pthread_cnt, 8, "use spsc worker pthread cnt");

}
