/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) TODO
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xpserver.h"

static char teamcup_score_file_name[1024];
static FILE *teamcup_score_file = NULL;

static void teamcup_open_score_file(void);
static void teamcup_close_score_file(void);

static void teamcup_check_options(void)
{
    bool ok = true;

    if (!options.teamcup)
	return;

    if (strlen(options.teamcupName) == 0) {
	ok = false;
	warn("Option teamcupName is not set.");
    }
    if (strlen(options.teamcupMailAddress) == 0) {
	ok = false;
	warn("Option teamcupMailAddress is not set.");
    }
    if (strlen(options.teamcupScoreFileNamePrefix) == 0) {
	ok = false;
	warn("Option teamcupScoreFileNamePrefix is not set.");
    }
    if (!ok) {
	warn("Teamcup related options must be set in the map file.");
	exit(1);
    }
}

void teamcup_init(void)
{
    teamcup_check_options();
}

static void teamcup_open_score_file(void)
{
    char msg[MSG_LEN];
    player_t *pl;
    int i;


    if (!options.teamcup)
	return;

/*    if (teamcup_score_file != NULL) {
	error("teamcup_score_file != NULL");
	End_game();
*/

    snprintf(teamcup_score_file_name, sizeof(teamcup_score_file_name), "%s%d",
	     options.teamcupScoreFileNamePrefix, options.teamcupMatchNumber);
    

    teamcup_score_file = Xp_fopen(teamcup_score_file_name, "w");
    if (teamcup_score_file == NULL) {
	error("fopen() failed, could not create score file");
	End_game();
    }

    snprintf(msg, sizeof(msg),
             "Score file \"%s\" opened.", teamcup_score_file_name);
    Set_message_f("%s [*Server notice*]", msg);

    warn("%s\n", msg);

    teamcup_log("1) Fill the names of the teams below.\n"
		"2) Fill the team number of total winner only if "
		"this was the second match.\n"
		"3) Send this file to %s with subject %s-RESULT.\n"
		"4) Copy this file in a safe place.  "
		"Do not delete it after sending.\n"
		"\nTeam 2 name: \n"
		"Team 4 name: \n"
		"Match: %d\n"
		"Total winner (team number): \n"
		"\nDO NOT CHANGE ANYTHING AFTER THIS LINE\n\n",
		options.teamcupMailAddress, options.teamcupName,
		options.teamcupMatchNumber
	);

    teamcup_log("Player present:\n");
    for (i = 0; i < NumPlayers; i++) {
	pl = Player_by_index(i);
        teamcup_log("Team %d: %s\n",pl->team,pl->name);
    }

}

static void teamcup_close_score_file(void)
{
    char msg[MSG_LEN];


    fclose(teamcup_score_file);
    teamcup_score_file = NULL;

    snprintf(msg, sizeof(msg),
	     "Score file \"%s\" closed.", teamcup_score_file_name);
    Set_message_f("%s [*Server notice*]", msg);
    warn("%s\n", msg);

    strcpy(teamcup_score_file_name, "");
}

void teamcup_game_start(void)
{
    teamcup_open_score_file();
    teamcup_round_start();
}

void teamcup_game_over(void)
{

    if (!options.teamcup || teamcup_score_file == NULL)
	return;

    teamcup_close_score_file();
}

void teamcup_log(const char *fmt, ...)
{
    va_list ap;

    if (!options.teamcup)
	return;

    if (teamcup_score_file == NULL)
	return;

    va_start(ap, fmt);
    vfprintf(teamcup_score_file, fmt, ap);
    va_end(ap);
}

void teamcup_round_start(void)
{
    if (!options.teamcup)
	return;

    teamcup_log("\nRound %d\n", roundsPlayed + 1);
}

void teamcup_round_end(int winning_team)
{
    int i, j, *list, best, team_players[MAX_TEAMS], best_team;
    double team_score[MAX_TEAMS];
    double best_score = -FLT_MAX;
    double best_team_score = -FLT_MAX;
    double double_max = FLT_MAX;
    player_t *pl;

    if (!options.teamcup)
	return;

    list = XMALLOC(int, NumPlayers);
    if (list == NULL) {
	error("Can't allocate memory for list");
	End_game();
    }

    for (i = 0; i < NumPlayers; i++)
	list[i] = i;

    for (i = 0; i < MAX_TEAMS; i++)
	team_score[i] = double_max;

    for (i = 0; i < MAX_TEAMS; i++)
	team_players[i] = 0;

    for (i = 0; i < NumPlayers; i++) {
	best = NumPlayers;
	for (j = 0; j < NumPlayers; j++) {
	    if (list[j] == NumPlayers)
		continue;

	    pl = Player_by_index(j);
	    if (best == NumPlayers ||  Get_Score(pl) > best_score) {
		best_score =  Get_Score(pl);
		best = j;
	    }
	}

	list[best] = NumPlayers;
	pl = Player_by_index(best);
	teamcup_log("%d\t%.0f\t%2d/%d\t%s\n", pl->team, Get_Score(pl),
		    pl->kills, pl->deaths, pl->name);

	if (team_score[pl->team] == double_max)
	    team_score[pl->team] = 0.0;
	team_score[pl->team] +=  Get_Score(pl);
	team_players[pl->team]++;
    }

    for (i = 0; i < MAX_TEAMS; i++) {
	if (team_score[i] != double_max){
	    teamcup_log("Team %d\t%d\n", i, (int)(team_score[i]));
	    if(team_score[i]>best_team_score){
	      best_team_score=team_score[i];
	      best_team=i;
	    }
        }
    }
     teamcup_log("Advantage Team %d\n",best_team);
    if (teamcup_score_file != NULL)
	fflush(teamcup_score_file);

    free(list);
}
