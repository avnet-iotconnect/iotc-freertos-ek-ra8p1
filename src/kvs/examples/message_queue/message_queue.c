/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include "logging.h"
#include "message_queue.h"

void MessageQueue_Destroy( MessageQueueHandler_t * pMessageQueueHandler,
                           const char * pQueueName )
{
    /* NULL-tolerant: when a session setup fails partway under low memory,
     * the teardown path destroys handlers whose queues never got created.
     * vQueueDelete(NULL) is a configASSERT (observed as a dead board). */
    if( ( pMessageQueueHandler != NULL ) &&
        ( pMessageQueueHandler->messageQueue != NULL ) )
    {
        vQueueDelete( pMessageQueueHandler->messageQueue );
        pMessageQueueHandler->messageQueue = NULL;
    }
}

MessageQueueResult_t MessageQueue_Create( MessageQueueHandler_t * pMessageQueueHandler,
                                          const char * pQueueName,
                                          size_t messageMaxLength,
                                          size_t messageQueueMaxNum )
{
    MessageQueueResult_t ret = MESSAGE_QUEUE_RESULT_OK;

    if( ( pMessageQueueHandler == NULL ) || ( pQueueName == NULL ) )
    {
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        pMessageQueueHandler->messageQueue = xQueueCreate( messageQueueMaxNum, messageMaxLength );

        if( pMessageQueueHandler->messageQueue == NULL )
        {
            LogError( ( "Fail to create message queue for %s", pQueueName ) );
            ret = MESSAGE_QUEUE_RESULT_MQ_OPEN_FAILED;
        }
        else
        {
            memset( pMessageQueueHandler->pQueueName, 0, MESSAGE_QUEUE_NAME_MAX_LENGTH + 1 );
            strncpy( pMessageQueueHandler->pQueueName, pQueueName, MESSAGE_QUEUE_NAME_MAX_LENGTH );
            pMessageQueueHandler->messageMaxLength = messageMaxLength;
            pMessageQueueHandler->messageQueueMaxNum = messageQueueMaxNum;
        }
    }

    return ret;
}

MessageQueueResult_t MessageQueue_Send( MessageQueueHandler_t * pMessageQueueHandler,
                                        void * pMessage,
                                        size_t messageLength )
{
    MessageQueueResult_t ret = MESSAGE_QUEUE_RESULT_OK;
    BaseType_t retSend;

    if( ( pMessageQueueHandler == NULL ) || ( pMessage == NULL ) )
    {
        LogError( ( "Invalid input, pMessageQueueHandler: %p, pMessage: %p",
                    pMessageQueueHandler,
                    pMessage ) );
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }
    else if( messageLength != pMessageQueueHandler->messageMaxLength )
    {
        LogError( ( "Invalid input, the input message length: %u is not messageMaxLength: %u, ",
                    messageLength,
                    pMessageQueueHandler->messageMaxLength ) );
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }
    else
    {
        /* Empty else marker. */
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        retSend = xQueueSend( pMessageQueueHandler->messageQueue, pMessage, 0 );
        if( retSend != pdTRUE )
        {
            LogError( ( "xQueueSend returns failed" ) );
            ret = MESSAGE_QUEUE_RESULT_MQ_SEND_FAILED;
        }
    }

    return ret;
}

MessageQueueResult_t MessageQueue_Recv( MessageQueueHandler_t * pMessageQueueHandler,
                                        void * pMessage,
                                        size_t * pMessageLength )
{
    MessageQueueResult_t ret = MESSAGE_QUEUE_RESULT_OK;
    BaseType_t retRecv;

    if( ( pMessageQueueHandler == NULL ) || ( pMessage == NULL ) || ( *pMessageLength < pMessageQueueHandler->messageMaxLength ) )
    {
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        /* Bounded wait (was portMAX_DELAY): the peer-connection session
         * task evaluates its inactivity timeout only between receives, so
         * an infinite wait made a vanished viewer (no DTLS close, no more
         * packets) a PERMANENT zombie session - it held ~110 KB of heap
         * and the single viewer slot forever. A 5 s timeout returns
         * MQ_RECV_FAILED (callers treat it as no-message) and lets the
         * 30 s inactivity check actually run. */
        retRecv = xQueueReceive( pMessageQueueHandler->messageQueue, pMessage,
                                 pdMS_TO_TICKS( 5000 ) );
        if( retRecv != pdTRUE )
        {
            ret = MESSAGE_QUEUE_RESULT_MQ_RECV_FAILED;
        }
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        *pMessageLength = pMessageQueueHandler->messageMaxLength;
    }

    return ret;
}

MessageQueueResult_t MessageQueue_IsEmpty( MessageQueueHandler_t * pMessageQueueHandler )
{
    MessageQueueResult_t ret = MESSAGE_QUEUE_RESULT_OK;
    UBaseType_t pendingMessageNumber;

    if( pMessageQueueHandler == NULL )
    {
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        pendingMessageNumber = uxQueueMessagesWaiting( pMessageQueueHandler->messageQueue );
        if( pendingMessageNumber == 0 )
        {
            ret = MESSAGE_QUEUE_RESULT_MQ_IS_EMPTY;
        }
        else
        {
            ret = MESSAGE_QUEUE_RESULT_MQ_HAVE_MESSAGE;
        }
    }

    return ret;
}

MessageQueueResult_t MessageQueue_IsFull( MessageQueueHandler_t * pMessageQueueHandler )
{
    MessageQueueResult_t ret = MESSAGE_QUEUE_RESULT_OK;

    if( pMessageQueueHandler == NULL )
    {
        ret = MESSAGE_QUEUE_RESULT_BAD_PARAMETER;
    }

    if( ret == MESSAGE_QUEUE_RESULT_OK )
    {
        if( uxQueueSpacesAvailable( pMessageQueueHandler->messageQueue ) == 0 )
        {
            ret = MESSAGE_QUEUE_RESULT_MQ_IS_FULL;
        }
        else
        {
            ret = MESSAGE_QUEUE_RESULT_MQ_IS_NOT_FULL;
        }
    }

    return ret;
}
