/*
 * prpltwtr 
 *
 * prpltwtr is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "prpltwtr_util.h"
#include "prpltwtr_conn.h"
#include <version.h>
#include <signals.h>

gboolean twitter_usernames_match(PurpleAccount * account, const gchar * u1, const gchar * u2)
{
    gboolean        match;
    gchar          *u1n = g_strdup(purple_normalize(account, u1));
    const gchar    *u2n = purple_normalize(account, u2);
    match = !strcmp(u1n, u2n);

    g_free(u1n);
    return match;
}

//TODO: move those
char           *twitter_format_tweet(PurpleAccount * account, const char *src_user, const char *message, gchar * tweet_id, PurpleConversationType conv_type, const gchar * conv_name, gboolean is_tweet, gchar * in_reply_to_status_id, gboolean favorited)
{
    char           *linkified_message = NULL;
    GString        *tweet;
    g_return_val_if_fail(src_user != NULL, NULL);

    linkified_message = purple_signal_emit_return_1(purple_conversations_get_handle(), "prpltwtr-format-tweet", account, src_user, message, tweet_id, conv_type, conv_name, is_tweet, in_reply_to_status_id, favorited);

    if (linkified_message)
        return linkified_message;

    g_return_val_if_fail(message != NULL, NULL);

    tweet = g_string_new(message);

    if (twitter_option_add_link_to_tweet(account) && is_tweet && tweet_id) {
        PurpleConnection *gc = purple_account_get_connection(account);
        TwitterConnectionData *twitter = gc->proto_data;
        gchar          *url = twitter_mb_prefs_get_status_url(twitter->mb_prefs, src_user, tweet_id);
        if (url) {
            g_string_append_printf(tweet, "\n%s\n", url);
            g_free(url);
        }
    }

    return g_string_free(tweet, FALSE);
}

gchar          *twitter_utf8_find_last_pos(const gchar * str, const gchar * needles, glong str_len)
{
    gchar          *last;
    const gchar    *needle;
    for (last = g_utf8_offset_to_pointer(str, str_len); last; last = g_utf8_find_prev_char(str, last))
        for (needle = needles; *needle; needle++)
            if (*last == *needle) {
                return last;
            }
    return NULL;
}

char           *twitter_utf8_get_segment(const gchar * message, int max_len, const gchar * add_text, const gchar ** new_start, gboolean prepend)
{
    int             add_text_len = 0;
    int             index_add_text = -1;
    char           *status;
    int             len_left;
    int             len = 0;
    static const gchar *spaces = " \r\n";

    while (message[0] == ' ')
        message++;

    if (message[0] == '\0')
        return NULL;

    if (add_text) {
        gchar          *message_lower = g_utf8_strdown(message, -1);
        char           *pnt_add_text = strstr(message_lower, add_text);
        add_text_len = g_utf8_strlen(add_text, -1);
        if (pnt_add_text)
            index_add_text = g_utf8_pointer_to_offset(message, pnt_add_text) + add_text_len;
        g_free(message_lower);
    }

    len_left = g_utf8_strlen(message, -1);
    if (len_left <= max_len && (!add_text || index_add_text != -1)) {
        status = g_strdup(message);
        len = strlen(message);
    } else if (len_left <= max_len && len_left + add_text_len + 1 <= max_len) {
        if (prepend) {
            status = g_strdup_printf("%s %s", add_text, message);
        } else {
            status = g_strdup_printf("%s %s", message, add_text);
        }
        len = strlen(message);
    } else {
        gchar          *space;
        if (add_text && index_add_text != -1 && index_add_text <= max_len && (space = twitter_utf8_find_last_pos(message + index_add_text, spaces, max_len - g_utf8_pointer_to_offset(message, message + index_add_text)))
            && (g_utf8_pointer_to_offset(message, space) <= max_len)) {
            //split already has our word in it
            len = space - message;
            status = g_strndup(message, len);
            len++;
        } else if ((space = twitter_utf8_find_last_pos(message, spaces, max_len - (add_text ? add_text_len + 1 : 0)))) {
            len = space - message;
            space[0] = '\0';
            if (prepend) {
                status = add_text ? g_strdup_printf("%s %s", add_text, message) : g_strdup(message);
            } else {
                status = add_text ? g_strdup_printf("%s %s", message, add_text) : g_strdup(message);
            }
            space[0] = ' ';
            len++;
        } else if (index_add_text != -1 && index_add_text <= max_len) {
            //one long word, which contains our add_text
            char            prev_char;
            gchar          *end_pos;
            end_pos = g_utf8_offset_to_pointer(message, max_len);
            len = end_pos - message;
            prev_char = end_pos[0];
            end_pos[0] = '\0';
            status = g_strdup(message);
            end_pos[0] = prev_char;
        } else {
            char            prev_char;
            gchar          *end_pos;
            end_pos = (index_add_text != -1 && index_add_text <= max_len ? g_utf8_offset_to_pointer(message, max_len) : g_utf8_offset_to_pointer(message, max_len - (add_text ? add_text_len + 1 : 0)));
            end_pos = g_utf8_offset_to_pointer(message, max_len - (add_text ? add_text_len + 1 : 0));
            len = end_pos - message;
            prev_char = end_pos[0];
            end_pos[0] = '\0';
            if (prepend) {
                status = add_text ? g_strdup_printf("%s %s", add_text, message) : g_strdup(message);
            } else {
                status = add_text ? g_strdup_printf("%s %s", add_text, message) : g_strdup(message);
            }
            end_pos[0] = prev_char;
        }
    }
    if (new_start)
        *new_start = message + len;
    return g_strstrip(status);
}

GArray         *twitter_utf8_get_segments(const gchar * message, int segment_length, const gchar * add_text, gboolean prepend)
{
    GArray         *segments;
    const gchar    *new_start = NULL;
    const gchar    *pos;
    gchar          *segment = twitter_utf8_get_segment(message, segment_length, add_text, &new_start, prepend);
    segments = g_array_new(FALSE, FALSE, sizeof (char *));
    while (segment) {
        g_array_append_val(segments, segment);
        pos = new_start;
        segment = twitter_utf8_get_segment(pos, segment_length, add_text, &new_start, prepend);
    };
    return segments;
}
