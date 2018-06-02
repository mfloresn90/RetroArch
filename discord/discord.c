/*  RetroArch - A frontend for libretro.
  *  Copyright (C) 2018 - Andrés Suárez
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "discord.h"

#include "../msg_hash.h"

static const char* APPLICATION_ID = "450822022025576457";
static int FrustrationLevel       = 0;
static int64_t start_time         = 0;

static bool discord_ready         = false;
static unsigned discord_status    = 0;

DiscordRichPresence discord_presence;

static void handle_discord_ready(const DiscordUser* connectedUser)
{
   RARCH_LOG("[Discord] connected to user %s#%s - %s\n",
      connectedUser->username,
      connectedUser->discriminator,
      connectedUser->userId);
}

static void handle_discord_disconnected(int errcode, const char* message)
{
   RARCH_LOG("[Discord] disconnected (%d: %s)\n", errcode, message);
}

static void handle_discord_error(int errcode, const char* message)
{
   RARCH_LOG("[Discord] error (%d: %s)\n", errcode, message);
}

static void handle_discord_join(const char* secret)
{
   RARCH_LOG("[Discord] join (%s)\n", secret);
}

static void handle_discord_spectate(const char* secret)
{
   RARCH_LOG("[Discord] spectate (%s)\n", secret);
}

static void handle_discord_join_request(const DiscordUser* request)
{
   int response = -1;
   char yn[4];
   RARCH_LOG("[Discord] join request from %s#%s - %s\n",
      request->username,
      request->discriminator,
      request->userId);
}

void discord_update(enum discord_presence presence)
{
   if (!discord_ready)
      return;
   if (
         (discord_status != DISCORD_PRESENCE_MENU) && 
         (discord_status == presence))
      return;

   RARCH_LOG("[Discord] updating (%d)\n", presence);

   memset(&discord_presence, 0, sizeof(discord_presence));

   switch (presence)
   {
      case DISCORD_PRESENCE_MENU:
         discord_presence.state           = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_presence.largeImageKey   = "icon";
         discord_presence.instance        = 0;
         discord_presence.startTimestamp  = start_time;
         break;
      case DISCORD_PRESENCE_GAME:
         start_time                       = time(0);
         discord_presence.state           = "Link's House";
         discord_presence.details         = "Legend of Zelda, The - Link's Awakening DX";
         discord_presence.largeImageKey   = "icon";
#if 0
         discord_presence.smallImageKey   = "icon";
#endif
         discord_presence.instance        = 0;
         discord_presence.startTimestamp  = start_time;
         break;
      case DISCORD_PRESENCE_NETPLAY_HOSTING:
      case DISCORD_PRESENCE_NETPLAY_CLIENT:
      case DISCORD_PRESENCE_CHEEVO_UNLOCKED:
         /* TODO/FIXME */
         break;
   }
   Discord_UpdatePresence(&discord_presence);
   discord_status                         = presence;
}

void discord_init(void)
{
   DiscordEventHandlers handlers;

   RARCH_LOG("[Discord] initializing ..\n");
   start_time            = time(0);

   memset(&handlers, 0, sizeof(handlers));
   handlers.ready        = handle_discord_ready;
   handlers.disconnected = handle_discord_disconnected;
   handlers.errored      = handle_discord_error;
   handlers.joinGame     = handle_discord_join;
   handlers.spectateGame = handle_discord_spectate;
   handlers.joinRequest  = handle_discord_join_request;

   Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);

   discord_ready         = true;
}

void discord_shutdown(void)
{
   RARCH_LOG("[Discord] shutting down ..\n");
   Discord_ClearPresence();
   Discord_Shutdown();
   discord_ready = false;
}
