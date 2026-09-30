/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Alan Korr & Nick Robinson
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* Lingo 0x02, Simple Remote Lingo
 *
 * TODO:
 * - Fix cmd 0x00 handling, there has to be a more elegant way of doing
 *   this
 */

#include "iap-core.h"
#include "iap-lingo.h"
#include "iap-remote-debug.h"
#include "kernel.h"
#include "system.h"
#include "button.h"
#include "audio.h"
#include "sound.h"
#include "settings.h"
#include "tuner.h"
#if CONFIG_TUNER
#include "ipod_remote_tuner.h"
#endif

/*
 * This macro is meant to be used inside an IAP mode message handler.
 * It is passed the expected minimum length of the message buffer.
 * If the buffer does not have the required lenght an ACK
 * packet with a Bad Parameter error is generated.
 */
#define CHECKLEN(x) do { \
        if (len < (x)) { \
            if (cmd != 0x00) cmd_ack(cmd, IAP_ACK_BAD_PARAM); \
            return; \
        }} while(0)

static unsigned char remote_tid[2];
static bool poweron_pressed;
void iap_reset_lingo2(void) { poweron_pressed = false; }

static void cmd_ack(const unsigned char cmd, const unsigned char status)
{
    IAP_TX_INIT(0x02, 0x01);
    if (device.auth.idps)
    {
        IAP_TX_PUT(remote_tid[0]); IAP_TX_PUT(remote_tid[1]);
    }
    IAP_TX_PUT(status);
    IAP_TX_PUT(cmd);

    iap_send_tx();
}

#define cmd_ok(cmd) cmd_ack((cmd), IAP_ACK_OK)

#if defined(IPOD_VIDEO) && defined(HAVE_WM8758)
static void remote_lineout_adjust_volume(int steps)
{
    int volume = global_status.volume + steps * sound_steps(SOUND_VOLUME);
    int max_volume = global_settings.volume_limit;

    if (max_volume > 0)
        max_volume = 0;

    if (volume < sound_min(SOUND_VOLUME))
        volume = sound_min(SOUND_VOLUME);
    else if (volume > max_volume)
        volume = max_volume;

    global_status.volume = volume;
    global_status.last_volume_change = current_tick;

    audiohw_set_remote_lineout_volume(volume);
}
#endif

void iap_handlepkt_mode2(const unsigned int len, const unsigned char *buf)
{
    if (!buf || len < 2) return;
#if CONFIG_TUNER
    static bool remote_mute = false;
#endif
    unsigned int cmd = buf[1];
    unsigned int doff = device.auth.idps ? 2 : 0;
    remote_tid[0] = remote_tid[1] = 0;
    if (doff && len >= 4)
    { remote_tid[0] = buf[2]; remote_tid[1] = buf[3]; }

    /* We expect at least three bytes in the buffer, one for the
     * lingo, one for the command, and one for the first button
     * state bits.
     */
    if (len < 3 + doff || (cmd == 0 && len > 6 + doff))
    {
        iap_remote_packet(buf, len, 0, 0, 'M');
        if (cmd != 0) cmd_ack(cmd, IAP_ACK_BAD_PARAM);
        return;
    }

    /* Lingo 0x02 must have been negotiated, except for
     * ContextButtonStatus (0x00): simple remotes like the Apple A1018
     * identify only once at power-up. If the remote was already
     * powered before Rockbox started (e.g. plugged in at boot) that
     * identification is never seen, and rejecting the button events
     * would leave the remote dead until it is replugged. Per MFi
     * spec Table 2-7, cmd 0x00 on UART does not require auth.
     */
    if ((cmd != 0x00) && !DEVICE_LINGO_SUPPORTED(0x02)) {
        cmd_ack(cmd, IAP_ACK_BAD_PARAM);
        return;
    }

    switch (cmd)
    {
        /* ContextButtonStatus (0x00)
         *
         * Transmit button events from the device to the iPod
         *
         * Packet format (offset in buf[]: Description)
         * 0x00: Lingo ID: Simple Remote Lingo, always 0x02
         * 0x01: Command, always 0x00
         * 0x02: Button states 0:7
         * 0x03: Button states 8:15 (optional)
         * 0x04: Button states 16:23 (optional)
         * 0x05: Button states 24:31 (optional)
         *
         * Returns: (none)
         */
        case 0x00:
        {
            unsigned long buttons = BUTTON_NONE;
            uint32_t bitmap = 0;
            for (unsigned i = 2 + doff; i < len; i++)
                bitmap |= (uint32_t)buf[i] << (8 * (i-2-doff));

            /* Kokkia reports a newly acquired Bluetooth peer as a transient
             * play-state status pulse.  Observe that edge before startup
             * quarantine discards it; waiting for remote_control_rx() loses
             * the pulse completely and Home never receives its animation
             * event.  Headset clicks use the command-button bytes instead,
             * so they do not manufacture connection notifications. */
            if (iap_kokkia_present() && len >= 4 + doff &&
                (buf[3 + doff] & (BIT_N(0) | BIT_N(1))))
                iap_note_kokkia_peer_connection();

            if (iap_remote_input_suppressed())
            {
                iap_remotebtn = BUTTON_NONE;
                iap_repeatbtn = iap_timeoutbtn = 0;
                iap_remote_packet(buf,len,bitmap,0,'Q');
                break;
            }

            /* ContextButtonStatus is one little-endian bitmap spread over
             * as many as four bytes.  Inspect every byte present: an
             * accessory may legally report bits from multiple bytes in the
             * same packet. */
            if(buf[2 + doff] != 0)
            {
                if(buf[2 + doff] & 1)
                {
                    buttons |= (BUTTON_RC_PLAY);
#if CONFIG_TUNER
                    if (radio_present == 1) {
                        if (remote_mute == 0) {
                            /* Not Muted so  radio on*/
                            tuner_set(RADIO_MUTE,0);
                        } else {
                            /* Muted so  radio off*/
                            tuner_set(RADIO_MUTE,1);
                        }
                        remote_mute = !remote_mute;
                    }
#endif
                }
                if(buf[2 + doff] & 2) {
#if defined(IPOD_VIDEO) && defined(HAVE_WM8758)
                    if (iap_remote_navigation_active())
                        buttons |= BUTTON_RC_VOL_UP;
                    else remote_lineout_adjust_volume(1);
#else
                    buttons |= (BUTTON_RC_VOL_UP);
#endif
                }
                if(buf[2 + doff] & 4) {
#if defined(IPOD_VIDEO) && defined(HAVE_WM8758)
                    if (iap_remote_navigation_active())
                        buttons |= BUTTON_RC_VOL_DOWN;
                    else remote_lineout_adjust_volume(-1);
#else
                    buttons |= (BUTTON_RC_VOL_DOWN);
#endif
                }
                if(buf[2 + doff] & 8)
                    buttons |= (BUTTON_RC_RIGHT);
                if(buf[2 + doff] & 16)
                    buttons |= (BUTTON_RC_LEFT);
            }
            if(len >= 4 + doff && buf[3 + doff] != 0)
            {
                if(buf[3 + doff] & 1) /* play */
                {
                    /* A Play state sent while playback is fully stopped is
                     * commonly an accessory startup announcement, not a
                     * click.  Posting RC_PLAY on Home maps to ACTION_STD_OK
                     * and launches Cover Flow.  Resume only an existing
                     * paused session; physical button events still arrive
                     * through byte 2 above. */
                    if ((audio_status() & (AUDIO_STATUS_PLAY |
                                           AUDIO_STATUS_PAUSE)) ==
                        (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE))
                        buttons |= (BUTTON_RC_PLAY);
#if CONFIG_TUNER
                    if (radio_present == 1) {
                        tuner_set(RADIO_MUTE,0);
                    }
#endif
                }
                if(buf[3 + doff] & 2) /* pause */
                {
                    if (audio_status() == AUDIO_STATUS_PLAY)
                        buttons |= (BUTTON_RC_PLAY);
#if CONFIG_TUNER
                    if (radio_present == 1) {
                        tuner_set(RADIO_MUTE,1);
                    }
#endif
                }
                if(buf[3 + doff] & 128) /* Shuffle */
                {
                    if (!iap_btnshuffle)
                    {
                        iap_shuffle_state(!global_settings.playlist_shuffle);
                        iap_btnshuffle = true;
                    }
                }
            }
            if(len >= 5 + doff && buf[4 + doff] != 0)
            {
                if(buf[4 + doff] & 1) /* repeat */
                {
                    if (!iap_btnrepeat)
                    {
                        iap_repeat_next();
                        iap_btnrepeat = true;
                    }
                }

                if (buf[4 + doff] & 2) /* power on */
                {
                    poweron_pressed = true;
                }

                /* Power off
                 * Not quite sure how to react to this, but stopping playback
                 * is a good start.
                 */
                if (buf[4 + doff] & 0x04)
                {
                    if (audio_status() == AUDIO_STATUS_PLAY)
                        buttons |= (BUTTON_RC_PLAY);
                }

                if(buf[4 + doff] & 16) /* ffwd */
                    buttons |= (BUTTON_RC_RIGHT);
                if(buf[4 + doff] & 32) /* frwd */
                    buttons |= (BUTTON_RC_LEFT);
                if(buf[4 + doff] & 64) /* menu */
                    buttons |= (BUTTON_RC_MENU);
                if(buf[4 + doff] & 128) /* select */
                    buttons |= (BUTTON_RC_SELECT);
            }
            if(len >= 6 + doff && buf[5 + doff] != 0)
            {
                if(buf[5 + doff] & 1) /* up */
                    buttons |= (BUTTON_RC_UP);
                if (buf[5 + doff] & 2) /* down */
                    buttons |= (BUTTON_RC_DOWN);
            }

            /* power on released */
            if (poweron_pressed && !(len >= 5 + doff && (buf[4 + doff] & 2)))
            {
                poweron_pressed = false;
#ifdef HAVE_LINE_REC
                /* Belkin TuneTalk microphone sends power-on press+release
                 * events once authentication sequence is finished,
                 * GetDevCaps command is ignored by the device when it is
                 * sent before power-on release event is received.
                 * XXX: It is unknown if other microphone devices are
                 * sending the power-on events.
                 */
                if (DEVICE_LINGO_SUPPORTED(0x01)) {
                    /* GetDevCaps */
                    IAP_TX_INIT(0x01, 0x07);
                    iap_send_tx();
                }
#endif
            }

            int level = disable_irq_save();
            char edge = buttons == iap_remotebtn ? 'H' : buttons ? 'P' : 'R';
            if (buttons != iap_remotebtn) iap_repeatbtn = 2;
            iap_remotebtn = buttons;
            iap_timeoutbtn = bitmap ? 3 : 0;
            if (!bitmap) { iap_btnshuffle = false; iap_btnrepeat = false; }
            restore_irq(level);
            iap_remote_packet(buf,len,bitmap,buttons,edge);
            break;
        }
        /* ACK (0x01)
         *
         * Sent from the iPod to the device
         */

        /* ImageButtonStatus (0x02)
         *
         * Transmit image button events from the device to the iPod
         *
         * Packet format (offset in buf[]: Description)
         * 0x00: Lingo ID: Simple Remote Lingo, always 0x02
         * 0x01: Command, always 0x02
         * 0x02: Button states 0:7
         * 0x03: Button states 8:15 (optional)
         * 0x04: Button states 16:23 (optional)
         * 0x05: Button states 24:31 (optional)
         *
         * This command requires authentication
         *
         * Returns on success:
         * IAP_ACK_OK
         *
         * Returns on failure:
         * IAP_ACK_*
         */
        case 0x02:
        {
            if (!DEVICE_AUTHENTICATED) {
                cmd_ack(cmd, IAP_ACK_NO_AUTHEN);
                break;
            }

            cmd_ack(cmd, IAP_ACK_CMD_FAILED);
            break;
        }

        /* VideoButtonStatus (0x03)
         *
         * Transmit video button events from the device to the iPod
         *
         * Packet format (offset in buf[]: Description)
         * 0x00: Lingo ID: Simple Remote Lingo, always 0x02
         * 0x01: Command, always 0x03
         * 0x02: Button states 0:7
         * 0x03: Button states 8:15 (optional)
         * 0x04: Button states 16:23 (optional)
         * 0x05: Button states 24:31 (optional)
         *
         * This command requires authentication
         *
         * Returns on success:
         * IAP_ACK_OK
         *
         * Returns on failure:
         * IAP_ACK_*
         */
        case 0x03:
        {
            if (!DEVICE_AUTHENTICATED) {
                cmd_ack(cmd, IAP_ACK_NO_AUTHEN);
                break;
            }

            cmd_ack(cmd, IAP_ACK_CMD_FAILED);
            break;
        }

        /* AudioButtonStatus (0x04)
         *
         * Transmit audio button events from the device to the iPod
         *
         * Packet format (offset in buf[]: Description)
         * 0x00: Lingo ID: Simple Remote Lingo, always 0x02
         * 0x01: Command, always 0x04
         * 0x02: Button states 0:7
         * 0x03: Button states 8:15 (optional)
         * 0x04: Button states 16:23 (optional)
         * 0x05: Button states 24:31 (optional)
         *
         * This command requires authentication
         *
         * Returns on success:
         * IAP_ACK_OK
         *
         * Returns on failure:
         * IAP_ACK_*
         */
        case 0x04:
        {
            unsigned char repeatbuf[8] = {0};

            if (!DEVICE_AUTHENTICATED) {
                cmd_ack(cmd, IAP_ACK_NO_AUTHEN);
                break;
            }

            /* This is basically the same command as ContextButtonStatus (0x00),
             * with the difference that it requires authentication and that
             * it returns an ACK packet to the device.
             * So just route it through the handler again, with 0x00 as the
             * command
             */
            memcpy(repeatbuf, buf, MIN(len, sizeof(repeatbuf)));
            repeatbuf[1] = 0x00;
            iap_handlepkt_mode2(MIN(len, sizeof(repeatbuf)), repeatbuf);

            cmd_ok(cmd);
            break;
        }

        /* The default response is IAP_ACK_BAD_PARAM */
        default:
        {
#ifdef LOGF_ENABLE
            logf("iap: Unsupported Mode02 Command");
#endif
            cmd_ack(cmd, IAP_ACK_BAD_PARAM);
            break;
        }
    }
}
