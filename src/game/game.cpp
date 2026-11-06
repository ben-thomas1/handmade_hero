#include "game/game.hpp"


void
update_game(Input *input, State *state)
{
    if (input->close_request || input->buttons[KEY_Q].pressed) state->running = false;
    state->mouse_x = input->mouse_x;
    state->mouse_y = input->mouse_y;
}
