#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "score.h"

#include "sprite.h"
#include "collider.h"
#include "linkedlist.h"

#define SPACESHIP_BOOST     0.25
#define SPACESHIP_FRICTION  0.002
#define BULLET_LIFETIME     25
#define BULLET_SPEED        10
#define DEFAULT_PTSIZE      24
#define NUMBER_OF_LIFES     5
#include "level.h"

#define GDB()  __asm__("int $0x3")
//#define GDB() __builtin_trap();

bool gameover;
SDL_Surface *screen;
SDL_Surface *shield_surface;
sprite_t sprite_ship;
list_ptr l_sprite_bullet;
list_ptr l_sprite_comet;
list_ptr l_sprite_explosion;
list_ptr l_sprite_text;
list_ptr l_sprite_life_counter;
list_ptr l_score_el = NULL;

bool shield_active = false;
int shield_timer = 0;
#define SHIELD_DURATION 180
#define SHIELD_FRAME_COUNT 8
#define SHIELD_SIZE 48
#define SHIELD_MAX_HITS 3
#define SHIELD_COOLDOWN_SECONDS 5
#define SHIELD_COOLDOWN_FRAMES 250
#define SHIELD_MESSAGE_LIFETIME 150

bool shoot_again;
bool shield_key_was_down = false;
int shield_hits = 0;
int shield_cooldown = 0;
int score;
int level = LEVEL_MIN;

typedef enum {
  MENU_START,
  MENU_SCOREBOARD,
  MENU_QUIT
} menu_result_t;

/* Declaration of few prototypes because there is no .h file with them */
int init_sdl(void);
void general_events(char *keys);
void game_events(char *keys);
void draw_explosion(int i,int j);
void draw_shield(void);
void show_shield_cooldown_message(TTF_Font *font);
void draw_fire(void);
void draw_life_counter(void);
void draw_score(TTF_Font *font);
void next_level(TTF_Font *font);
void draw_sprites(list_ptr *l_sprite);
void split(sprite_t old_comet, list_ptr **l_sprite_comet, enum sprite_type new_type);
void split_and_score(list_ptr element, list_ptr *l_sprite_comet, bool update_score);
menu_result_t run_menu(TTF_Font *font_title, TTF_Font *font_button, SDL_Surface *bg);

static Uint32 surface_pixel(SDL_Surface *surface, int x, int y) {
  int bytes_per_pixel = surface->format->BytesPerPixel;
  Uint8 *pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * bytes_per_pixel;

  switch (bytes_per_pixel) {
    case 1:
      return *pixel;
    case 2:
      return *(Uint16 *)pixel;
    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        return pixel[0] << 16 | pixel[1] << 8 | pixel[2];
      return pixel[0] | pixel[1] << 8 | pixel[2] << 16;
    default:
      return *(Uint32 *)pixel;
  }
}

/* SDL Initialisation. Create windows and so on
 *  return 0 if everything is ok, otherwise 1.
 * */
int init_sdl(void) {
  /* initialize SDL */
  SDL_Init(SDL_INIT_VIDEO);
  /* set the title bar */
  SDL_WM_SetCaption("Comet buster", "w00t!");
  /* create window */
  screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_SWSURFACE);
  if (!screen)
    return 1;
  /* Initialize the TTF library a nd open the font */
  if ( TTF_Init() < 0 ) {
    return 1;
  }

  /* set keyboard repeat */
  SDL_EnableKeyRepeat(50, 50);
  return 0;
}

/* Draw the current frame of the shield spritesheet. */
void draw_shield(void) {
  int frame_width;
  int frame;
  int center_x;
  int center_y;
  SDL_Rect source;
  SDL_Rect destination;

  if (!shield_active || shield_surface == NULL)
    return;

  frame_width = shield_surface->w / SHIELD_FRAME_COUNT;
  frame = (SHIELD_DURATION - shield_timer) /
          (SHIELD_DURATION / SHIELD_FRAME_COUNT);
  if (frame >= SHIELD_FRAME_COUNT)
    frame = SHIELD_FRAME_COUNT - 1;

  center_x = sprite_ship->rc_screen_xy.x + sprite_ship->size / 2;
  center_y = sprite_ship->rc_screen_xy.y + sprite_ship->size / 2;
  source.x = frame * frame_width;
  source.y = 0;
  source.w = frame_width;
  source.h = shield_surface->h;
  destination.x = center_x - SHIELD_SIZE / 2;
  destination.y = center_y - SHIELD_SIZE / 2;
  SDL_BlitSurface(shield_surface, &source, screen, &destination);
}

void show_shield_cooldown_message(TTF_Font *font) {
  SDL_Color color = {255, 255, 0, 0};
  SDL_Surface *message_surface;
  sprite_t message_sprite;
  char message[128];

  if (font == NULL)
    return;

  sprintf(message, "Bouclier inactif pendant %d secondes",
          SHIELD_COOLDOWN_SECONDS);
  message_surface = TTF_RenderText_Solid(font, message, color);
  if (message_surface == NULL)
    return;

  message_sprite = sprite_new_text(message_surface, 120, SCREEN_HEIGHT - 45);
  message_sprite->lifetime = SHIELD_MESSAGE_LIFETIME;
  l_sprite_text = list_add(message_sprite, l_sprite_text);
}


/* general SDL events hanlder
 * */
void general_events(char* keys) {
  SDL_Event event;
  while(SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        gameover = true;
        break;
      case SDL_KEYUP:
        keys[event.key.keysym.sym] = 0;
        break;
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
          case SDLK_q:
            gameover = true;
            break;
          case SDLK_d:
            GDB();
            break;
        }
        keys[event.key.keysym.sym] = 1;
        break;
    }
  }
}


/* In game event handler
 * */
void game_events(char* key) {
  SDLKey tabkey[] = {SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT, SDLK_SPACE, SDLK_e};
  int i;
  if (key[tabkey[0]]) { //UP
    sprite_boost(sprite_ship, SPACESHIP_BOOST);
  }

  if (key[tabkey[1]]) { //DOWN
    sprite_boost(sprite_ship, -SPACESHIP_BOOST);
  }
  if (key[tabkey[2]]) { // LEFT
    sprite_turn_left(sprite_ship);
  }
  if (key[tabkey[3]]) { // RIGHT
    sprite_turn_right(sprite_ship);
  }
  if (key[tabkey[4]]) { // SPACE
    if (shoot_again) {
      draw_fire();
      shoot_again = false;
    }
  }
  if (key[tabkey[5]] && !shield_key_was_down) {
    if (shield_active) {
      shield_active = false;
      shield_timer = 0;
    } else if (shield_cooldown == 0) {
      shield_active = true;
      shield_timer = SHIELD_DURATION;
      shield_hits = 0;
    }
  }
  shield_key_was_down = key[tabkey[5]];

  if (shield_active) {
    shield_timer--;

    if (shield_timer <= 0) {
      shield_active = false;
    }
  }

  if (shield_cooldown > 0)
    shield_cooldown--;

  if (!key[tabkey[4]]) {
    shoot_again = true;
  }
}

/* Add a new explosion to the list
 * */
void draw_explosion(int i,int j) {
  sprite_t sprite;
  int colorkey = SDL_MapRGB(screen->format, 255, 0, 255);
  sprite = sprite_new(EXPLOSION, "sprites/explosion17.bmp", colorkey, 64, 25, 0, i-32, j-32, 0., 0., 0.);
  sprite->lifetime = 25;
  l_sprite_explosion = list_add(sprite, l_sprite_explosion);
}


/* Launch a bullet ;)
 *  Add the bullet to the list
 * */
void draw_fire(void) {
  int colorkey;
  float dir_angle;
  sprite_t sprite;
  colorkey = SDL_MapRGB(screen->format, 255, 125, 0);
  dir_angle = 2.*PI*sprite_ship->anim_sprite_num/sprite_ship->anim_sprite_num_max;
  sprite = sprite_new(BULLET, "sprites/bullet02.bmp", colorkey, 4, 1, 0,
                      sprite_ship->rc_screen_xy.x + sprite_ship->size / 2 * (1 + cos(dir_angle)),
                      sprite_ship->rc_screen_xy.y + sprite_ship->size / 2 * (1 - sin(dir_angle)),
                      sprite_ship->vx + BULLET_SPEED * cos(dir_angle),
                      sprite_ship->vy - BULLET_SPEED * sin(dir_angle),
                      0.);
  sprite->lifetime = BULLET_LIFETIME;
  l_sprite_bullet = list_add(sprite, l_sprite_bullet);
}

/* Draw the score sprite
 *  Add it to the list
 * */
void draw_score(TTF_Font * font) {

  SDL_Surface *score_surf;
  SDL_Color score_color = {255, 255, 255, 0};
  char score_text[1024];
  sprite_t sprite;

  sprintf(score_text, "%08d", score);
  score_surf = TTF_RenderText_Solid(font, score_text, score_color);
  sprite = sprite_new_text(score_surf, 5, 5);
  if (l_score_el)
    list_remove(l_score_el, &l_sprite_text);
  l_score_el = l_sprite_text = list_add(sprite, l_sprite_text);
}

/* Draw the life counter sprites
 *  Add sprites to the list
 * */
void draw_life_counter(void) {
  int x = 5;
  int i;
  sprite_t sprite;
  int colorkey = SDL_MapRGB(screen->format, 255, 0, 255);

  /* create and initialize the life counter sprite */
  l_sprite_life_counter = list_new();
  for (i = 0; i < NUMBER_OF_LIFES; i++) {
    sprite = sprite_new(LIFE_COUNTER, "sprites/grayship_16x16.bmp", colorkey, 16, 1, 0, x, 35, 0., 0., 0.);
    l_sprite_life_counter = list_add(sprite, l_sprite_life_counter);
    x += 15;
  }
}

/* Draw the new level ban,
 *  and level up
 * */
void next_level( TTF_Font * font) {
  SDL_Color text_color = {255, 255, 0, 0};//R,G,B,A
  char text[1024];
  sprite_t sprite_text;

  printf("DEBUG: Congratulation! You won the level %d (score=%d)\n",level,score);
  fflush(stdout);
  level++;
  printf("DEBUG: Start level %d, good luck!\n",level);
  fflush(stdout);
  // Add the "next level" text sprite to the text sprite list
  sprintf(text, "LEVEL %d", level);
  SDL_Surface *text_surf = TTF_RenderText_Solid(font, text, text_color);
  sprite_text = sprite_new_text(text_surf, SCREEN_WIDTH/2-60, SCREEN_HEIGHT/3);
  sprite_text->lifetime = 150;
  l_sprite_text = list_add(sprite_text, l_sprite_text);
}


/* Blit a sprites list.
 *  Depending on the sprite type, we had physics & effects
 * */
void draw_sprites(list_ptr *l_sprite) {
  sprite_t sprite;
  list_ptr l_ptr = *(l_sprite);

  // nothing to draw
  if (list_is_empty(l_ptr))
    return;

  // loop throught the list and draw
  while (!list_is_empty(l_ptr)) {
    sprite = list_head_sprite(l_ptr);
    list_ptr list_el = l_ptr;

    l_ptr = list_next(l_ptr);

    // let's take some age if needed
    if (sprite_can_die(sprite)) {
      sprite_get_older(sprite);

      if (sprite_is_dead(sprite)) {
        //printf("DEBUG sprite_is_dead %d\n",sprite->type);
        list_remove(list_el, l_sprite);
        continue;
      }
    }

    if (sprite) {
      if (sprite_is_ennemy(sprite)||sprite->type==BULLET) {
        sprite_play_physics(sprite);
      }

      // comet are dangerous, they rotate on themself to maximize damages
      if (sprite_is_comet(sprite)) {
        sprite_turn_right(sprite);
      }

      // explosions effect is simple, simply add a rotation :)
      if (sprite->type == EXPLOSION) {
        sprite_turn_left(sprite);
      }

      // TXT sprite don't provide animation files, so provide a special blit
      if (sprite->type == TXT)
        SDL_BlitSurface(sprite->sprite, NULL, screen, &sprite->rc_screen_xy);
      else
        SDL_BlitSurface(sprite->sprite, &sprite->rc_anim_xy, screen, &sprite->rc_screen_xy);
    }
  }
}


/* Handle consequences of a collision:
 *   increase score depending on the sprite->type
 *   split object if needed
 * */
void split_and_score(list_ptr element, list_ptr *l_sprite_comet, bool update_score) {
  //score + split
  int diff = 0;
  sprite_t data = list_head_sprite(element);
  switch (data->type) {
    case L_COMET:
      diff = 20;
      split(data, &l_sprite_comet, M_COMET);
      break;
    case M_COMET:
      diff = 50;
      split(data, &l_sprite_comet, S_COMET);
      break;
    case S_COMET:
      diff = 100;
      break;
    case ET:
      diff = 60;
      break;
  }
  score += (update_score)?diff:0;
}

/* Split an asteroid into two new objects which get most of parameters from
 * their parent (old_comet). They are also added to the list of comets.
 * */
void split(sprite_t old_comet, list_ptr **l_sprite_comet, enum sprite_type new_type) {
  float speed = get_base_speed(level, new_type);
  int angle = (float)(rand()%360)/360*2*PI;
  int colorkey = old_comet->colorkey;
  int x = old_comet->x;
  int y = old_comet->y;
  int new_sprite_size = old_comet->size/2; //64 32 26
  const char * fname = get_comet_sprite(level, new_type);
  sprite_t first = sprite_new(new_type, fname, colorkey, new_sprite_size, 32, 0, x, y, speed*cos(angle), speed*sin(angle), 0.);
  //change angle for the second asteroid
  angle = (float)(rand()%360)/360*2*PI;
  sprite_t second = sprite_new(new_type, fname, colorkey, new_sprite_size, 32, 0, x, y, speed*cos(angle), speed*sin(angle), 0.);
  **l_sprite_comet = list_add(first, **l_sprite_comet);
  **l_sprite_comet = list_add(second, **l_sprite_comet);
}

static bool point_in_rect(int x, int y, SDL_Rect *r) {
  return (x >= r->x && x <= r->x + r->w && y >= r->y && y <= r->y + r->h);
}

static SDL_Surface *render_text_centered(TTF_Font *font, const char *text, SDL_Color color, int cx, int y, SDL_Rect *out_rect) {
  SDL_Surface *surf = TTF_RenderText_Solid(font, text, color);
  out_rect->x = cx - surf->w / 2;
  out_rect->y = y;
  out_rect->w = surf->w;
  out_rect->h = surf->h;
  return surf;
}

menu_result_t run_menu(TTF_Font *font_title, TTF_Font *font_button, SDL_Surface *bg) {
  SDL_Color white  = {255, 255, 255, 0};
  bool in_menu = true;
  menu_result_t result = MENU_QUIT;
  SDL_Rect rcBg = {0, 0, 0, 0};

  SDL_Rect rc_title,  rc_score;
  SDL_Surface *s_title = render_text_centered(font_title,  "PRESS A KEY TO START", white, SCREEN_WIDTH/2, SCREEN_HEIGHT/3, &rc_title);
  SDL_Surface *s_score = render_text_centered(font_button, "SCORE BOARD",          white, SCREEN_WIDTH/2, SCREEN_HEIGHT/3 + 130, &rc_score);

  SDL_Rect btn_score = {rc_score.x - 10, rc_score.y - 6, rc_score.w + 20, rc_score.h + 12};

  while (in_menu) {
    SDL_Event event;
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    bool hover_score = point_in_rect(mx, my, &btn_score);

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          result = MENU_QUIT;
          in_menu = false;
          break;

        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            if (hover_score) {
              result = MENU_SCOREBOARD;
              in_menu = false;
            }
          }
          break;

        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_ESCAPE) {
            result = MENU_QUIT;
            in_menu = false;
          } else {
            result = MENU_START;
            in_menu = false;
          }
          break;
      }
    }

    SDL_BlitSurface(bg, NULL, screen, &rcBg);

    SDL_BlitSurface(s_title, NULL, screen, &rc_title);

    SDL_FillRect(screen, &btn_score,
        hover_score ? SDL_MapRGB(screen->format, 90, 90, 90) : SDL_MapRGB(screen->format, 40, 40, 40));

    SDL_BlitSurface(s_score, NULL, screen, &rc_score);

    SDL_UpdateRect(screen, 0, 0, 0, 0);
    while (SDL_Flip(screen) != 0) {
      SDL_Delay(1);
    }
    SDL_Delay(15);
  }

  SDL_FreeSurface(s_title);
  SDL_FreeSurface(s_score);

  return result;
}

/*Function that display best score of the player*/
menu_result_t display_scoreboard(TTF_Font *font_title, TTF_Font *font_button, SDL_Surface *bg) {
  SDL_Color white  = {255, 255, 255, 0};
  bool in_score_menu = true;
  int score = read_score();
  bool has_score = (score >= 0);
  SDL_Rect rcBg = {0, 0, 0, 0};

  SDL_Rect rc_title, rc_score, rc_back;
  const char *title_text = has_score ? "BEST SCORE : " : "NO SCORE";
  SDL_Surface *s_title = render_text_centered(font_title, title_text, white, SCREEN_WIDTH/2, SCREEN_HEIGHT/3, &rc_title);

  SDL_Surface *s_score = NULL;
  if (has_score) {
    char score_text[64];
    sprintf(score_text, "%08d", score);
    s_score = render_text_centered(font_button, score_text, white, SCREEN_WIDTH/2, rc_title.y + rc_title.h + 10, &rc_score);
  }

  SDL_Surface *s_back = render_text_centered(font_button, "MENU",    white, SCREEN_WIDTH/2, SCREEN_HEIGHT/3 + 130, &rc_back);
  char score_text[64];
  sprintf(score_text, "%08d", score);

  SDL_Rect btn_back = {rc_back.x - 10, rc_back.y - 6, rc_back.w + 20, rc_back.h + 12};

  while (in_score_menu) {
    SDL_Event event;
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    bool hover_back = point_in_rect(mx, my, &btn_back);

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            if (hover_back) {
              in_score_menu = false;
            }
          }
          break;
      }
    }
    SDL_BlitSurface(bg, NULL, screen, &rcBg);

    SDL_BlitSurface(s_title, NULL, screen, &rc_title);
    SDL_BlitSurface(s_score, NULL, screen, &rc_score);
    SDL_FillRect(screen, &btn_back,
        hover_back ? SDL_MapRGB(screen->format, 90, 90, 90) : SDL_MapRGB(screen->format, 40, 40, 40));


    SDL_BlitSurface(s_back, NULL, screen, &rc_back);

    SDL_UpdateRect(screen, 0, 0, 0, 0);
    while (SDL_Flip(screen) != 0) {
      SDL_Delay(1);
    }
    SDL_Delay(15);

  }

  SDL_FreeSurface(s_title);
  SDL_FreeSurface(s_score);
  SDL_FreeSurface(s_back);
  return MENU_SCOREBOARD;
}

int main(int argc, char* argv[]) {
  SDL_Surface *temp, *bg;
  SDL_Rect rcBg;
  int colorkey;
  bool collide;
  int cu, cv;
  int timer = 0;
  sprite_t sprite_life_counter;
  sprite_t current_sprite;
  int ret;
  TTF_Font * font_score;
  TTF_Font * font_next_level;
  menu_result_t menu_choice;


  ret = init_sdl();
  if (ret) {
    perror("Error init_sdl: ");
    SDL_Quit();
    return(ret);
  }

  // default colorkey
  colorkey = SDL_MapRGB(screen->format, 255, 0, 255);

  // fonts
  font_score = TTF_OpenFont("fonts/LinLibertine_DR.ttf", 24);
  font_next_level = TTF_OpenFont("fonts/LinLibertine_DR.ttf", 36);;

  temp = SDL_LoadBMP("sprites/backgroundlvl1.bmp");
  bg = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  rcBg.x = 0;
  rcBg.y = 0;

  do {
    menu_choice = run_menu(font_next_level, font_score, bg);

    if (menu_choice == MENU_SCOREBOARD) {
      menu_choice = display_scoreboard(font_next_level, font_score, bg);
    }
  } while (menu_choice == MENU_SCOREBOARD);

  if (menu_choice == MENU_QUIT) {
    SDL_FreeSurface(bg);
    TTF_CloseFont(font_next_level);
    TTF_CloseFont(font_score);
    TTF_Quit();
    SDL_Quit();
    return 0;
  }


  // load and resize the eight-frame shield spritesheet
  temp = SDL_LoadBMP("sprites/shield_sheet.bmp");
  shield_surface = temp ? SDL_DisplayFormat(temp) : NULL;
  if (temp)
    SDL_FreeSurface(temp);
  if (shield_surface) {
    SDL_Surface *source_surface = shield_surface;
    SDL_Surface *small_surface = SDL_CreateRGBSurface(
        SDL_SWSURFACE,
        SHIELD_SIZE * SHIELD_FRAME_COUNT,
        SHIELD_SIZE,
        screen->format->BitsPerPixel,
        screen->format->Rmask,
        screen->format->Gmask,
        screen->format->Bmask,
        screen->format->Amask);
    int source_frame_width = source_surface->w / SHIELD_FRAME_COUNT;
    int frame;

    if (small_surface) {
      for (frame = 0; frame < SHIELD_FRAME_COUNT; frame++) {
        SDL_Rect source = {frame * source_frame_width, 0,
                           source_frame_width, source_surface->h};
        SDL_Rect destination = {frame * SHIELD_SIZE, 0,
                                SHIELD_SIZE, SHIELD_SIZE};
        SDL_SoftStretch(source_surface, &source, small_surface, &destination);
      }
      SDL_FreeSurface(source_surface);
      shield_surface = small_surface;
      SDL_SetColorKey(shield_surface, SDL_SRCCOLORKEY,
                      surface_pixel(shield_surface, 0, 0));
    }
  }

  // create the text sprites list
  l_sprite_text = list_new();

  // initialize score and score sprite
  score = 0;
  draw_score(font_score);
  draw_life_counter();

  //create and initialize the spaceship sprite
  sprite_ship = sprite_new(SHIP, "sprites/greenship-v1.bmp", colorkey, 32, 36, 9, SPACESHIP_INIT_X, SPACESHIP_INIT_Y, 0., 0., SPACESHIP_FRICTION);

  /* create and initialize a comet list (only for tests, comets will be generated in level.c) */
  l_sprite_comet = list_new();
  gen_level(level, &l_sprite_comet, screen);

  //create the bullets list
  l_sprite_bullet = list_new();

  gameover = false;
  shoot_again = true;

  char key[SDLK_LAST] = {0};
  unsigned int lasttime = 0;
  /* message pump */
  while (!gameover) {
    lasttime = SDL_GetTicks();
    SDL_Event event;
    list_ptr l_ptr;
    int counter;

    general_events(key);
    game_events(key);

    /* draw the background */
    SDL_BlitSurface(bg, NULL, screen, &rcBg);

    // draw the text sprites
    draw_sprites(&l_sprite_text);

    // draw the life counter sprites
    draw_sprites(&l_sprite_life_counter);

    /* play & draw the spaceship sprite */
    sprite_play_physics(sprite_ship);
    draw_shield();
    SDL_BlitSurface(sprite_ship->sprite, &sprite_ship->rc_anim_xy, screen, &sprite_ship->rc_screen_xy);

    // draw comets & nyancats
    draw_sprites(&l_sprite_comet);
    // draw bullets
    draw_sprites(&l_sprite_bullet);

    /* collide tests ship <-> comets */
    l_ptr = l_sprite_comet;
    while (!list_is_empty(l_ptr)) {
      current_sprite = list_head_sprite(l_ptr);
      list_ptr list_el = l_ptr;
      l_ptr = list_next(l_ptr);

      collide = collide_test(sprite_ship, current_sprite, screen->format, &cu, &cv);
      if (collide) {
        // draw the explosion
        draw_explosion(cu,cv);
        //no additional score if the ship is destroyed?
        split_and_score(list_el, &l_sprite_comet, false);
        list_remove(list_el, &l_sprite_comet);
        sprite_ship->x = sprite_ship->rc_screen_xy.x = 304;
        sprite_ship->y = sprite_ship->rc_screen_xy.y = 224;
        sprite_ship->vx = 0.;
        sprite_ship->vy = 0.;
        if (!list_is_empty(l_sprite_life_counter)) {
          sprite_t dead_sprite = list_pop_sprite(&l_sprite_life_counter);
          if (dead_sprite)
            sprite_free(dead_sprite);
        } else {
          if (score > read_score()) {
            write_score(score);
          }
          printf(" ============ Game Over ============= \n");
          printf("Score: you reached level %d with %d points\n",level,score);
          fflush(stdout);
          gameover = true;

          if (shield_active) {
            /* The shield destroys the enemy instead of the ship. */
            shield_hits++;
            split_and_score(list_el, &l_sprite_comet, true);
            list_remove(list_el, &l_sprite_comet);

            if (shield_hits >= SHIELD_MAX_HITS) {
              shield_active = false;
              shield_timer = 0;
              shield_cooldown = SHIELD_COOLDOWN_FRAMES;
              show_shield_cooldown_message(font_score);
            }
          } else {
            //no additional score if the ship is destroyed?
            split_and_score(list_el, &l_sprite_comet, false);
            list_remove(list_el, &l_sprite_comet);
            sprite_ship->x = sprite_ship->rc_screen_xy.x = 304;
            sprite_ship->y = sprite_ship->rc_screen_xy.y = 224;
            sprite_ship->vx = 0.;
            sprite_ship->vy = 0.;
            if (!list_is_empty(l_sprite_life_counter)) {
              sprite_t dead_sprite = list_pop_sprite(&l_sprite_life_counter);
              if (dead_sprite)
                sprite_free(dead_sprite);
            } else {
              printf(" ============ Game Over ============= \n");
              printf("Score: you reached level %d with %d points\n",level,score);
              fflush(stdout);
              gameover = true;
            }
          }
        }
      }
    }

    /* collide tests bullets <-> comets */
    l_ptr = l_sprite_bullet;
    while (!list_is_empty(l_ptr)) {
      current_sprite = list_head_sprite(l_ptr);
      list_ptr list_el = l_ptr;
      list_ptr l_ptr_c = l_sprite_comet;

      l_ptr = list_next(l_ptr);
      while (!list_is_empty(l_ptr_c)) {
        sprite_t sprite_comet = list_head_sprite(l_ptr_c);
        list_ptr list_el_c = l_ptr_c;

        l_ptr_c = list_next(l_ptr_c);
        collide = collide_test(current_sprite, sprite_comet, screen->format, &cu, &cv);
        if (collide) {
          // draw the explosion
          draw_explosion(cu,cv);
          split_and_score(list_el_c, &l_sprite_comet, true);//addtional points
          list_remove(list_el_c, &l_sprite_comet);
          list_remove(list_el, &l_sprite_bullet);
          // update the score display
          draw_score(font_score);
          break;
        }
      }
    }

    draw_sprites(&l_sprite_explosion);

    //when enemies are all destroyed, the level is passed
    if (list_is_empty(l_sprite_comet)) {
      // draw the new level
      next_level(font_next_level);
      //clean comets & nyancats list from old level
      list_free(l_sprite_comet);
      l_sprite_comet = list_new();
      gen_level(level, &l_sprite_comet, screen);
    }

    /* update the screen */
    SDL_UpdateRect(screen, 0, 0, 0, 0);

    timer++;
    //FIXME?
    while(SDL_Flip(screen)!=0) { /* if GC not ready to blit */
      SDL_Delay(1); /* wait and keep vertical sync*/
    }
    while(SDL_GetTicks()-lasttime<20) { /* minimal frame time: 20ms */
      SDL_Delay(1);
    }

  }//loop !gameover

  printf("Bye bye.\n");
  /* free the space_ship sprite */
  sprite_free(sprite_ship);
  /* free the background surface */
  SDL_FreeSurface(bg);
  SDL_FreeSurface(shield_surface);
  // Shutdown the TTF library
  TTF_CloseFont(font_next_level);
  TTF_CloseFont(font_score);
  TTF_Quit();
  /* cleanup SDL */
  SDL_Quit();
  return 0;
}//main

