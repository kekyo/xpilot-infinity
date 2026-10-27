/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 1991-2001 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
 *      Dick Balaska         <dick@xpilot.org>
 *
 * Copyright (C) 2003-2004 Kristian Söderblom <kps@users.sourceforge.net>
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

#include "xpclient.h"

#define MAX_BUTTON_DEFS		10

static BITV_DECL(keyv, NUM_KEYS);
static unsigned char keyv_new[NUM_KEYS];

keys_t buttonDefs[MAX_POINTER_BUTTONS][MAX_BUTTON_DEFS+1];

char *pointerButtonBindings[MAX_POINTER_BUTTONS] =
{ NULL, NULL, NULL, NULL, NULL };

static int Key_get_count(keys_t key);
static bool Key_inc_count(keys_t key);
static bool Key_dec_count(keys_t key);

void Pointer_control_newbie_message(void)
{
    xp_option_t *opt = Find_option("keyExit");
    char msg[MSG_LEN];
    const char *val;

    if (!newbie)
	return;

    if (!opt)
	return;

    val = Option_value_to_string(opt);
    if (strlen(val) == 0)
	return;

    if (clData.pointerControl)
	snprintf(msg, sizeof(msg),
		 "Mouse steering enabled. "
		 "Key(s) to disable it: %s.", val);
    else
	snprintf(msg, sizeof(msg),
		 "Mouse steering disabled. "
#ifdef __EMSCRIPTEN__
		 "Select Mouse steering to enable it."
#else
		 "Click background with left mouse button to enable it."
#endif
		 );

    Add_newbie_message(msg);
}

void Pointer_control_set_state(bool on)
{
    if (clData.pointerControl == on)
	return;
    Platform_specific_pointer_control_set_state(on);
    clData.pointerControl = on;
    if (!clData.restorePointerControl)
	Pointer_control_newbie_message();
}

void Talk_set_state(bool on)
{
    if (clData.talking == on)
	return;
    if (on) {
	/* When enabling talking, disable pointer control if it is enabled. */
	if (clData.pointerControl) {
	    clData.restorePointerControl = true;
	    Pointer_control_set_state(false);
	}
    }
    Platform_specific_talk_set_state(on);
    if (!on) {
	/* When disabling talking, enable pointer control if it was enabled. */
	if (clData.restorePointerControl) {
	    Pointer_control_set_state(true);
	    clData.restorePointerControl = false;
	}
    }
    clData.talking = on;
}

static inline int pointer_button_index_by_option(xp_option_t *opt)
{
    return atoi(Option_get_name(opt) + strlen("pointerButton")) - 1;
}

static int numButtonDefs[MAX_POINTER_BUTTONS] = { 0, 0, 0, 0, 0 };

static int Num_buttonDefs(int ind)
{
    assert(ind >= 0);
    assert(ind < MAX_POINTER_BUTTONS);
    return numButtonDefs[ind];
}

static void Clear_buttonDefs(int ind)
{
    assert(ind >= 0);
    assert(ind < MAX_POINTER_BUTTONS);
    numButtonDefs[ind] = 0;
}



int Key_init(void)
{
    int i;

    if (sizeof(keyv) != KEYBOARD_SIZE) {
	warn("%s, %d: keyv size %d, KEYBOARD_SIZE is %d",
	     __FILE__, __LINE__,
	     sizeof(keyv), KEYBOARD_SIZE);
	exit(1);
    }
    memset(keyv, 0, sizeof keyv);
    for (i = 0; i < NUM_KEYS; i++)
    	keyv_new[i] = 0;
    
    BITV_SET(keyv, KEY_SHIELD);

    return 0;
}

int Key_update(void)
{
    return Send_keyboard(keyv);
}

static bool Key_check_talk_macro(keys_t key)
{
    if (key >= KEY_MSG_1 && key < KEY_MSG_1 + TALK_FAST_NR_OF_MSGS)
	Talk_macro((int)(key - KEY_MSG_1));
    return true;
}


static bool Key_press_id_mode(void)
{
    showUserName = showUserName ? false : true;
    scoresChanged = true;
    return false;	/* server doesn't need to know */
}

static bool Key_press_autoshield_hack(void)
{
    if (auto_shield && BITV_ISSET(keyv, KEY_SHIELD))
	BITV_CLR(keyv, KEY_SHIELD);
    return false;
}

static bool Key_press_shield(keys_t key)
{
    if (toggle_shield) {
	shields = !shields;
	if (shields)
	    BITV_SET(keyv, key);
	else
	    BITV_CLR(keyv, key);
	return true;
    } else if (auto_shield)
	shields = true;

    return false;
}

static bool Key_press_fuel(void)
{
    fuelTime = FUEL_NOTIFY_TIME;
    return false;
}

static bool Key_press_swap_settings(void)
{
    double tmp;
#define SWAP(a, b) (tmp = (a), (a) = (b), (b) = tmp)

    SWAP(power, power_s);
    SWAP(turnspeed, turnspeed_s);
    SWAP(turnresistance, turnresistance_s);
    controlTime = CONTROL_TIME;
    Config_redraw();

    return true;
}

static bool Key_press_swap_scalefactor(void)
{
    double a = clData.altScaleFactor;

    Set_altScaleFactor(NULL, clData.scaleFactor);
    Set_scaleFactor(NULL, a);

    return false;
}

static bool Key_press_increase_power(void)
{
    power = power * 1.10;
    power = MIN(power, MAX_PLAYER_POWER);
    Send_power(power);

    Config_redraw();
    controlTime = CONTROL_TIME;
    return false;	/* server doesn't see these keypresses anymore */

}

static bool Key_press_decrease_power(void)
{
    power = power / 1.10;
    power = MAX(power, MIN_PLAYER_POWER);
    Send_power(power);

    Config_redraw();
    controlTime = CONTROL_TIME;
    return false;	/* server doesn't see these keypresses anymore */
}

static bool Key_press_increase_turnspeed(void)
{
    turnspeed = turnspeed * 1.05;
    turnspeed = MIN(turnspeed, MAX_PLAYER_TURNSPEED);
    Send_turnspeed(turnspeed);

    Config_redraw();
    controlTime = CONTROL_TIME;
    return false;	/* server doesn't see these keypresses anymore */
}

static bool Key_press_decrease_turnspeed(void)
{
    turnspeed = turnspeed / 1.05;
    turnspeed = MAX(turnspeed, MIN_PLAYER_TURNSPEED);
    Send_turnspeed(turnspeed);

    Config_redraw();
    controlTime = CONTROL_TIME;
    return false;	/* server doesn't see these keypresses anymore */
}

static bool Key_press_talk(void)
{
    int i;

    /*
     * this releases mouse in x11 client, so we clear the mouse buttons
     * so they don't lock on
     */
    if (clData.pointerControl)
	for (i = 0; i < MAX_POINTER_BUTTONS; i++)
	    Pointer_button_released(i);

    Talk_set_state(!clData.talking);
    return false;	/* server doesn't need to know */
}

static bool Key_press_show_items(void)
{
    instruments.showItems = !instruments.showItems;
    return false;	/* server doesn't need to know */
}

static bool Key_press_show_messages(void)
{
    instruments.showMessages = !instruments.showMessages;
    return false;	/* server doesn't need to know */
}

static bool Key_press_pointer_control(void)
{
    Pointer_control_set_state(!clData.pointerControl);
    return false;	/* server doesn't need to know */
}

static bool Key_press_toggle_fullscreen(void)
{
    Toggle_fullscreen();
    return false;	/* server doesn't need to know */
}

static bool Key_press_toggle_radar_score(void)
{
    Toggle_radar_and_scorelist();
    return false;	/* server doesn't need to know */
}

static bool Key_press_toggle_record(void)
{
    Record_toggle();
    return false;	/* server doesn't need to know */
}

static bool Key_press_toggle_sound(void)
{
#ifdef SOUND
    sound = !sound;
#endif
    return false;	/* server doesn't need to know */
}

static bool Key_press_msgs_stdout(void)
{
    Print_messages_to_stdout();
    return false;	/* server doesn't need to know */
}

static bool Key_press_select_lose_item(void)
{
    if (lose_item_active == 1)
	lose_item_active = 2;
    else
	lose_item_active = 1;
    return true;
}

static bool Key_press_yes(void)
{
    /* Handled in other code */
    assert(!clData.quitMode);

    return false;	/* server doesn't need to know */
}

static bool Key_press_no(void)
{
    return false;	/* server doesn't need to know */
}

static bool Key_press_exit(void)
{
    int i;

    /* exit pointer control if exit key pressed in pointer control mode */
    if (clData.pointerControl) {
	/*
	 * this releases mouse, so we clear the mouse buttons so
	 * they don't lock on
	 */
	for (i = 0; i < MAX_POINTER_BUTTONS; i++)
	    Pointer_button_released(i);

	Pointer_control_set_state(false);
	return false;	/* server doesn't need to know */
    }

    clData.quitMode = true;
    Add_alert_message("Really Quit (y/n) ?", 0.0);

    return false;	/* server doesn't need to know */
}

static int Key_get_count(keys_t key)
{
   if (key >= NUM_KEYS)
       return -1;

   return keyv_new[key];
}

static bool Key_inc_count(keys_t key)
{
    if (key >= NUM_KEYS)
	return false;

    if (keyv_new[key] < 255) {
    	++keyv_new[key];
	return true;
    }

    return false;
}

static bool Key_dec_count(keys_t key)
{
    if (key >= NUM_KEYS)
	return false;

    if (keyv_new[key] > 0) {
    	--keyv_new[key];
	return true;
    }

    return false;
}

void Key_clear_counts(void)
{
    int i;
    bool change = false;

    for (i = 0; i < NUM_KEYS; i++) {
    	if (keyv_new[i] > 0) {
	    /* set to one so that Key_release(i) will trigger */
    	    keyv_new[i] = 1;
	    change |= Key_release((keys_t)i);
	}
    }
    
    if (change)
	Net_key_change();
}

int Key_held_count(void)
{
    int count = 0;
    for (int i = 0; i < NUM_KEYS; i++) count += keyv_new[i] != 0;
    return count;
}

/* Remember which key we used to exit quit mode. */
static keys_t quit_mode_exit_key = KEY_DUMMY;

static bool Quit_mode_key_press(keys_t key)
{
    if (key == KEY_YES)
	Client_exit(0);

    if (key == KEY_NO || key == KEY_EXIT) {
	clData.quitMode = false;
	Clear_alert_messages();
	quit_mode_exit_key = key;
    }
	
    return false;
}

bool Key_press(keys_t key)
{
    bool countchange;
    int keycount, i;

    if (clData.quitMode)
	return Quit_mode_key_press(key);

    countchange = Key_inc_count(key);
    keycount = Key_get_count(key);
    
    /*
     * keycount -1 means this was a client only key, we don't count those
     */
    if (keycount != -1) { 
	/*
	 * if countchange is false it means that Key_<inc|dec>_count()
	 * failed to change the count due to being at the end of the range
	 * (should be very rare) keycount != 1 means that this key was
	 * already pressed (multiple key mappings)
	 */
	if ((!countchange) || (keycount != 1))
	    return true;
    }

    Key_check_talk_macro(key);

    switch (key) {
    case KEY_ID_MODE:
	return Key_press_id_mode();

    case KEY_FIRE_SHOT:
    case KEY_FIRE_LASER:
    case KEY_FIRE_MISSILE:
    case KEY_FIRE_TORPEDO:
    case KEY_FIRE_HEAT:
    case KEY_DROP_MINE:
    case KEY_DETACH_MINE:
	Key_press_autoshield_hack();
	break;

    case KEY_SHIELD:
	if (Key_press_shield(key))
	    return true;
	break;

    case KEY_REFUEL:
    case KEY_REPAIR:
    case KEY_TANK_NEXT:
    case KEY_TANK_PREV:
	Key_press_fuel();
	break;

    case KEY_SWAP_SETTINGS:
	if (!Key_press_swap_settings())
	    return false;
	break;

    case KEY_SWAP_SCALEFACTOR:
	if (!Key_press_swap_scalefactor())
	    return false;
	break;

    case KEY_INCREASE_POWER:
	return Key_press_increase_power();

    case KEY_DECREASE_POWER:
	return Key_press_decrease_power();

    case KEY_INCREASE_TURNSPEED:
	return Key_press_increase_turnspeed();

    case KEY_DECREASE_TURNSPEED:
	return Key_press_decrease_turnspeed();

    case KEY_TALK:
	return Key_press_talk();

    case KEY_TOGGLE_OWNED_ITEMS:
	return Key_press_show_items();

    case KEY_TOGGLE_MESSAGES:
	return Key_press_show_messages();

    case KEY_POINTER_CONTROL:
    	/*
	 * this releases mouse, so we clear the mouse buttons so they
	 * don't lock on
	 */
    	if (clData.pointerControl)
    	    for (i = 0; i < MAX_POINTER_BUTTONS; i++)
    	    	Pointer_button_released(i);

	return Key_press_pointer_control();

    case KEY_TOGGLE_RECORD:
	return Key_press_toggle_record();

    case KEY_TOGGLE_SOUND:
	return Key_press_toggle_sound();

    case KEY_TOGGLE_RADAR_SCORE:
	return Key_press_toggle_radar_score();

    case KEY_PRINT_MSGS_STDOUT:
	return Key_press_msgs_stdout();

    case KEY_TOGGLE_FULLSCREEN:
	return Key_press_toggle_fullscreen();

    case KEY_SELECT_ITEM:
    case KEY_LOSE_ITEM:
	if (!Key_press_select_lose_item())
	    return false;
	break;

    case KEY_EXIT:
	return Key_press_exit();
    case KEY_YES:
	return Key_press_yes();
    case KEY_NO:
	return Key_press_no();

    default:
	break;
    }

    if (key < NUM_KEYS)
	BITV_SET(keyv, key);

    return true;
}

bool Key_release(keys_t key)
{
    bool countchange;
    int keycount;

    /*
     * Make sure nothing is done when we release the button we used
     * to exit quit mode with.
     */
    if (key == quit_mode_exit_key) {
	assert(key != KEY_DUMMY);
	quit_mode_exit_key = KEY_DUMMY;
	return false;
    }

    countchange = Key_dec_count(key);
    keycount = Key_get_count(key);

    /* -1 means this was a client only key, we don't count those */
    if (keycount != -1) {
	/*
	 * if countchange is false it means that Key_<inc|dec>_count()
	 * failed to change the count due to being at the end of the range
	 * (happens to most key releases let through from talk mode)
	 * keycount != 0 means that some physical keys remain pressed
	 * that map to this xpilot key 
	 */
	if ((!countchange) || (keycount != 0))
	    return true;
    }


    switch (key) {
    case KEY_ID_MODE:
    case KEY_TALK:
    case KEY_TOGGLE_OWNED_ITEMS:
    case KEY_TOGGLE_MESSAGES:
	return false;	/* server doesn't need to know */

    /* Don auto-shield hack */
    /* restore shields */
    case KEY_FIRE_SHOT:
    case KEY_FIRE_LASER:
    case KEY_FIRE_MISSILE:
    case KEY_FIRE_TORPEDO:
    case KEY_FIRE_HEAT:
    case KEY_DROP_MINE:
    case KEY_DETACH_MINE:
	if (auto_shield && shields && !BITV_ISSET(keyv, KEY_SHIELD)) {
	    /* Here We need to know if any other weapons are still on */
	    /*      before we turn shield back on   */
	    BITV_CLR(keyv, key);
	    if (!BITV_ISSET(keyv, KEY_FIRE_SHOT) &&
		!BITV_ISSET(keyv, KEY_FIRE_LASER) &&
		!BITV_ISSET(keyv, KEY_FIRE_MISSILE) &&
		!BITV_ISSET(keyv, KEY_FIRE_TORPEDO) &&
		!BITV_ISSET(keyv, KEY_FIRE_HEAT) &&
		!BITV_ISSET(keyv, KEY_DROP_MINE) &&
		!BITV_ISSET(keyv, KEY_DETACH_MINE))
		BITV_SET(keyv, KEY_SHIELD);
	}
	break;

    case KEY_SHIELD:
	if (toggle_shield)
	    return false;
	else if (auto_shield)
	    shields = false;
	break;

    case KEY_REFUEL:
    case KEY_REPAIR:
	fuelTime = FUEL_NOTIFY_TIME;
	break;

    case KEY_SELECT_ITEM:
    case KEY_LOSE_ITEM:
	if (lose_item_active == 2)
	    lose_item_active = 1;
	else
	    lose_item_active = -(int)(clientFPS + 0.5);
	break;

    default:
	break;
    }
    if (key < NUM_KEYS)
	BITV_CLR(keyv, key);

    return true;
}

void Reset_shields(void)
{
    if (toggle_shield || auto_shield) {
	BITV_SET(keyv, KEY_SHIELD);
	shields = true;
	if (auto_shield) {
	    if (BITV_ISSET(keyv, KEY_FIRE_SHOT) ||
		BITV_ISSET(keyv, KEY_FIRE_LASER) ||
		BITV_ISSET(keyv, KEY_FIRE_MISSILE) ||
		BITV_ISSET(keyv, KEY_FIRE_TORPEDO) ||
		BITV_ISSET(keyv, KEY_FIRE_HEAT) ||
		BITV_ISSET(keyv, KEY_DROP_MINE) ||
		BITV_ISSET(keyv, KEY_DETACH_MINE))
		BITV_CLR(keyv, KEY_SHIELD);
	}
	Net_key_change();
    }
}

void Set_auto_shield(bool on)
{
    auto_shield = on;
}

void Set_toggle_shield(bool on)
{
    toggle_shield = on;
    if (toggle_shield) {
	if (auto_shield)
	    shields = true;
	else
	    shields = (BITV_ISSET(keyv, KEY_SHIELD)) ? true : false;
    }
}

/*
 * Function to call when a button of a pointing device has been pressed.
 * Argument 'button' should be 1 for the first pointer button, etc.
 */
void Pointer_button_pressed(int button)
{
    int i, b_index = button - 1;
    bool key_change = false;

    if (button < 1 || button > MAX_POINTER_BUTTONS)
	return;

    for (i = 0; i < Num_buttonDefs(b_index); i++)
	key_change |= Key_press(buttonDefs[b_index][i]);

    if (key_change)
	Net_key_change();
}

void Pointer_button_released(int button)
{
    int i, b_index = button - 1;
    bool key_change = false;

    if (button < 1 || button > MAX_POINTER_BUTTONS)
	return;

    for (i = 0; i < Num_buttonDefs(b_index); i++)
	key_change |= Key_release(buttonDefs[b_index][i]);

    if (key_change)
	Net_key_change();
}


void Keyboard_button_pressed(xp_keysym_t ks)
{
    bool change = false;
    keys_t key;

#if 0
    {
	char foo[80];

	sprintf(foo, "keysym = %d (0x%x) []", (int)ks, (int)ks);
	Add_message(foo);
    }
#endif

    for (key = Generic_lookup_key(ks, true);
	 key != KEY_DUMMY;
	 key = Generic_lookup_key(ks, false))
	change |= Key_press(key);

    if (change)
	Net_key_change();
}
void Keyboard_button_released(xp_keysym_t ks)
{
    bool change = false;
    keys_t key;

    for (key = Generic_lookup_key(ks, true);
	 key != KEY_DUMMY;
	 key = Generic_lookup_key(ks, false))
	change |= Key_release(key);

    if (change)
	Net_key_change();
}



static void Bind_key_to_pointer_button(keys_t key, int ind)
{
    int num_defs;

    assert(ind >= 0);
    assert(ind < MAX_POINTER_BUTTONS);
    assert(key != KEY_DUMMY);

    num_defs = Num_buttonDefs(ind);
    if (num_defs == MAX_BUTTON_DEFS) {
	warn("Can only have %d keys bound to pointerButton%d.",
	     MAX_BUTTON_DEFS, ind + 1);
	return;
    }

    buttonDefs[ind][num_defs] = key;
    numButtonDefs[ind]++;
}



static bool setPointerButtonBinding(xp_option_t *opt, const char *value)
{
    int ind = pointer_button_index_by_option(opt);
    char *ptr, *valcpy;
    int j;

    assert(ind >= 0);
    assert(ind < MAX_POINTER_BUTTONS);
    assert(value);
    XFREE(pointerButtonBindings[ind]);

    Clear_buttonDefs(ind);

    pointerButtonBindings[ind] = xp_safe_strdup(value);
    valcpy = xp_safe_strdup(value);

    for (ptr = strtok(valcpy, " \t\r\n");
	 ptr != NULL;
	 ptr = strtok(NULL, " \t\r\n")) {
	if (!strncasecmp(ptr, "key", 3))
	    ptr += 3;
	for (j = 0; j < num_options; j++) {
	    xp_option_t *opt_j = Option_by_index(j);
	    const char *opt_j_name;
	    keys_t opt_j_key;

	    assert(opt_j);
	    opt_j_name = Option_get_name(opt_j);
	    opt_j_key = Option_get_key(opt_j);
	    if (opt_j_key != KEY_DUMMY
		&& (!strcasecmp(ptr, opt_j_name + 3))) {
		Bind_key_to_pointer_button(opt_j_key, ind);
		break;
	    }
	}
	if (j == num_options)
	    warn("Unknown key \"%s\" for %s.", ptr, Option_get_name(opt));
    }

    XFREE(valcpy);

    return true;
}

static const char *getPointerButtonBinding(xp_option_t *opt)
{
    int ind = pointer_button_index_by_option(opt);

    assert(ind >= 0);
    assert(ind < MAX_POINTER_BUTTONS);

    return pointerButtonBindings[ind];
}



/*
 * Generic key options.
 */
xp_option_t key_options[] = {
    XP_KEY_OPTION(
	"keyTurnLeft",
	"a",
	KEY_TURN_LEFT,
	"Turn left (anti-clockwise).\n"),

    XP_KEY_OPTION(
	"keyTurnRight",
	"s",
	KEY_TURN_RIGHT,
	"Turn right (clockwise).\n"),

    XP_KEY_OPTION(
	"keyThrust",
	"Shift_R Shift_L",
	KEY_THRUST,
	"Thrust.\n"),

    XP_KEY_OPTION(
	"keyShield",
	"space",
	KEY_SHIELD,
	"Raise or toggle the shield.\n"),

    XP_KEY_OPTION(
	"keyFireShot",
	"Return Linefeed",
	KEY_FIRE_SHOT,
	"Fire shot.\n"
	"Note that shields must be down to fire.\n"),

    XP_KEY_OPTION(
	"keyFireMissile",
	"backslash",
	KEY_FIRE_MISSILE,
	"Fire smart missile.\n"),

    XP_KEY_OPTION(
	"keyFireTorpedo",
	"quoteright",
	KEY_FIRE_TORPEDO,
	"Fire unguided torpedo.\n"),

    XP_KEY_OPTION(
	"keyFireHeat",
	"semicolon",
	KEY_FIRE_HEAT,
	"Fire heatseeking missile.\n"),

    XP_KEY_OPTION(
	"keyFireLaser",
	"slash",
	KEY_FIRE_LASER,
	"Activate laser beam.\n"),

    XP_KEY_OPTION(
	"keyDropMine",
	"Tab",
	KEY_DROP_MINE,
	"Drop a stationary mine.\n"),

    XP_KEY_OPTION(
	"keyDetachMine",
	"bracketright",
	KEY_DETACH_MINE,
	"Detach a moving mine.\n"),

    XP_KEY_OPTION(
	"keyDetonateMines",
	"equal",
	KEY_DETONATE_MINES,
	"Detonate the closest mine you have dropped or thrown.\n"),

    XP_KEY_OPTION(
	"keyLockClose",
	"Up",
	KEY_LOCK_CLOSE,
	"Lock on closest player.\n"),

    XP_KEY_OPTION(
	"keyLockNextClose",
	"Down",
	KEY_LOCK_NEXT_CLOSE,
	"Lock on next closest player.\n"),

    XP_KEY_OPTION(
	"keyLockNext",
	"Next Right",		/* Next is a.k.a. Page Down */
	KEY_LOCK_NEXT,
	"Lock on next player.\n"),

    XP_KEY_OPTION(
	"keyLockPrev",
	"Prior Right",		/* Prior is a.k.a. Page Up */
	KEY_LOCK_PREV,
	"Lock on previous player.\n"),

    XP_KEY_OPTION(
	"keyRefuel",
	"f Control_L Control_R",
	KEY_REFUEL,
	"Refuel.\n"),

    XP_KEY_OPTION(
	"keyRepair",
	"f",
	KEY_REPAIR,
	"Repair target.\n"),

    XP_KEY_OPTION(
	"keyCloak",
	"Delete BackSpace",
	KEY_CLOAK,
	"Toggle cloakdevice.\n"),

    XP_KEY_OPTION(
	"keyEcm",
	"bracketleft",
	KEY_ECM,
	"Use ECM.\n"),

    XP_KEY_OPTION(
	"keySelfDestruct",
	"End",
	KEY_SELF_DESTRUCT,
	"Toggle self destruct.\n"),

    XP_KEY_OPTION(
	"keyIdMode",
	"u",
	KEY_ID_MODE,
	"Toggle User mode (show real names).\n"),

    XP_KEY_OPTION(
	"keyPause",
	"Pause",
	KEY_PAUSE,
	"Toggle pause mode.\n"
	"When the ship is stationary on its homebase.\n"),

    XP_KEY_OPTION(
	"keySwapSettings",
	"",
	KEY_SWAP_SETTINGS,
	"Swap to alternate control settings.\n"
	"These are the power, turn speed and turn resistance settings.\n"),

    XP_KEY_OPTION(
	"keySwapScaleFactor",
	"",
	KEY_SWAP_SCALEFACTOR,
	"Swap scalefactor settings.\n"),

    XP_KEY_OPTION(
	"keyChangeHome",
	"Home h",
	KEY_CHANGE_HOME,
	"Change home base.\n"
	"When the ship is close to a suitable base.\n"),

    XP_KEY_OPTION(
	"keyConnector",
	"f Control_L Control_R",
	KEY_CONNECTOR,
	"Connect to a ball.\n"),

    XP_KEY_OPTION(
	"keyDropBall",
	"d",
	KEY_DROP_BALL,
	"Drop a ball.\n"),

    XP_KEY_OPTION(
	"keyTankNext",
	"e",
	KEY_TANK_NEXT,
	"Use the next tank.\n"),

    XP_KEY_OPTION(
	"keyTankPrev",
	"w",
	KEY_TANK_PREV,
	"Use the the previous tank.\n"),

    XP_KEY_OPTION(
	"keyTankDetach",
	"r",
	KEY_TANK_DETACH,
	"Detach the current tank.\n"),

    XP_KEY_OPTION(
	"keyIncreasePower",
	"KP_Multiply",
	KEY_INCREASE_POWER,
	"Increase engine power.\n"),

    XP_KEY_OPTION(
	"keyDecreasePower",
	"KP_Divide",
	KEY_DECREASE_POWER,
	"Decrease engine power.\n"),

    XP_KEY_OPTION(
	"keyIncreaseTurnspeed",
	"KP_Add",
	KEY_INCREASE_TURNSPEED,
	"Increase turnspeed.\n"),

    XP_KEY_OPTION(
	"keyDecreaseTurnspeed",
	"KP_Subtract",
	KEY_DECREASE_TURNSPEED,
	"Decrease turnspeed.\n"),

    XP_KEY_OPTION(
	"keyTransporter",
	"t",
	KEY_TRANSPORTER,
	"Use transporter to steal an item.\n"),

    XP_KEY_OPTION(
	"keyDeflector",
	"o",
	KEY_DEFLECTOR,
	"Toggle deflector.\n"),

    XP_KEY_OPTION(
	"keyHyperJump",
	"q",
	KEY_HYPERJUMP,
	"Teleport.\n"),

    XP_KEY_OPTION(
	"keyPhasing",
	"p",
	KEY_PHASING,
	"Use phasing device.\n"),

    XP_KEY_OPTION(
	"keyTalk",
	"m",
	KEY_TALK,
	"Toggle talk window.\n"),

    XP_KEY_OPTION(
	"keyToggleNuclear",
	"n",
	KEY_TOGGLE_NUCLEAR,
	"Toggle nuclear weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleCluster",
	"c",
	KEY_TOGGLE_CLUSTER,
	"Toggle cluster weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleImplosion",
	"i",
	KEY_TOGGLE_IMPLOSION,
	"Toggle implosion weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleVelocity",
	"v",
	KEY_TOGGLE_VELOCITY,
	"Toggle explosion velocity weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleMini",
	"x",
	KEY_TOGGLE_MINI,
	"Toggle mini weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleSpread",
	"z",
	KEY_TOGGLE_SPREAD,
	"Toggle spread weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyTogglePower",
	"b",
	KEY_TOGGLE_POWER,
	"Toggle power weapon modifier.\n"),

    XP_KEY_OPTION(
	"keyToggleCompass",
	"KP_7",
	KEY_TOGGLE_COMPASS,
	"Toggle HUD/radar compass lock.\n"),

    XP_KEY_OPTION(
	"keyToggleAutoPilot",
	"h",
	KEY_TOGGLE_AUTOPILOT,
	"Toggle automatic pilot mode.\n"),

    XP_KEY_OPTION(
	"keyToggleLaser",
	"l",
	KEY_TOGGLE_LASER,
	"Toggle laser modifier.\n"),

    XP_KEY_OPTION(
	"keyEmergencyThrust",
	"j",
	KEY_EMERGENCY_THRUST,
	"Pull emergency thrust handle.\n"),

    XP_KEY_OPTION(
	"keyEmergencyShield",
	"Caps_Lock",
	KEY_EMERGENCY_SHIELD,
	"Toggle emergency shield power.\n"),

    XP_KEY_OPTION(
	"keyTractorBeam",
	"comma",
	KEY_TRACTOR_BEAM,
	"Use tractor beam in attract mode.\n"),

    XP_KEY_OPTION(
	"keyPressorBeam",
	"period",
	KEY_PRESSOR_BEAM,
	"Use tractor beam in repulse mode.\n"),

    XP_KEY_OPTION(
	"keyClearModifiers",
	"k",
	KEY_CLEAR_MODIFIERS,
	"Clear current weapon modifiers.\n"),

    XP_KEY_OPTION(
	"keyLoadModifiers1",
	"1",
	KEY_LOAD_MODIFIERS_1,
	"Load the weapon modifiers from bank 1.\n"),

    XP_KEY_OPTION(
	"keyLoadModifiers2",
	"2",
	KEY_LOAD_MODIFIERS_2,
	"Load the weapon modifiers from bank 2.\n"),

    XP_KEY_OPTION(
	"keyLoadModifiers3",
	"3",
	KEY_LOAD_MODIFIERS_3,
	"Load the weapon modifiers from bank 3.\n"),

    XP_KEY_OPTION(
	"keyLoadModifiers4",
	"4",
	KEY_LOAD_MODIFIERS_4,
	"Load the weapon modifiers from bank 4.\n"),

    XP_KEY_OPTION(
	"keyToggleOwnedItems",
	"KP_8",
	KEY_TOGGLE_OWNED_ITEMS,
	"Toggle list of owned items on HUD.\n"),

    XP_KEY_OPTION(
	"keyToggleMessages",
	"KP_9",
	KEY_TOGGLE_MESSAGES,
	"Toggle showing of messages.\n"),

    XP_KEY_OPTION(
	"keyReprogram",
	"quoteleft",
	KEY_REPROGRAM,
	"Reprogram modifier or lock bank.\n"),

    XP_KEY_OPTION(
	"keyLoadLock1",
	"5",
	KEY_LOAD_LOCK_1,
	"Load player lock from bank 1.\n"),

    XP_KEY_OPTION(
	"keyLoadLock2",
	"6",
	KEY_LOAD_LOCK_2,
	"Load player lock from bank 2.\n"),

    XP_KEY_OPTION(
	"keyLoadLock3",
	"7",
	KEY_LOAD_LOCK_3,
	"Load player lock from bank 3.\n"),

    XP_KEY_OPTION(
	"keyLoadLock4",
	"8",
	KEY_LOAD_LOCK_4,
	"Load player lock from bank 4.\n"),

    XP_KEY_OPTION(
	"keyToggleRecord",
	"KP_5",
	KEY_TOGGLE_RECORD,
	"Toggle recording of session (see recordFile).\n"),

#ifdef SOUND
    XP_KEY_OPTION(
	"keyToggleSound",
	"",
	KEY_TOGGLE_SOUND,
	"Toggle sound. Changes value of option 'sound'.\n"),
#endif

    XP_KEY_OPTION(
	"keyToggleRadarScore",
	"F11",
	KEY_TOGGLE_RADAR_SCORE,
	"Toggles the radar and score windows on and off.\n"),

    XP_KEY_OPTION(
	"keyToggleFullScreen",
	"F11",
	KEY_TOGGLE_FULLSCREEN,
	"Toggles between fullscreen mode and window mode.\n"),

    XP_KEY_OPTION(
	"keySelectItem",
	"KP_0 KP_Insert",
	KEY_SELECT_ITEM,
	"Select an item to lose.\n"),

    XP_KEY_OPTION(
	"keyLoseItem",
	"KP_Delete KP_Decimal",
	KEY_LOSE_ITEM,
	"Lose the selected item.\n"),

    XP_KEY_OPTION(
	"keyPrintMessagesStdout",
	"Print",
	KEY_PRINT_MSGS_STDOUT,
	"Print the current messages to stdout.\n"),

    XP_KEY_OPTION(
	"keyTalkCursorLeft",
	"Left",
	KEY_TALK_CURSOR_LEFT,
	"Move Cursor to the left in the talk window.\n"),

    XP_KEY_OPTION(
	"keyTalkCursorRight",
	"Right",
	KEY_TALK_CURSOR_RIGHT,
	"Move Cursor to the right in the talk window.\n"),

    XP_KEY_OPTION(
	"keyTalkCursorUp",
	"Up",
	KEY_TALK_CURSOR_UP,
	"Browsing in the history of the talk window.\n"),

    XP_KEY_OPTION(
	"keyTalkCursorDown",
	"Down",
	KEY_TALK_CURSOR_DOWN,
	"Browsing in the history of the talk window.\n"),

    XP_KEY_OPTION(
	"keyPointerControl",
	"KP_Enter",
	KEY_POINTER_CONTROL,
	"Toggle pointer control.\n"),

    XP_KEY_OPTION(
	"keyExit",
	"Escape",
	KEY_EXIT,
	"Generic exit key.\n"
	"Used for example to exit mouse mode or quit the client.\n"),

    XP_KEY_OPTION(
	"keyYes",
	"y",
	KEY_YES,
	"Positive reply key.\n"
	"Used to reply 'yes' to client questions.\n"),

    XP_KEY_OPTION(
	"keyNo",
	"n",
	KEY_NO,
	"Negative reply key.\n"
	"Used to reply 'no' to client questions.\n"),

    XP_KEY_OPTION(
	"keySendMsg1",
	"F1",
	KEY_MSG_1,
	"Sends the talkmessage stored in msg1.\n"),

    XP_KEY_OPTION(
	"keySendMsg2",
	"F2",
	KEY_MSG_2,
	"Sends the talkmessage stored in msg2.\n"),

    XP_KEY_OPTION(
	"keySendMsg3",
	"F3",
	KEY_MSG_3,
	"Sends the talkmessage stored in msg3.\n"),

    XP_KEY_OPTION(
	"keySendMsg4",
	"F4",
	KEY_MSG_4,
	"Sends the talkmessage stored in msg4.\n"),

    XP_KEY_OPTION(
	"keySendMsg5",
	"F5",
	KEY_MSG_5,
	"Sends the talkmessage stored in msg5.\n"),

    XP_KEY_OPTION(
	"keySendMsg6",
	"F6",
	KEY_MSG_6,
	"Sends the talkmessage stored in msg6.\n"),

    XP_KEY_OPTION(
	"keySendMsg7",
	"F7",
	KEY_MSG_7,
	"Sends the talkmessage stored in msg7.\n"),

    XP_KEY_OPTION(
	"keySendMsg8",
	"F8",
	KEY_MSG_8,
	"Sends the talkmessage stored in msg8.\n"),

    XP_KEY_OPTION(
	"keySendMsg9",
	"F9",
	KEY_MSG_9,
	"Sends the talkmessage stored in msg9.\n"),

    XP_KEY_OPTION(
	"keySendMsg10",
	"F10",
	KEY_MSG_10,
	"Sends the talkmessage stored in msg10.\n"),

    XP_KEY_OPTION(
	"keySendMsg11",
	"", /* F11 is keyToggleFullScreen now */
	KEY_MSG_11,
	"Sends the talkmessage stored in msg11.\n"),

    XP_KEY_OPTION(
	"keySendMsg12",
	"",
	KEY_MSG_12,
	"Sends the talkmessage stored in msg12.\n"),

    XP_KEY_OPTION(
	"keySendMsg13",
	"",
	KEY_MSG_13,
	"Sends the talkmessage stored in msg13.\n"),

    XP_KEY_OPTION(
	"keySendMsg14",
	"",
	KEY_MSG_14,
	"Sends the talkmessage stored in msg14.\n"),

    XP_KEY_OPTION(
	"keySendMsg15",
	"",
	KEY_MSG_15,
	"Sends the talkmessage stored in msg15.\n"),

    XP_KEY_OPTION(
	"keySendMsg16",
	"",
	KEY_MSG_16,
	"Sends the talkmessage stored in msg16.\n"),

    XP_KEY_OPTION(
	"keySendMsg17",
	"",
	KEY_MSG_17,
	"Sends the talkmessage stored in msg17.\n"),

    XP_KEY_OPTION(
	"keySendMsg18",
	"",
	KEY_MSG_18,
	"Sends the talkmessage stored in msg18.\n"),

    XP_KEY_OPTION(
	"keySendMsg19",
	"",
	KEY_MSG_19,
	"Sends the talkmessage stored in msg19.\n"),

    XP_KEY_OPTION(
	"keySendMsg20",
	"",
	KEY_MSG_20,
	"Sends the talkmessage stored in msg20.\n"),

    /*
     * These are after the keys so that the key options will be
     * present when the pointer button options are stored.
     */
    XP_STRING_OPTION(
	"pointerButton1",
	"keyFireShot",
	NULL, 0,
	setPointerButtonBinding, NULL, getPointerButtonBinding,
	XP_OPTFLAG_DEFAULT,
	"The keys to activate when pressing the first mouse button.\n"),

    XP_STRING_OPTION(
	"pointerButton2",
	"keyThrust",
	NULL, 0,
	setPointerButtonBinding, NULL, getPointerButtonBinding,
	XP_OPTFLAG_DEFAULT,
	"The keys to activate when pressing the second mouse button.\n"),

    XP_STRING_OPTION(
	"pointerButton3",
	"keyThrust",
	NULL, 0,
	setPointerButtonBinding, NULL, getPointerButtonBinding,
	XP_OPTFLAG_DEFAULT,
	"The keys to activate when pressing the third mouse button.\n"),

    XP_STRING_OPTION(
	"pointerButton4",
	"",
	NULL, 0,
	setPointerButtonBinding, NULL, getPointerButtonBinding,
	XP_OPTFLAG_DEFAULT,
	"The keys to activate when pressing the fourth mouse button.\n"),

    XP_STRING_OPTION(
	"pointerButton5",
	"",
	NULL, 0,
	setPointerButtonBinding, NULL, getPointerButtonBinding,
	XP_OPTFLAG_DEFAULT,
	"The keys to activate when pressing the fifth mouse button.\n"),
};

void Store_key_options(void)
{
    STORE_OPTIONS(key_options);
}
