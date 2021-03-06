/*
Nome: Guilherme de Brito Abreu
RA: 173691
*/

#include "../include/model/snake_model.hpp"
#include "../include/controler/snake_controler.hpp"
#include <ncurses.h>

#include "../serial/serializable.hpp"

Fisica::Fisica(ListaDeSnakes *lds) {
  this->lista = lds;
}

void Fisica::feed_snake(){
  srand(time(NULL));
  
  int y = rand()%LINES;
  int x = rand()%COLS;
  int j;
  bool try_again;
  std::vector<Snake*> *s = this->lista->get_snakes();

  do{
    try_again = false;
    
    j = 0;
    while(j < this->food_vector.size() && !try_again){
      if ((int)this->food_vector[j].x == x && (int)this->food_vector[j].y == y){
        try_again = true;
      }
      j++;
    }

    j = 0;
    while(j < s->size() && !try_again){
      std::vector<Corpo*> *corpos = (*s)[j]->get_corpos();
      j++;

      std::vector<pos_2d> last_pos(corpos->size());

      // get last positions
      for (int i = 0; i < corpos->size(); i++) {
        last_pos[i]= (*corpos)[i]->get_posicao();
      }

      // verify if already exist some body in the position x,y
      for (int i = 0; i < corpos->size(); i++){
        if ((int)last_pos[i].x == x && (int)last_pos[i].y == y){
          try_again = true;
          break;
        }
      }
    }

    if (try_again){
      x = rand()%COLS;
      y = rand()%LINES;
    }
  }while(try_again);

  pos_2d next_food = {(float)x,(float)y};
  this->food_vector.push_back(next_food);
  return;
}

short int Fisica::update(int snake_ID, bool snakes_status[]) {
  // get snake list!
  std::vector<Snake*> *s = this->lista->get_snakes();

  int snakes_in_game = 0;

  for (int i = 0; i < s->size(); i++){
    if (snakes_status[i] == true)
      snakes_in_game++;
  }

  if (snakes_in_game == 1)
    return -5;

  std::vector<Corpo*> *c = (*s)[snake_ID]->get_corpos();
  vel_2d vel = (*c)[0]->get_velocidade();
  std::vector<pos_2d> last_pos(c->size());
  pos_2d new_pos;

  // get last positions
  for (int i = 0; i < c->size(); i++) {
    last_pos[i]= (*c)[i]->get_posicao();
  }
  
  // compute new position of the head
  new_pos.x = last_pos[0].x + vel.x;
  new_pos.y = last_pos[0].y + vel.y;

  // borders coditions
  if ((new_pos.x < 0) || (new_pos.y < 0)){
    return -2;
  }
  if ((new_pos.x >= COLS) || (new_pos.y >= LINES)){
    return -2;
  }

  // update snake position
  for (int i = 1; i < c->size(); i++) {
    (*c)[i]->update(vel, last_pos[i-1]);
  }
  (*c)[0]->update(vel, new_pos);
  
  // verify if snake collided
  short int collison = this->verify_snake_collison(s, snake_ID, snakes_status);
  if (collison != -3)
    return collison;

  // increase snake size or not
  if (this->verify_snake_ate(c)){
    Corpo *new_corpo = new Corpo(vel, last_pos[c->size()-1]);
    (*s)[snake_ID]->add_corpo(new_corpo);
    collison = -4;
  }

  return collison;
}

bool Fisica::verify_snake_ate(std::vector<Corpo*> *c){
  bool ate;
  pos_2d food_pos;

  for (int i=0; i < this->food_vector.size() && !ate; i++){ 
    food_pos = this->food_vector[i];
    
    if ((int)(*c)[0]->get_posicao().x == (int)food_pos.x \
          && (int)(*c)[0]->get_posicao().y == (int)food_pos.y){
      ate = true;
      this->food_vector.erase(this->food_vector.begin() + i);
    }
    else{
      ate = false;
    }
  }
  
  return ate;
}

short int Fisica::verify_snake_collison(std::vector<Snake*> *s, int snake_ID, bool snakes_status[]){
  std::vector<Corpo*> *snake_target = (*s)[snake_ID]->get_corpos();
  pos_2d head_pos = (*snake_target)[0]->get_posicao();
  std::vector<int> snakes_in_game(0);

  for (int i = 0; i < s->size(); i++){
    if (snakes_status[i]  && i != snake_ID)
      snakes_in_game.push_back(i);
  }

  for (int i = 0; i < snakes_in_game.size(); i++){
    std::vector<Corpo *> *c = (*s)[snakes_in_game[i]]->get_corpos();
    for (int k = 0; k < c->size(); k++){
      pos_2d corpo_pos = (*c)[k]->get_posicao();

      if (head_pos.x == corpo_pos.x && head_pos.y == corpo_pos.y){
        if (k == 0)
          return snakes_in_game[i];
        else
          return -1;
      }
    }
  }

  return -3;
}

void Fisica::change_dir(int direction, int i) {
  vel_2d new_vel;
  vel_2d last_vel;
  
  switch (direction){
    case 0:
      new_vel.x = 0;
      new_vel.y = VEL;
      break;
    case 1:
      new_vel.x = -VEL;
      new_vel.y = 0;
      break;
    case 2:
      new_vel.x = 0;
      new_vel.y = -VEL;  
      break;
    case 3:
      new_vel.x = VEL;
      new_vel.y = 0;
      break;
    default:
      return;
  }

  std::vector<Snake *> *s = this->lista->get_snakes();
  std::vector<Corpo *> *c = (*s)[i]->get_corpos();
  last_vel = (*c)[0]->get_velocidade();

  // do not turn around
  if ((new_vel.x < 0 && last_vel.x > 0) || (new_vel.x > 0 && last_vel.x < 0))
    return;
  else if ((new_vel.y < 0 && last_vel.y > 0) || (new_vel.y > 0 && last_vel.y < 0))
    return;
  else
    (*c)[0]->update(new_vel, (*c)[0]->get_posicao());
}