#include <stdio.h>
#include <assert.h>
#include <SDL2/SDL.h>

#include "./player.h"
#include "./platforms.h"
#include "./camera.h"
#include "./game.h"
#include "./error.h"
#include "./lt.h"

typedef enum game_state_t {
    GAME_STATE_RUNNING = 0,
    GAME_STATE_PAUSE,
    GAME_STATE_QUIT,

    GAME_STATE_N
} game_state_t;

typedef struct game_t {
    lt_t *lt;

    game_state_t state;
    player_t *player;
    platforms_t *platforms;
    camera_t *camera;
    char *level_file_path;
} game_t;

game_t *create_game(const char *level_file_path)
{
    assert(level_file_path);

    lt_t *const lt = create_lt();
    if (lt == NULL) {
        return NULL;
    }

    game_t *game = PUSH_LT(lt, malloc(sizeof(game_t)), free);
    if (game == NULL) {
        throw_error(ERROR_TYPE_LIBC);
        RETURN_LT(lt, NULL);
    }

    game->player = PUSH_LT(lt, create_player(100.0f, 0.0f), destroy_player);
    if (game->player == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->platforms = PUSH_LT(lt, load_platforms_from_file(level_file_path), destroy_platforms);
    if (game->platforms == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->camera = PUSH_LT(lt, create_camera(vec(0.0f, 0.0f)), destroy_camera);
    if (game->camera == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->level_file_path = PUSH_LT(lt, malloc(sizeof(char) * (strlen(level_file_path) + 1)), free);
    if (game->level_file_path == NULL) {
        throw_error(ERROR_TYPE_LIBC);
        RETURN_LT(lt, NULL);
    }

    strcpy(game->level_file_path, level_file_path);

    game->state = GAME_STATE_RUNNING;
    game->lt = lt;

    return game;
}

void destroy_game(game_t *game)
{
    assert(game);
    RETURN_LT0(game->lt);
}

int game_render(const game_t *game, SDL_Renderer *renderer)
{
    assert(game);
    assert(renderer);

    if (game->state == GAME_STATE_QUIT) {
        return 0;
    }

    if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0) {
        throw_error(ERROR_TYPE_SDL2);
        return -1;
    }

    if (SDL_RenderClear(renderer) < 0) {
        throw_error(ERROR_TYPE_SDL2);
        return -1;
    }

    if (render_player(game->player, renderer, game->camera) < 0) {
        return -1;
    }

    if (render_platforms(game->platforms, renderer, game->camera) < 0) {
        return -1;
    }

    SDL_RenderPresent(renderer);

    return 0;
}

int game_update(game_t *game, Uint32 delta_time)
{
    assert(game);
    assert(delta_time > 0);

    if (game->state == GAME_STATE_QUIT) {
        return 0;
    }

    if (game->state == GAME_STATE_RUNNING) {
        update_player(game->player, game->platforms, delta_time);
        player_focus_camera(game->player, game->camera);
    }

    return 0;
}


static int game_event_pause(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (event->type) {
    case SDL_QUIT:
        game->state = GAME_STATE_QUIT;
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case SDLK_p:
            game->state = GAME_STATE_RUNNING;
            break;
        }
        break;
    }

    return 0;
}

static int game_event_running(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (event->type) {
    case SDL_QUIT:
        game->state = GAME_STATE_QUIT;
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case SDLK_SPACE:
            player_jump(game->player);
            break;

        case SDLK_q:
            printf("Reloading the level from '%s'...\n", game->level_file_path);

            game->platforms = RESET_LT(
                game->lt,
                game->platforms,
                load_platforms_from_file(game->level_file_path));

            if (game->platforms == NULL) {
                print_current_error_msg("Could not reload the level");
                game->state = GAME_STATE_QUIT;
                return -1;
            }
            break;

        case SDLK_p:
            game->state = GAME_STATE_PAUSE;
            break;
        }
        break;

    case SDL_JOYBUTTONDOWN:
        if (event->jbutton.button == 1) {
            player_jump(game->player);
        }
        break;
    }

    return 0;
}

int game_event(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (game->state) {
    case GAME_STATE_RUNNING:
        return game_event_running(game, event);

    case GAME_STATE_PAUSE:
        return game_event_pause(game, event);

    default: {}
    }

    return 0;
}


int game_input(game_t *game,
               const Uint8 *const keyboard_state,
               SDL_Joystick *the_stick_of_joy)
{
    assert(game);
    assert(keyboard_state);
    assert(the_stick_of_joy);

    if (game->state == GAME_STATE_QUIT || game->state == GAME_STATE_PAUSE) {
        return 0;
    }

    if (keyboard_state[SDL_SCANCODE_A]) {
        player_move_left(game->player);
    } else if (keyboard_state[SDL_SCANCODE_D]) {
        player_move_right(game->player);
    } else if (the_stick_of_joy && SDL_JoystickGetAxis(the_stick_of_joy, 0) < 0) {
        player_move_left(game->player);
    } else if (the_stick_of_joy && SDL_JoystickGetAxis(the_stick_of_joy, 0) > 0) {
        player_move_right(game->player);
    } else {
        player_stop(game->player);
    }

    return 0;
}

int is_game_over(const game_t *game)
{
    return game->state == GAME_STATE_QUIT;
}