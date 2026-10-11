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

/* Robot code originally submitted by Maurice Abraham. */
/* Rewrite started by Karsten Siegmund - in progress */
#include "xpserver.h"

#define ROB_LOOK_AH		2

#define WITHIN(NOW,THEN,DIFF) (NOW<=THEN && (THEN-NOW)<DIFF)

/*
 * Flags for the suibot (well, default...) robots being in different modes (or moods).
 */
#define RM_ROBOT_IDLE         	(1 << 2)
#define RM_EVADE_LEFT         	(1 << 3)
#define RM_EVADE_RIGHT          (1 << 4)
#define RM_ROBOT_CLIMB          (1 << 5)
#define RM_HARVEST            	(1 << 6)
#define RM_ATTACK             	(1 << 7)
#define RM_TAKE_OFF           	(1 << 8)
#define RM_CANNON_KILL		(1 << 9)
#define RM_REFUEL		(1 << 10)
#define RM_NAVIGATE		(1 << 11)

/* long term modes */
#define FETCH_TREASURE		(1 << 0)
#define TARGET_KILL		(1 << 1)
#define NEED_FUEL		(1 << 2)

/*
 * Prototypes for methods of the suibot.
 */
static void Robot_suibot_round_tick(void);
static void Robot_suibot_create(player_t *pl, char *str);
static void Robot_suibot_go_home(player_t *pl);
static void Robot_suibot_play(player_t *pl);
static void Robot_suibot_set_war(player_t *pl, int victim_id);
static int Robot_suibot_war_on_player(player_t *pl);
static void Robot_suibot_message(player_t *pl, const char *str);
static void Robot_suibot_destroy(player_t *pl);
static void Robot_suibot_invite(player_t *pl, player_t *inviter);
       int Robot_suibot_setup(robot_type_t *type_ptr);




/*
 * The robot type structure for the suibot - contains the functions 
 * that will be called from robot.c
 */
static robot_type_t robot_suibot_type = {
    "suibot",
    Robot_suibot_round_tick,
    Robot_suibot_create,
    Robot_suibot_go_home,
    Robot_suibot_play,
    Robot_suibot_set_war,
    Robot_suibot_war_on_player,
    Robot_suibot_message,
    Robot_suibot_destroy,
    Robot_suibot_invite
};


/*
 * Local static variables
 */
static double	Visibility_distance;
static double	Max_enemy_distance;

/*
 * The only thing we export from this file.
 * A function to initialize the robot type structure
 * with our name and the pointers to our action routines.
 *
 * Return 0 if all is OK, anything else will ignore this
 * robot type forever.
 */
int Robot_suibot_setup(robot_type_t *type_ptr)
{
    /* Just init the type structure. */

    *type_ptr = robot_suibot_type;

    return 0;
}

/*
 * Private functions.
 */
static bool Check_robot_evade(player_t *pl, int mine_i, int ship_i);
static bool Detect_ship(player_t *pl, player_t *ship);
static int Rank_item_value(player_t *pl, enum Item itemtype);
static bool Ball_handler(player_t *pl);
static void Robot_move_randomly(player_t *pl);


/*
 * Function to cast from player structure to robot data structure.
 * This isolates casts (aka. type violations) to a few places.
 */
static robot_default_data_t *Robot_suibot_get_data(player_t *pl)
{
    return (robot_default_data_t *)pl->robot_data_ptr->private_data;
}

/*
 * Create the suibot.
 */
static void Robot_suibot_create(player_t *pl, char *str)
{
    robot_default_data_t *my_data;

    if (!(my_data = XMALLOC(robot_default_data_t, 1))) {
	error("no mem for default robot");
	End_game();
    }

    pl->turnspeed = 0; /* we play with "mouse" */
    pl->turnresistance = 0;

    my_data->robot_mode      = RM_TAKE_OFF;
    my_data->robot_count     = 0;
    my_data->robot_lock      = LOCK_NONE;
    my_data->robot_lock_id   = 0;

    my_data->longterm_mode	= 0;

    pl->robot_data_ptr->private_data = (void *)my_data;
}

/*
 * A suibot is placed on its homebase.
 */
static void Robot_suibot_go_home(player_t *pl)
{
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);

    my_data->robot_mode      = RM_TAKE_OFF;
    my_data->longterm_mode   = 0;
}

/*
 * A default robot is declaring war (or resetting war).
 */
static void Robot_suibot_set_war(player_t *pl, int victim_id)
{
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);

    if (victim_id == NO_ID)
	CLR_BIT(my_data->robot_lock, LOCK_PLAYER);
    else {
	my_data->robot_lock_id = victim_id;
	SET_BIT(my_data->robot_lock, LOCK_PLAYER);
    }
}

/*
 * Return the id of the player a default robot has war against (or NO_ID).
 */
static int Robot_suibot_war_on_player(player_t *pl)
{
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);

    if (BIT(my_data->robot_lock, LOCK_PLAYER))
	return my_data->robot_lock_id;
    else
	return NO_ID;
}

/*
 * A default robot receives a message.
 */
static void Robot_suibot_message(player_t *pl, const char *message)
{
    UNUSED_PARAM(pl); UNUSED_PARAM(message);
}

/*
 * A default robot is destroyed.
 */
static void Robot_suibot_destroy(player_t *pl)
{
    XFREE(pl->robot_data_ptr->private_data);
}

/*
 * A default robot is asked to join an alliance
 */
static void Robot_suibot_invite(player_t *pl, player_t *inviter)
{
    int war_id = Robot_suibot_war_on_player(pl), i;
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);
    double limit;
    bool we_accept = true;

    if (pl->alliance != ALLIANCE_NOT_SET) {
	/* if there is a human in our alliance, they should decide
	   let robots refuse in this case */
	for (i = 0; i < NumPlayers; i++) {
	    player_t *pl_i = Player_by_index(i);

	    if (Player_is_human(pl_i) && Players_are_allies(pl, pl_i)) {
		we_accept = false;
		break;
	    }
	}
	if (!we_accept) {
	    Refuse_alliance(pl, inviter);
	    return;
	}
    }
    limit = MAX(ABS( Get_Score(pl) / MAX((my_data->attack / 10), 10)),
		my_data->defense);
    if (inviter->alliance == ALLIANCE_NOT_SET) {
	/* don't accept players we are at war with */
	if (inviter->id == war_id)
	    we_accept = false;
	/* don't accept players who are not active */
	if (!Player_is_active(inviter))
	    we_accept = false;
	/* don't accept players with scores substantially lower than ours */
	else if ( Get_Score(inviter) < ( Get_Score(pl) - limit))
	    we_accept = false;
    }
    else {
	double avg_score = 0;
	int member_count = Get_alliance_member_count(inviter->alliance);

	for (i = 0; i < NumPlayers; i++) {
	    player_t *pl_i = Player_by_index(i);
	    if (pl_i->alliance == inviter->alliance) {
		if (pl_i->id == war_id) {
		    we_accept = false;
		    break;
		}
		avg_score +=  Get_Score(pl_i);
	    }
	}
	if (we_accept) {
	    avg_score = avg_score / member_count;
	    if (avg_score < ( Get_Score(pl) - limit))
		we_accept = false;
	}
    }
    if (we_accept)
	Accept_alliance(pl, inviter);
    else
	Refuse_alliance(pl, inviter);
}

static inline int decide_travel_dir(player_t *pl)
{
    double gdir;

    if (pl->velocity <= 0.2) {
	vector_t grav = World_gravity(pl->pos);

	gdir = findDir(grav.x, grav.y);
    } else
	gdir = findDir(pl->vel.x, pl->vel.y);

    return MOD2((int) (gdir + 0.5), RES);
}


static void Robot_take_off_from_base(player_t *pl);

static void Robot_take_off_from_base(player_t *pl)
{ 
  robot_default_data_t  *my_data = Robot_suibot_get_data(pl);
  /*Function that could do specific things when robot takes off*/  
  
  Robot_move_randomly(pl);
  Thrust(pl, true);


  if((rfrac())<0.1 ){
    my_data->robot_mode = RM_ATTACK;
  }
}


/*KS: let robot "play mouse" */
static void Robot_set_pointing_direction(player_t *pl,double direction);

static void Robot_set_pointing_direction(player_t *pl,double direction)
{ 
  robot_default_data_t	*my_data = Robot_suibot_get_data(pl);
  int turnvel;     

    if(pl->turnspeed != 0 | pl->turnresistance != 0) End_game();


   turnvel = (direction - pl->float_dir);
   pl->turnvel = turnvel;
}

/*KS: describe func here.....*/
/* void Obj_pos_set(object *obj, int cx, int cy); */

/* void Obj_pos_set(object *obj, int cx, int cy){ */
/*     struct _objposition		*pos = (struct _objposition *)&obj->pos; */



/*     pos->cx = cx; */
/*     pos->x = CLICK_TO_PIXEL(cx); */
/*     pos->bx = pos->x / BLOCK_SZ; */
/*     pos->cy = cy; */
/*     pos->y = CLICK_TO_PIXEL(cy); */
/*     pos->by = pos->y / BLOCK_SZ; */
/*     obj->prevpos.x = obj->pos.x; */
/*     obj->prevpos.y = obj->pos.y; */

/* } */

static bool Wall_in_between_points(int cx1, int cy1, int cx2, int cy2);
static bool Wall_in_between_points(int cx1, int cy1, int cx2, int cy2){ /* Wall between two given points?*/
  
  struct collans answer;
  move_t mv;
  mv.delta.cx = WRAP_DCX(cx2 - cx1);
  mv.delta.cy = WRAP_DCY(cy2 - cy1);
  mv.start.cx = WRAP_XCLICK(cx1);
  mv.start.cy = WRAP_YCLICK(cy1);
  mv.obj   = NULL;
  mv.hitmask  = NONBALL_BIT;

  while (mv.delta.cx || mv.delta.cy) {
    Move_point(&mv, &answer);
    if (answer.line != -1)
      return true;
    mv.start.cx = WRAP_XCLICK(mv.start.cx + answer.moved.cx);
    mv.start.cy = WRAP_YCLICK(mv.start.cy + answer.moved.cy);
    mv.delta.cx -= answer.moved.cx;
    mv.delta.cy -= answer.moved.cy;
  }
  return false;

}


bool Robot_evade_shot(player_t *pl);

typedef struct {
  double hit_time;
  double sqdistance;
} object_proximity_t;


static bool Get_object_proximity(player_t *pl, object_t *shot, double sqmaxdist,int maxtime, object_proximity_t *object_proximity){
   /* get square of closest distance between player and object
    * compare with sqmaxdist and maxtime and return sqdistance and time
    * if both are smaller than the maximal wanted values 
    */
   double delta_velx, delta_vely, delta_x, delta_y, sqdistance;
   double time_until_closest, shortest_hit_time;

  /* calculate relative positions and velocities */
  delta_velx=( shot->vel.x -  pl->vel.x );
  delta_vely=( shot->vel.y -  pl->vel.y );
  delta_x=   WRAP_DCX( shot->pos.cx - pl->pos.cx );
  delta_y=   WRAP_DCY( shot->pos.cy - pl->pos.cy );

  /* prevent possible division by 0 */
  if(delta_velx == 0 && delta_vely == 0)
    return false;

  /* get time of encounter from deviation of distance function */
  time_until_closest =
    -( delta_x * delta_velx + delta_y * delta_vely) /
    ((sqr(delta_velx) + sqr(delta_vely)));

  /* ignore if there is enough time to deal with this object  later */
  if((time_until_closest < 0) || (time_until_closest > maxtime))
    /*option instead of fixed value: options.dodgetime))*/
    return false;

  /* get the square of the distance */
  sqdistance =
    (sqr(delta_velx) + sqr(delta_vely)) * sqr(time_until_closest)  +
    2 * (delta_velx * delta_x + delta_vely * delta_y) * time_until_closest +
    sqr(delta_x) + sqr(delta_y);
  
  if(sqdistance>sqmaxdist)
    return false;

  object_proximity->hit_time=time_until_closest;
  object_proximity->sqdistance=sqdistance;

  return true;

}


bool Robot_evade_shot(player_t *pl){

/*  change to use   struct dangerous_shot_data *shotsarray; */
  int j;
  object_t *shot, **obj_list;
  int  obj_count;
  long killing_shots;
  //  player_t *opponent;
  
      killing_shots = KILLING_SHOTS;
    if (options.treasureCollisionMayKill)
        killing_shots |= OBJ_BALL;
    if (options.wreckageCollisionMayKill)
        killing_shots |= OBJ_WRECKAGE;
    if (options.asteroidCollisionMayKill)
        killing_shots |= OBJ_ASTEROID;
    if (!options.allowShields)
	killing_shots |=  OBJ_PLAYER;

    robot_default_data_t *my_data = Robot_suibot_get_data(pl);

    const int                   max_objs = 1000;
    int shot_dist;
    double time_shot_closest, shortest_hit_time;
    double delta_velx, delta_vely, delta_x, delta_y, sqdistance;
    double sqship_sz;
    Cell_get_objects(pl->pos, (int)(Visibility_distance / BLOCK_SZ),
		     max_objs, &obj_list, &obj_count); 

    /*This is the viewable area for players:
    #include "connection.h"
    int hori_blocks, vert_blocks;
    hori_blocks = (view_width + (BLOCK_SZ - 1)) / (2 * BLOCK_SZ);
    vert_blocks = (view_height + (BLOCK_SZ - 1)) / (2 * BLOCK_SZ);
    if (NumObjs >= options.cellGetObjectsThreshold)
        Cell_get_objects(pl->pos, MAX(hori_blocks, vert_blocks),
                         num_object_shuffle, &obj_list, &obj_count);*/

    

    shortest_hit_time=10000;
    int closest_shot = -1;
    int dangerous_shots[obj_count];
    double dangerous_shots_time[obj_count];
    int dangerous_shots_ind =0;
    double dangerous_shots_distance[obj_count];

    for (j = 0; j < obj_count; j++) { /*for .. obj_count*/
	
	shot = obj_list[j];
	
	/* Get rid of most objects */
	if (!BIT(shot->type, killing_shots ))
	    continue;
	
	/* calculate relative positions and velocities */
	delta_velx=( shot->vel.x -  pl->vel.x );
	delta_vely=( shot->vel.y -  pl->vel.y );
	delta_x=   WRAP_DCX( shot->pos.cx - pl->pos.cx );
	delta_y=   WRAP_DCY( shot->pos.cy - pl->pos.cy );
	
	/* prevent possible division by 0 */
	if(delta_velx == 0 && delta_vely == 0) 
	    continue;
	
	/* get time of encounter from deviation of distance function */
	time_shot_closest = 
	    -( delta_x * delta_velx + delta_y * delta_vely) /
	    ((sqr(delta_velx) + sqr(delta_vely))); 
	
	/* ignore if there is enough time to dodge this shot in a later frame*/
	if((time_shot_closest < 0) || (time_shot_closest > 1000))
	    /*option instead of fixed value: options.dodgetime))*/
	    continue;
	
	/* look if shot will hit (compare squares of distances and shipsize) */
	sqdistance = 
	    (sqr(delta_velx) + sqr(delta_vely)) * sqr(time_shot_closest)  
	    + 2 * (delta_velx * delta_x + delta_vely * delta_y) 
	    * time_shot_closest 
	    + sqr(delta_x) + sqr(delta_y);
	
#define SQ_SHIP_SZ_11 sqr(1.1 * PIXEL_TO_CLICK(SHIP_SZ))
#define SQ_SHIP_SZ_25 sqr(2.5 * PIXEL_TO_CLICK(SHIP_SZ))
	
	if(sqdistance > SQ_SHIP_SZ_25)
	    continue;
	
	/* ignore shots that will hit a wall before it hits us */
	if(Wall_in_between_points(
	       shot->pos.cx ,
	       shot->pos.cy ,
	       shot->pos.cx + time_shot_closest * shot->vel.x,
	       shot->pos.cy + time_shot_closest * shot->vel.y
	       ))
	    continue;
	
	/* 
	 * store shot id and time for every shot that hits 
	 * within the given time 
	 */
	
	dangerous_shots[dangerous_shots_ind]=j;
	dangerous_shots_time[dangerous_shots_ind]=time_shot_closest;
        dangerous_shots_distance[dangerous_shots_ind]=sqdistance;
	dangerous_shots_ind++;
	
	if(sqdistance > SQ_SHIP_SZ_11)
	    continue;
	
	if(shortest_hit_time > time_shot_closest){
	    shortest_hit_time = time_shot_closest;
	    closest_shot=j;
	}
    }
    
    /* return, if nothing will hit */
    if(closest_shot == -1) {return false; }
    
//printf("Time for closest shot (%i): %.2f\n",closest_shot,shortest_hit_time);
//for (j = 0; j < dangerous_shots_ind  ; j++) { /*for .. dangerous_shots_ind*/
//printf("dangerous shot %i (%i) impact in %.2f, distance: %.2f\n",j,
//dangerous_shots[j],dangerous_shots_time[j],
 //sqrt(dangerous_shots_distance[j]));
 //}

    /* get vector from center of ship to shot at hit time */
    /* this is orthogonal to delta_vel(x,y) */
    shot = obj_list[closest_shot];
    delta_velx=( shot->vel.x -  pl->vel.x );
    delta_vely=( shot->vel.y -  pl->vel.y );
    delta_x=   WRAP_DCX( shot->pos.cx - pl->pos.cx );
    delta_y=   WRAP_DCY( shot->pos.cy - pl->pos.cy );


double hit_dx, hit_dy;
hit_dx = delta_x + shortest_hit_time * delta_velx;
hit_dy = delta_y + shortest_hit_time * delta_vely;
double evade_x=-hit_dx;
double evade_y=-hit_dy;
double direction_evade1;

/*printf("delta_x: %.2f delta_y: %.2f evade_x: %.2f evade_y: %.2f product: %.2f\n",
	delta_velx, delta_vely, evade_x, evade_y, delta_velx*evade_x+delta_vely*evade_y);*/
//XXX
//printf("direction hit: %.2f  --- ",direction_evade1);
    
    /* if the shot hits the exactly the center, use alternate calculation of orthogonal to shot path */
/*    double direction_pl,direction_evade1,direction_evade2;
    double norm_vel= sqrt(sqr(delta_velx)+sqr(delta_vely));
    double norm_xy = sqrt(sqr(delta_x)+sqr(delta_y));
    double evade_x = -(delta_velx / norm_vel  + delta_x / norm_xy);
    double evade_y = -(delta_vely / norm_vel  + delta_y / norm_xy);
*/
    direction_evade1 = Wrap_findDir(evade_x, evade_y );

    /* Change evade by 180° if wall will be in the way in the chosen direction */
    if(Wall_in_between_points(
                  pl->pos.cx + time_shot_closest * (pl->vel.x +  pl->power * cos(direction_evade1) / pl->mass),
                  pl->pos.cy + time_shot_closest * (pl->vel.y +  pl->power * sin(direction_evade1) / pl->mass),
                  pl->pos.cx ,
                  pl->pos.cy 
                  )){
	direction_evade1+=RES;
	//	printf("Wall!\n");
	}


//printf("direction old: %.2f\n",direction_evade1);

  Robot_set_pointing_direction(pl, direction_evade1);
    Thrust(pl, true);

    return true;

}


void Robot_move_randomly(player_t *pl){
  double direction;

  /* Move randomly */
  if(rfrac()<0.25) 
    pl->turnvel = ((rfrac()*RES)- RES / 2)*0.3;

  if(pl->velocity > options.maxUnshieldedWallBounceSpeed){ /* not too fast...*/
                            
    direction= findDir(-pl->vel.x,-pl->vel.y);
    Robot_set_pointing_direction(pl, direction);
    Thrust(pl, true);
    return;
  }
  
  /* Fire, too */
  if((rfrac())>0.98 ){
        Fire_normal_shots(pl);  
  }
  /* Sometimes thrust */
  if((rfrac())>0.65 )
    {  
      Thrust(pl, true); 
    } 
  else{
    Thrust(pl, false);
    }
  
} 

double Robot_ram_object(player_t *pl,object_t *object){

    double direction;
    int x,y,x_tgo, y_tgo;
    double velx, vely; /* relative positions and velocities */
    double time, delta_time;
    double sqr_a, sqr_b, sqr_c, b_dot_c, function, deviation;
    int i=0;
    int j=0;

    velx = ( object->vel.x -  pl->vel.x ) * CLICK;
    vely = ( object->vel.y -  pl->vel.y ) * CLICK;
    /* multiply with CLICK to get clicks/time, but keep as float */
    x    = WRAP_DCX( object->pos.cx - pl->pos.cx );
    y    = WRAP_DCY( object->pos.cy - pl->pos.cy );
    



#define DD false /* debug */
    
    /* use squares of length, so sqrt doesnt need to be calculated */
    sqr_a   = sqr(pl->power / pl->mass * CLICK);       /* acceleration */
    sqr_b   = sqr(velx)+sqr(vely);  /* velocity     */
    sqr_c   = sqr((double)x)+sqr((double)y); /* distance */
    b_dot_c = velx*x + vely*y;      /* dot product b·c */

/*     for (i = 0; i < 500; i++){ */
/*       time=i/10.0; */
/*       double tmp_x = (int)(x + velx * time); */
/*       double tmp_y = (int)(y + vely * time); */
      
/*       double tmp_length= */
/* 	abs(LENGTH(tmp_x,tmp_y)- 0.5 * (CLICK * pl->power / pl->mass) * sqr(time)); */
/*       if(tmp_length < 3000) */
/* 	printf("time %.2f  length %.2f\n",time,tmp_length); */
/*     } */

    time = 1;
    //time=sqrt(2*LENGTH(x,y)/(pl->power/pl->mass));
    /* exact: time=sqrt(2*LENGTH(x,y)/pl->power*pl->mass);*/
    delta_time=5000; /* set high value so convercence criterion isnt met */
    
    if(DD){printf("time %.3f\n",time);}

    for (i = 0; i < 30; i++) { /* allow only limited amount of iterations */

	/* Newton iterations to get the time of impact */
	function = 
	    -0.25 * sqr_a * sqr(sqr(time)) 
	    + sqr_b * sqr(time)
	    + 2 * b_dot_c * time 
	    + sqr_c;
	deviation=
	    -sqr_a * sqr(time) * time 
	    + 2 * sqr_b * time 
	    + 2 * b_dot_c;

	delta_time=function/deviation;	

        if(delta_time > time){
	  if(j>2)
	    break; /* setting time=0 failed, give up */
	  time*=0;
	  j++;
	}else{
	  time-=delta_time;
	}

	
	if(DD)
	    printf("time %.3f function: %e delta_time %E\n",time, function, delta_time);
	
	
	 int tmp_x = (int)(x + velx * time);
	 int tmp_y = (int)(y + vely * time);


//	if( abs(delta_time)< 0.001* abs(time))
      if(fabs(function) < 10)
//	if(abs(LENGTH(tmp_x,tmp_y)- 0.5 * (CLICK * pl->power/pl->mass) * sqr(time)) < 10 )
	    break;
    }
    
    if(DD){
	printf("time: %.2f\n",time);
	printf("test1: %.4f %.4f %.4f\n",
	       - 0.25 * sqr_a * sqr(sqr(time)),
	       + sqr_b * sqr(time)
	       + 2 * b_dot_c * time
	       + sqr_c,
	       -sqr_a/4 * sqr(sqr(time)) 
	       + sqr_b * sqr(time)
	       + 2 * b_dot_c * time 
	       + sqr_c
	    );
    }
/*

    velx=( object->vel.x -  pl->vel.x );
    vely=( object->vel.y -  pl->vel.y );
    x=   WRAP_DCX( object->pos.cx - pl->pos.cx );
    y=   WRAP_DCY( object->pos.cy - pl->pos.cy );
*/
    /* prevent possible division by 0 */
/*    if(velx == 0 || vely == 0) 
	time =0;
    else 
	time = 
	    -( x * velx + y * vely) /
	    ((sqr(velx) + sqr(vely))); 
*/    
    if(DD){
    printf("x %i y %.i\n",x,y);
    printf("velx %.2f * time %.2f = %.2f\n",velx,time,velx*time);
    }


    x_tgo = (int)(x + velx * time);
    y_tgo = (int)(y + vely * time);

    if(DD) {
	printf("nx %i ny %i, length %.3f\n",x_tgo,y_tgo,LENGTH(x_tgo,y_tgo));
	printf("            difference: %.3f\n",LENGTH(x_tgo,y_tgo)- 0.5 * sqrt(sqr_a) * sqr(time));

    }

    /*
     * if vector to ball at time-to-go (tgo) points away from the ball
     * something is wrong - better use the vector of current LOS (line of sight)
     */    
//    if(sqr_c > sqr(x+x_tgo)+sqr(y+y_tgo)){
//	x_tgo=x;
//	y_tgo=y;
//	}
	

    direction=(Wrap_cfindDir(x_tgo,y_tgo));

    if(DD)
    printf("                         direction %.2f, direction %i\n",direction, (int)direction);

    Robot_set_pointing_direction(pl, direction);
    Thrust(pl, true);
    return time;
}


#define NO_DIR -1
void Robot_find_shooting_dir(player_t *pl, player_t *pl_to_suicide);
void Robot_find_shooting_dir(player_t *pl, player_t *pl_to_suicide){



}

void Robot_attack_player(player_t *pl, player_t *opponent);
void Robot_attack_player(player_t *pl, player_t *opponent){/*attack_player*/   
    
    int        dcx,dcy;
    double     direction;
    double     velx,vely;
    double     tmp_a,tmp_b,tmp_c,t1,t,t2;
    
    /* check if we can fire or have to wait because of repeat rate */
    /* same check as in Fire_normal_shots(pl)                      */
    if (frame_time 
	<= pl->shot_time + options.fireRepeatRate 
	- timeStep + 1e-3){
	Robot_ram_object(pl, OBJ_PTR(opponent));
	return;
    }
    
    
    dcx   = WRAP_DCX(opponent->pos.cx - pl->pos.cx);
    dcy   = WRAP_DCY(opponent->pos.cy - pl->pos.cy);
    velx  = (opponent->vel.x - pl->vel.x) * CLICK;
    vely  = (opponent->vel.y - pl->vel.y) * CLICK;

    
    /*Find direction, where a shot will hit a ship with constant velocity*/
    /* use tmp_vars to try to keep it readable */
    tmp_a = dcx * velx + dcy * vely;
    
    tmp_b = sqr(velx) + sqr(vely) - sqr(options.shotSpeed * CLICK);
    
    tmp_c = sqr(tmp_a) - tmp_b * (sqr(dcx) + sqr(dcy));
    
	t  = -1; /* -1 for no solution */
	
	if( tmp_c >= 0) { /* square-root only if number positive*/
	
	tmp_c = sqrt(tmp_c);
	
	t1 = (-tmp_a - tmp_c) / tmp_b;
	t2 = (-tmp_a + tmp_c) / tmp_b;

	
	/* t (=time) must be greater than 0, but as small as possible... 
	   if problem can't be solved call ram_object*/
	
	if (t1 >= 0 && t2 >= 0) { 
	    if (t1 > t2){ t = t2;} else {t = t1;}
	    Fire_normal_shots(pl);
	}
	else if( t2 >= 0 ) {t = t2; Fire_normal_shots(pl);} 
	else if( t1 >= 0 ) {t = t1; Fire_normal_shots(pl);}
	}
    
	/* t = -1 for no solution */    
	if(t<0){
	    /* try to get closer */
	    Robot_ram_object(pl, OBJ_PTR(opponent));
	    
	    if(rfrac() >0.5){ Fire_normal_shots(pl);}
	    return;
	}
	
#define D2 false
    if(D2)
    printf("no:  dcx %i = dcx %i + t %.2f * velx %.2f\n", 
	   (int)(dcx + t * velx), dcx, t, velx);
    
    dcx = (int) (dcx + t * velx);
    dcy = (int) (dcy + t * vely);
    
    /* slightly bias shooting towads where a player thrusts */
    
    if(false && rfrac() > 0.75){
	dcx = dcx + opponent->acc.x * t * sqr(sqr(rfrac()));
	dcy = dcy + opponent->acc.y * t * sqr(sqr(rfrac()));
	
	/* actually, it should be acc * sqr(time); 
	 * but this is much too much for even relatively
	 * small times 
	 */
    }
    
    direction = findDir((double)dcx,(double)dcy);
    if(D2)
      printf("no: dir= %g\n",direction);
    
    if(rfrac() > 0.8) /* spread shots */
      direction += ((rfrac()-0.5 )*10);
    
        Robot_set_pointing_direction(pl, direction);
        Thrust(pl, true); 
}



/*
 * Calculate minimum of length of hypotenuse in triangle with sides
 * 'dcx' and 'dcy' and 'min', taking into account wrapping.
 * Unit is clicks.
 */
static inline double Wrap_length_min(double dcx, double dcy, double min)
{
    double len;

    dcx = WRAP_DCX(dcx), dcx = ABS(dcx);
    if (dcx >= min)
	return min;
    dcy = WRAP_DCY(dcy), dcy = ABS(dcy);
    if (dcy >= min)
	return min;

    len = LENGTH(dcx, dcy);

    return MIN(len, min);
}


static void Robotdef_fire_laser(player_t *pl)
{
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);
    double x2, y2, x3, y3, x4, y4, x5, y5;
    double ship_dist, dir3, dir4, dir5;
    clpos_t m_gun;
    player_t *ship;

    if (BIT(my_data->robot_lock, LOCK_PLAYER)
	&& Player_is_active(Player_by_id(my_data->robot_lock_id)))
	ship = Player_by_id(my_data->robot_lock_id);
    else if (BIT(pl->lock.tagged, LOCK_PLAYER))
	ship = Player_by_id(pl->lock.pl_id);
    else
	return;

    /* kps - this should be Player_is_alive() ? */
    if (!Player_is_active(ship))
	return;

    m_gun = Ship_get_m_gun_clpos(pl->ship, pl->dir);
    x2 = CLICK_TO_PIXEL(pl->pos.cx) + pl->vel.x
	+ CLICK_TO_PIXEL(m_gun.cx);
    y2 = CLICK_TO_PIXEL(pl->pos.cy) + pl->vel.y
	+ CLICK_TO_PIXEL(m_gun.cy);
    x3 = CLICK_TO_PIXEL(ship->pos.cx) + ship->vel.x;
    y3 = CLICK_TO_PIXEL(ship->pos.cy) + ship->vel.y;

    ship_dist = Wrap_length(PIXEL_TO_CLICK(x3 - x2),
			    PIXEL_TO_CLICK(y3 - y2)) / CLICK;

    if (ship_dist >= options.pulseSpeed * options.pulseLife + SHIP_SZ)
	return;

    dir3 = Wrap_findDir(x3 - x2, y3 - y2);
    Robot_set_pointing_direction(pl, dir3);

	SET_BIT(pl->used, HAS_LASER);
}

static bool Detect_ship(player_t *pl, player_t *ship)
{
    double distance;

    /* can't go after non-playing ships */
    if (!Player_is_alive(ship))
	return false;

    /* can't do anything with phased ships */
    if (Player_is_phasing(ship))
	return false;

    /* trivial */
    if (pl->visibility[GetInd(ship->id)].canSee)
	return true;

    /*
     * can't see it, so it must be cloaked
     * maybe we can detect it's presence from other clues?
     */
    distance = Wrap_length(ship->pos.cx - pl->pos.cx,
			   ship->pos.cy - pl->pos.cy) / CLICK;
    /* can't detect ships beyond visual range */
    if (distance > Visibility_distance)
	return false;

    if (Player_is_thrusting(ship)
	&& options.cloakedExhaust)
	return true;

    if (BIT(ship->used, HAS_SHOT)
	|| BIT(ship->used, HAS_LASER)
	|| Player_is_refueling(ship)
	|| Player_is_repairing(ship)
	|| Player_uses_connector(ship)
	|| Player_uses_tractor_beam(ship))
	return true;

    if (BIT(ship->have, HAS_BALL))
	return true;

    /* the sky seems clear.. */
    return false;
}

/*
 * Determine how important an item is to a given player.
 * Return one of the following 3 values:
 */
#define ROBOT_MUST_HAVE_ITEM	2	/* must have */
#define ROBOT_HANDY_ITEM	1	/* handy */
#define ROBOT_IGNORE_ITEM	0	/* ignore */
/*
 */
static int Rank_item_value(player_t *pl, enum Item itemtype)
{
    robot_default_data_t *my_data = Robot_suibot_get_data(pl);

    if (itemtype == ITEM_AUTOPILOT)
	return ROBOT_IGNORE_ITEM;		/* never useful for robots */
    if (pl->item[itemtype] >= world->items[itemtype].limit)
	return ROBOT_IGNORE_ITEM;		/* already full */
    if ((IsDefensiveItem(itemtype)
	 && CountDefensiveItems(pl) >= options.maxDefensiveItems)
	|| (IsOffensiveItem(itemtype)
	 && CountOffensiveItems(pl) >= options.maxOffensiveItems))
	return ROBOT_IGNORE_ITEM;
    if (itemtype == ITEM_FUEL) {
	if (pl->fuel.sum >= pl->fuel.max * 0.90)
	    return ROBOT_IGNORE_ITEM;		/* already (almost) full */
	else if ((pl->fuel.sum < (BIT(world->rules->mode, TIMING))
		  ? my_data->fuel_l1
		  : my_data->fuel_l2))
	    return ROBOT_MUST_HAVE_ITEM;		/* ahh fuel at last */
	else
	    return ROBOT_HANDY_ITEM;
    }
    if (BIT(world->rules->mode, TIMING)) {
	switch (itemtype) {
	case ITEM_FUEL:		/* less refuel stops */
	case ITEM_REARSHOT:	/* shoot competitors behind you */
	case ITEM_AFTERBURNER:	/* the more speed the better */
	case ITEM_TRANSPORTER:	/* steal fuel when you overtake someone */
	case ITEM_MINE:		/* blows others off the track */
	case ITEM_ECM:		/* blinded players smash into walls */
	case ITEM_EMERGENCY_THRUST:	/* makes you go really fast */
	case ITEM_EMERGENCY_SHIELD:	/* could be useful when ramming */
	    return ROBOT_MUST_HAVE_ITEM;
	case ITEM_WIDEANGLE:	/* not important in racemode */
	case ITEM_CLOAK:	/* not important in racemode */
	case ITEM_SENSOR:	/* who cares about seeing others? */
	case ITEM_TANK:		/* makes you heavier */
	case ITEM_MISSILE:	/* likely to hit self */
	case ITEM_LASER:	/* cost too much fuel */
	case ITEM_TRACTOR_BEAM:	/* pushes/pulls owner off the track too */
	case ITEM_AUTOPILOT:	/* probably not useful */
	case ITEM_DEFLECTOR:	/* cost too much fuel */
	case ITEM_HYPERJUMP:	/* likely to end up in wrong place */
	case ITEM_PHASING:	/* robots don't know how to use them yet */
	case ITEM_MIRROR:	/* not important in racemode */
	case ITEM_ARMOR:	/* makes you heavier */
	    return ROBOT_IGNORE_ITEM;
	default:		/* unknown */
	    warn("Rank_item_value: unknown item %ld.", itemtype);
	    return ROBOT_IGNORE_ITEM;
	}
    } else {
	switch (itemtype) {
	case ITEM_EMERGENCY_SHIELD:
	case ITEM_DEFLECTOR:
	case ITEM_ARMOR:
	    if (BIT(pl->have, HAS_SHIELD))
		return ROBOT_HANDY_ITEM;
	    else
		return ROBOT_MUST_HAVE_ITEM;

	case ITEM_REARSHOT:
	case ITEM_WIDEANGLE:
	    if (options.maxPlayerShots <= 0
		|| options.shotLife <= 0
		|| !options.allowPlayerKilling)
		return ROBOT_HANDY_ITEM;
	    else
		return ROBOT_MUST_HAVE_ITEM;

	case ITEM_MISSILE:
	    if (options.maxPlayerShots <= 0
		|| options.shotLife <= 0
		|| !options.allowPlayerKilling)
		return ROBOT_IGNORE_ITEM;
	    else
		return ROBOT_MUST_HAVE_ITEM;

	case ITEM_MINE:
	case ITEM_CLOAK:
	    return ROBOT_MUST_HAVE_ITEM;

	case ITEM_LASER:
	    if (options.allowPlayerKilling)
		return ROBOT_MUST_HAVE_ITEM;
	    else
		return ROBOT_HANDY_ITEM;

	case ITEM_PHASING:	/* robots don't know how to use them yet */
	    return ROBOT_IGNORE_ITEM;

	default:
	    break;
	}
    }
    return ROBOT_HANDY_ITEM;
}

static void Robot_suibot_play(player_t *pl)
{
  player_t *ship;
  int direction;
  double distance, ship_dist, enemy_dist, speed, x_speed, y_speed;
  int item_dist, mine_dist, item_i, mine_i;
  int j, ship_i, item_imp, enemy_i, shoot_time;
  bool harvest_checked, evade_checked, navigate_checked;
  robot_default_data_t *my_data = Robot_suibot_get_data(pl);
  
  double ship_dist_closest;
  player_t *closest_opponent;
  closest_opponent= NULL;
  const int maxdist =  1200; /* maximum distance from which to try to pop ball*/
  double ball_dist;
  
  pl->turnspeed = 0;
  pl->turnacc = 0;
  pl->power = MAX_PLAYER_POWER;
  
  if(my_data->robot_mode == RM_TAKE_OFF){
    Robot_take_off_from_base(pl);
    return;
  }
  /* important goal is not to be shot */
  if(Robot_evade_shot(pl)){
    return;
  }
  
  /* Try not to crash into walls */


  if(pl->velocity > options.maxUnshieldedWallBounceSpeed){
      double time;
      time= (pl->velocity 
	     - options.maxUnshieldedWallBounceSpeed)/(pl->power / pl->mass);
      time=time*0.05;
      
      if(Wall_in_between_points(pl->pos.cx, 
				pl->pos.cy,
				pl->pos.cx + pl->vel.x * time,
				pl->pos.cy + pl->vel.y * time)) {
      direction= (int)findDir(-pl->vel.x, -pl->vel.y);
      Robot_set_pointing_direction(pl, direction);
      printf("avoiding wall\n");
      Thrust(pl, true);
      return;
      }
  }


      Thrust(pl, false);
  
  ship_dist_closest= 2* World.hypotenuse;
  for (ship_i = 0; ship_i < NumPlayers; ship_i++) {
   player_t *ship = Player_by_index(ship_i); 
    ship_dist = 
      CLICK_TO_PIXEL((int)
	(Wrap_length((pl->pos.cx - ship->pos.cx),
	       (pl->pos.cy - ship->pos.cy))));
    
    if(BIT(ship->have, HAS_BALL ))
      ship_dist = ship_dist/3.0;
    /* 
     * Player with ball is considered as "much closer" 
     *
     * this is rather arbitrary 
     * the reasoning goes: dont try to attack player 
     * with ball who is really far off
     * while some other player is really really close
     */
    
    if ((ship->id != pl->id)
	&& Player_is_alive(ship)
	&& ship_dist < ship_dist_closest 
	&& (pl->team != ship->team) 
      	&& ((!BIT(ship->used, HAS_SHIELD))
	    || Wrap_length(ship->pos.cx - pl->pos.cx,
			   ship->pos.cy - pl->pos.cy) < 8000)
	)
      {
	ship_dist_closest = ship_dist;
	closest_opponent = ship; 
      }
  }
  
  if(ship_dist_closest <  maxdist && ! closest_opponent){ 
      char msg[MSG_LEN];
      /* if not true, there's a bug */ 
      warn(" Robotdef.c: opponent very close, but variable empty!\n");
      sprintf (msg,"Bug: Chasing a non-existant opponent! [%s]",pl->name);
      Set_message(msg);
      return;
  }
  
  if(ship_dist_closest <  maxdist && BIT(closest_opponent->used, HAS_SHIELD)){
      direction = Wrap_cfindDir(-closest_opponent->pos.cx + pl->pos.cx,
				-closest_opponent->pos.cy + pl->pos.cy);
      Robot_set_pointing_direction(pl, (int)(fabs(direction+0.5)));
      Thrust(pl, true); 
      return;
  }
  

  
  
  /* Closest_ball - should be function, but how do I return a ball + a dist?*/

 ballobject_t *closest_ball, *ball;
 object_t **obj_list, *object;
 //robot_default_data_t *my_data = Robot_suibot_get_data(pl);

 double     closest_ball_dist;
 int     i;
 int     obj_count;
 const int                   max_objs = 1000;
 
 ball_dist = 2 * maxdist;
 closest_ball_dist= 2* maxdist;
 closest_ball=NULL;
 
 Cell_get_objects(pl->pos, (int)(Visibility_distance / BLOCK_SZ),
		  max_objs, &obj_list, &obj_count);
 
 for (i = 0; i < obj_count; i++) { /*for .. obj_count*/
   object = obj_list[i];
   if (object->type == OBJ_BALL) {
     ball= BALL_PTR(object);
     ball_dist = Wrap_length(pl->pos.cx - ball->pos.cx,
			     pl->pos.cy - ball->pos.cy) / CLICK;
     if (ball_dist < closest_ball_dist) {
       closest_ball_dist = ball_dist;
       closest_ball = ball;
     }
   }
 }    
 ball =  closest_ball;
 
 if(ball){
   if (Wall_in_between_points((pl->pos.cx),(pl->pos.cy),(ball->pos.cx),
			      (ball->pos.cy)))
     ball_dist = 2*  World.hypotenuse; 
 }
 

 if(ship_dist_closest <  maxdist
    && ship_dist_closest < (2.5 * ball_dist)
    && (Wall_in_between_points((pl->pos.cx),
			       (pl->pos.cy),
			       (closest_opponent->pos.cx),
			       (closest_opponent->pos.cy)) == 0)
    && (!BIT(pl->have, HAS_BALL))	
     ) {
   Robot_attack_player(pl,closest_opponent);
   return;
 }

 if( ball 
     && ball_dist < maxdist 
     && Wrap_length(ball->pos.cx - ball->ball_treasure->pos.cx,
     		    ball->pos.cy - ball->ball_treasure->pos.cy
	 ) > 10000
     ){
     Robot_ram_object(pl, OBJ_PTR(ball));
     return;
 }

 /* Helps to get stuck on walls less frequently
    10 propably only works ok with framerate of 50 fps 
    plan: a) make value depend on framerate,
    b) use better algorithm than that
    ---> probably no problem anymore because of turnpush.
    Robots should be able to get free anyways now.
 */

 if((pl->last_wall_touch + 11) >= frame_loops) {
     direction= (int)findDir(pl->vel.x,pl->vel.y);
     Robot_set_pointing_direction(pl, direction);
     
     Thrust(pl, false);
     return;
 }
 
 if((pl->last_wall_touch + 14) >= frame_loops){
     direction= (int)findDir(pl->vel.x,pl->vel.y);
     Robot_set_pointing_direction(pl, direction);
     if(!Wall_in_between_points(pl->pos.cx, pl->pos.cy, 
				pl->pos.cx + pl->vel.x * 10,
				pl->pos.cy + pl->vel.y * 10)){
	 Thrust(pl, true);
     }
 }
 
/* nothing sensible to to at the moment */
 Robot_move_randomly(pl);
}



/*
 * This is called each round.
 * It allows us to adjust our file local parameters.
 */

static void Robot_suibot_round_tick(void)
{
    double min_visibility = 256.0;
    double min_enemy_distance = 512.0;

    /* reduce visibility when there are a lot of robots. */
    Visibility_distance = VISIBILITY_DISTANCE;
	/*min_visibility
	+ (((VISIBILITY_DISTANCE - min_visibility)
	    * (NUM_IDS - NumRobots)) / NUM_IDS);*/


    /* limit distance to allowable enemies. */
    Max_enemy_distance = world->hypotenuse;
    if (world->hypotenuse > Visibility_distance)
	Max_enemy_distance =  world->hypotenuse;
/*	min_enemy_distance
	    + (((world->hypotenuse - min_enemy_distance)
		* (NUM_IDS - NumRobots)) / NUM_IDS);*/
}
