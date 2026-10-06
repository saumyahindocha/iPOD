#include "btstack_config_common.h"

// Pod patch: AVRCP cover art (album art over Bluetooth, via OBEX/BIP)
#define ENABLE_AVRCP_COVER_ART
// AVDTP signalling + media, AVRCP control + browsing, cover-art OBEX, spare
#undef  MAX_NR_L2CAP_CHANNELS
#define MAX_NR_L2CAP_CHANNELS 6
