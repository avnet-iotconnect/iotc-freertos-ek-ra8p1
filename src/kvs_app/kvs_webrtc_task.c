/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * KVS WebRTC master task for the EK-RA8P1 /IOTCONNECT vision demo.
 *
 * Flow:
 *   1. iotc_kvs_identity_hook() (net thread, during the identity flow)
 *      parses d.p.vs -> region/channel/endpoint/role-alias/thing-name.
 *   2. iotc_kvs_set_credentials() stores the device cert/key PEM.
 *   3. The task (created by iotc_kvs_start_task()) waits for both, then
 *      initialises the media pipeline + SDK and runs the signaling
 *      controller as master. Viewers connecting from the dashboard's
 *      Video Streaming tab drive the actual encode/stream start/stop.
 *   4. On error the controller is restarted with exponential backoff.
 */
#include "kvs_webrtc_task.h"

/* Must precede demo_config.h/app_common.h: redirects the AWS_* config
 * macros to the runtime pointers defined below. */
#include "kvs_runtime_config.h"
#include "demo_config.h"

#include <string.h>
#include <stdlib.h>

#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"

#include "logging_levels.h"
#define LOG_LEVEL LOG_INFO
#include "logging.h"

#include "cJSON.h"
#include "iotcl.h"
#include "iotc_time.h"

#include "app_common.h"
#include "app_media_source.h"

/* ── Runtime config globals (consumed via kvs_runtime_config.h macros) ─── */
const char *pcKvsAwsRegion           = "";
const char *pcKvsChannelName         = "";
const char *pcKvsCredentialsEndpoint = "";
const char *pcKvsIotThingName        = "";
const char *pcKvsIotRoleAlias        = "";
const char *pcKvsIotThingCert        = "";
const char *pcKvsIotPrivateKey       = "";

/* ── Task configuration ─────────────────────────────────────────────────── */
#define KVS_TASK_STACK_WORDS   ( 8192U )
#define KVS_TASK_PRIORITY      ( 1U )
#define KVS_RETRY_MIN_DELAY_MS ( 10000U )
#define KVS_RETRY_MAX_DELAY_MS ( 300000U )

/* ── State ──────────────────────────────────────────────────────────────── */

/* The SDK app context is ~large (SDP buffers, signaling + per-viewer peer
 * sessions); it does not fit next to the encoder pools in SRAM, so it goes
 * to (uncached) SDRAM. Zeroed manually before use. */
static AppContext_t s_app_ctx BSP_ALIGN_VARIABLE( 64 )
    BSP_PLACE_IN_SECTION( BSP_UNINIT_SECTION_PREFIX ".sdram_noinit" );
static AppMediaSourcesContext_t s_media_ctx;

static TaskHandle_t s_task;
static volatile bool s_config_ready;
static volatile bool s_creds_ready;
static const char *s_state = "off";

/* Parsed vs strings (heap, lifetime = app; re-parse frees the old set). */
static char *s_region, *s_channel, *s_endpoint, *s_role_alias, *s_thing;
static char *s_cert_pem, *s_key_pem;
static bool s_autostart;

static char *prv_strdup_len( const char *s, size_t n )
{
    char *d = pvPortMalloc( n + 1U );

    if( d != NULL )
    {
        memcpy( d, s, n );
        d[ n ] = '\0';
    }
    return d;
}

static char *prv_strdup( const char *s )
{
    return ( s != NULL ) ? prv_strdup_len( s, strlen( s ) ) : NULL;
}

static void prv_free_null( char **p )
{
    if( *p != NULL )
    {
        vPortFree( *p );
        *p = NULL;
    }
}

/* ── Identity parsing ───────────────────────────────────────────────────── */
/* ARN: arn:aws:kinesisvideo:<region>:<acct>:channel/<name>/<ts>
 * URL: https://<endpoint>/role-aliases/<alias>/credentials               */

void iotc_kvs_identity_hook( const char *identity_json )
{
    cJSON *root;
    cJSON *vs = NULL;
    const char *carn = NULL;
    const char *url = NULL;

    if( identity_json == NULL )
    {
        return;
    }
    root = cJSON_Parse( identity_json );
    if( root == NULL )
    {
        return;
    }

    {
        cJSON *d = cJSON_GetObjectItem( root, "d" );
        cJSON *p = cJSON_GetObjectItem( d, "p" );

        vs = cJSON_GetObjectItem( p, "vs" );
        if( !cJSON_IsObject( vs ) )
        {
            vs = cJSON_GetObjectItem( d, "vs" );
        }
    }
    if( !cJSON_IsObject( vs ) )
    {
        LogInfo( ( "[KVS] identity has no vs block - video streaming not "
                 "enabled for this device" ) );
        goto done;
    }

    carn = cJSON_GetStringValue( cJSON_GetObjectItem( vs, "carn" ) );
    url = cJSON_GetStringValue( cJSON_GetObjectItem( vs, "url" ) );
    if( ( carn == NULL ) || ( url == NULL ) )
    {
        LogWarn( ( "[KVS] vs block missing carn/url" ) );
        goto done;
    }
    s_autostart = cJSON_IsTrue( cJSON_GetObjectItem( vs, "as" ) );

    prv_free_null( &s_region );
    prv_free_null( &s_channel );
    prv_free_null( &s_endpoint );
    prv_free_null( &s_role_alias );
    prv_free_null( &s_thing );

    /* region = 4th colon field of the ARN */
    {
        const char *p = carn;
        const char *start = NULL, *end = NULL;
        int colons = 0;

        for( ; *p != '\0'; p++ )
        {
            if( *p == ':' )
            {
                colons++;
                if( colons == 3 )
                {
                    start = p + 1;
                }
                else if( colons == 4 )
                {
                    end = p;
                    break;
                }
            }
        }
        if( ( start == NULL ) || ( end == NULL ) || ( end <= start ) )
        {
            LogWarn( ( "[KVS] cannot parse region from carn: %s", carn ) );
            goto done;
        }
        s_region = prv_strdup_len( start, ( size_t ) ( end - start ) );
    }

    /* channel name = between "channel/" and next '/' */
    {
        const char *p = strstr( carn, "channel/" );

        if( p != NULL )
        {
            const char *end;

            p += 8;
            end = strchr( p, '/' );
            if( end == NULL )
            {
                end = p + strlen( p );
            }
            s_channel = prv_strdup_len( p, ( size_t ) ( end - p ) );
        }
    }

    /* endpoint = URL hostname; role alias = between /role-aliases/ and
     * /credentials */
    {
        const char *host = url;
        const char *end;

        if( 0 == strncmp( host, "https://", 8 ) )
        {
            host += 8;
        }
        end = strchr( host, '/' );
        if( end != NULL )
        {
            s_endpoint = prv_strdup_len( host, ( size_t ) ( end - host ) );
        }

        const char *ra = strstr( url, "/role-aliases/" );
        if( ra != NULL )
        {
            const char *ra_end;

            ra += 14;
            ra_end = strstr( ra, "/credentials" );
            if( ra_end == NULL )
            {
                ra_end = ra + strlen( ra );
            }
            s_role_alias = prv_strdup_len( ra, ( size_t ) ( ra_end - ra ) );
        }
    }

    /* thing name = MQTT client id */
    {
        IotclMqttConfig *mc = iotcl_mqtt_get_config();

        if( ( mc != NULL ) && ( mc->client_id != NULL ) )
        {
            s_thing = prv_strdup( mc->client_id );
        }
    }

    if( ( s_region == NULL ) || ( s_channel == NULL ) ||
        ( s_endpoint == NULL ) || ( s_role_alias == NULL ) ||
        ( s_thing == NULL ) )
    {
        LogWarn( ( "[KVS] vs parse incomplete" ) );
        goto done;
    }

    pcKvsAwsRegion = s_region;
    pcKvsChannelName = s_channel;
    pcKvsCredentialsEndpoint = s_endpoint;
    pcKvsIotRoleAlias = s_role_alias;
    pcKvsIotThingName = s_thing;

    LogInfo( ( "[KVS] config: region=%s channel=%s endpoint=%s role=%s "
             "thing=%s autostart=%d",
             s_region, s_channel, s_endpoint, s_role_alias, s_thing,
             ( int ) s_autostart ) );
    s_config_ready = true;

done:
    cJSON_Delete( root );
}

void iotc_kvs_set_credentials( const char *cert_pem, const char *key_pem )
{
    if( ( cert_pem == NULL ) || ( key_pem == NULL ) )
    {
        return;
    }
    prv_free_null( &s_cert_pem );
    prv_free_null( &s_key_pem );
    s_cert_pem = prv_strdup( cert_pem );
    s_key_pem = prv_strdup( key_pem );
    if( ( s_cert_pem != NULL ) && ( s_key_pem != NULL ) )
    {
        pcKvsIotThingCert = s_cert_pem;
        pcKvsIotPrivateKey = s_key_pem;
        s_creds_ready = true;
    }
}

bool iotc_kvs_config_ready( void )
{
    return s_config_ready;
}

const char *iotc_kvs_state( void )
{
    return s_state;
}

/* ── Transceiver init (video-only; audio returns a disabled track) ──────── */

static int32_t prv_init_transceiver( void *pvMediaCtx,
                                     TransceiverTrackKind_t kind,
                                     Transceiver_t *pxTransceiver )
{
    AppMediaSourcesContext_t *ctx = ( AppMediaSourcesContext_t * ) pvMediaCtx;

    if( ( ctx == NULL ) || ( pxTransceiver == NULL ) )
    {
        return -1;
    }
    switch( kind )
    {
        case TRANSCEIVER_TRACK_KIND_VIDEO:
            return AppMediaSource_InitVideoTransceiver( ctx, pxTransceiver );

        case TRANSCEIVER_TRACK_KIND_AUDIO:
            return AppMediaSource_InitAudioTransceiver( ctx, pxTransceiver );

        default:
            return -2;
    }
}

/* ── Media sink: encoded frames -> all connected peers ──────────────────── */

static int32_t prv_on_media_sink( void *pvCustom, MediaFrame_t *pxFrame )
{
    AppContext_t *app = ( AppContext_t * ) pvCustom;
    PeerConnectionFrame_t pc_frame;
    int i;

    if( ( app == NULL ) || ( pxFrame == NULL ) )
    {
        return -1;
    }

    pc_frame.version = PEER_CONNECTION_FRAME_CURRENT_VERSION;
    pc_frame.presentationUs = pxFrame->timestampUs;
    pc_frame.pData = pxFrame->pData;
    pc_frame.dataLength = pxFrame->size;

    for( i = 0; i < AWS_MAX_VIEWER_NUM; i++ )
    {
        AppSession_t *session = &app->appSessions[ i ];
        Transceiver_t *transceiver;

        if( pxFrame->trackKind == TRANSCEIVER_TRACK_KIND_VIDEO )
        {
            transceiver = &session->transceivers[ DEMO_TRANSCEIVER_MEDIA_INDEX_VIDEO ];
        }
        else
        {
            break; /* no audio source */
        }

        if( session->peerConnectionSession.state ==
            PEER_CONNECTION_SESSION_STATE_CONNECTION_READY )
        {
            PeerConnectionResult_t r =
                PeerConnection_WriteFrame( &session->peerConnectionSession,
                                           transceiver, &pc_frame );
            if( r != PEER_CONNECTION_RESULT_OK )
            {
                LogWarn( ( "[KVS] WriteFrame session %d: %d", i, ( int ) r ) );
            }
        }
    }
    return 0;
}

/* ── Task ───────────────────────────────────────────────────────────────── */

static void prv_kvs_task( void *pvParameters )
{
    uint32_t retry_ms = KVS_RETRY_MIN_DELAY_MS;
    bool sdk_inited = false;

    ( void ) pvParameters;
    LogInfo( ( "[KVS] task started" ) );
    s_state = "wait";

    /* Wait until the identity flow delivered the vs config, credentials are
     * set, and wall-clock time is valid (TLS + SigV4 both need it). */
    while( !s_config_ready || !s_creds_ready || !iotc_time_is_synced() )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000U ) );
    }

    LogInfo( ( "[KVS] config + credentials ready" ) );

    for( ; ; )
    {
        int rc;

        if( !sdk_inited )
        {
            memset( &s_app_ctx, 0, sizeof( s_app_ctx ) );
            memset( &s_media_ctx, 0, sizeof( s_media_ctx ) );

            rc = AppMediaSource_Init( &s_media_ctx, prv_on_media_sink,
                                      &s_app_ctx );
            if( rc != 0 )
            {
                LogError( ( "[KVS] AppMediaSource_Init failed: %d", rc ) );
                goto retry;
            }

            rc = AppCommon_Init( &s_app_ctx, prv_init_transceiver,
                                 &s_media_ctx );
            if( rc != 0 )
            {
                LogError( ( "[KVS] AppCommon_Init failed: %d", rc ) );
                goto retry;
            }
            sdk_inited = true;

            static const char master_id[] = "RA8P1-Master";
            memcpy( s_app_ctx.signalingControllerClientId, master_id,
                    sizeof( master_id ) );
            s_app_ctx.signalingControllerClientIdLength =
                sizeof( master_id ) - 1U;
            s_app_ctx.signalingControllerRole = SIGNALING_ROLE_MASTER;
        }

        LogInfo( ( "[KVS] connecting to signaling channel '%s' (%s)...",
                 pcKvsChannelName, pcKvsAwsRegion ) );
        s_state = "ready";

        /* Blocks until the controller stops (error / disconnect). */
        rc = AppCommon_StartSignalingController( &s_app_ctx );
        AppCommon_WaitSignalingControllerStop( &s_app_ctx );

        if( rc != 0 )
        {
            LogError( ( "[KVS] signaling controller exited: %d", rc ) );
        }
        else
        {
            LogInfo( ( "[KVS] signaling controller stopped" ) );
        }

retry:
        s_state = "wait";
        LogInfo( ( "[KVS] retrying in %lu ms", ( unsigned long ) retry_ms ) );
        vTaskDelay( pdMS_TO_TICKS( retry_ms ) );
        retry_ms *= 2U;
        if( retry_ms > KVS_RETRY_MAX_DELAY_MS )
        {
            retry_ms = KVS_RETRY_MAX_DELAY_MS;
        }
    }
}

void iotc_kvs_start_task( void )
{
    if( s_task != NULL )
    {
        return;
    }
    if( pdPASS != xTaskCreate( prv_kvs_task,
                               "KVSCtrl",
                               KVS_TASK_STACK_WORDS,
                               NULL,
                               KVS_TASK_PRIORITY,
                               &s_task ) )
    {
        LogError( ( "[KVS] task create failed" ) );
        s_task = NULL;
    }
}
