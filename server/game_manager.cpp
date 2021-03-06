#include "../include/model/snake_model.hpp"
#include "../include/view/snake_view.hpp"
#include "../include/controler/snake_controler.hpp"
#include "../include/controler/keyboard_controler.hpp"

#include "../serial/serializable.hpp"
#include "../include/client/remote_keyboard.hpp"

#include "../include/server/game_manager.hpp"

std::mutex player_key;
int SNAKE_MAX;

bool game_run (int portno, int socket_fd, struct sockaddr_in myself, Tela *tela){

  ListaDeSnakes *snake_list = new ListaDeSnakes();
  pos_2d p = {4, (float)LINES - 1};
  for(int i = 0; i < SNAKE_MAX; i++){
    Snake *snake = create_snake(4, p);
    snake_list->add_snake(snake);
    p.y-= (int)(LINES-1)/SNAKE_MAX;
    if (p.y < 0)
      error((char *)"SNAKE_MAX is too large\n");
  }
  Fisica *physic = new Fisica(snake_list);
  
  tela->catch_param(snake_list, &physic->food_vector);

  bool thread_running[SNAKE_MAX];
  int connection_fd[SNAKE_MAX];

  for (int i = 0; i < SNAKE_MAX; i++)
    thread_running[i] = false;

  std::vector<std::thread> connection_thread(SNAKE_MAX);

  plyr_data args;

  for (int i = 0; i < FOOD_AMOUNT_AT_BEGIN; i++){
    physic->feed_snake();
  }
    
  args.socket = socket_fd;
  args.port = portno;
  args.connection_fd = connection_fd;
  args.myself = myself;
  args.physic = physic;
  args.snake_list = snake_list;
  args.thread_running = thread_running;

  for (int i = 0; i < SNAKE_MAX; i++){
    args.snake_ID = i;
    std::thread new_thread(player_management, args);
    connection_thread[i].swap(new_thread);
  }

  print_msg(0,0, (char *)"Waiting PLayers...", true);

  // Wait for all players
  client::Teclado *my_keyboard = new client::Teclado();
  my_keyboard->init(false);
  bool terminate = false;
  int j = 0;
  while(j < SNAKE_MAX){
    if (thread_running[j] == true)
      j++;

    if (j == 0){
      int c = my_keyboard->getchar();
      if (c == 27){
        terminate = true;
        break;
      }
    }
  }

  if (terminate){
    print_msg((int)LINES/2, -10 + (int)COLS/2, (char *)"Exiting...", true);
    std::this_thread::sleep_for (std::chrono::milliseconds(100));

    shutdown(socket_fd, SHUT_RDWR);
    for (int i = 0; i < SNAKE_MAX; i++){
      connection_thread[i].join();
    }

    return false;
  }

  int snakes_in_game;
  do{
    snakes_in_game = 0;
    for (int i = 0; i < SNAKE_MAX; i++){
      if (thread_running[i])
        snakes_in_game++;
    }

    if (snakes_in_game){
      player_key.lock();
      tela->update(thread_running);
      player_key.unlock();
    }

    std::this_thread::sleep_for (std::chrono::milliseconds(100));
  }while(snakes_in_game > 0);

  for (int i = 0; i < SNAKE_MAX; i++){
    connection_thread[i].join();
  }

  //my_keyboard->stop();
  std::this_thread::sleep_for (std::chrono::milliseconds(6000));
  return true;
}

void player_management(plyr_data args){
  int socket_fd = args.socket, port = args.port, *connection_fd = args.connection_fd;
  struct sockaddr_in myself = args.myself;
  int snake_ID = args.snake_ID;
  Fisica *physic = args.physic;
  ListaDeSnakes *snake_list = args.snake_list;
  bool *thread_running = args.thread_running;

  struct sockaddr_in client;
  socklen_t client_size = (socklen_t)sizeof(client);
  
  connection_fd[snake_ID] = accept(socket_fd, (struct sockaddr*)&client, &client_size);

  if (connection_fd[snake_ID]  == -1)
    return;

  // begin keyboard interface
  Teclado *teclado = new Teclado();
  teclado->init();
  teclado->get_server(port, socket_fd, connection_fd[snake_ID], myself, client);
  
  int impulse = 0; // speed up snake
  int food_counter = 0;
  int interation = 0;
  
  std::vector<Snake *> *snake_vector = snake_list->get_snakes();
  std::vector<Corpo*> *this_snake = (*snake_vector)[snake_ID]->get_corpos();

  RelevantData *data = new RelevantData();

  thread_running[snake_ID] = true;

  // Wait for all players
  int i = 0;
  while(i < SNAKE_MAX){
    if (thread_running[i] == true)
      i++;
  }

  std::this_thread::sleep_for (std::chrono::milliseconds(500));

  short int ID = snake_ID + 1;
  send(connection_fd[snake_ID], &ID, sizeof(short int), 0);
  
  std::this_thread::sleep_for (std::chrono::milliseconds(3000));

  while (thread_running[snake_ID]) {
    char buffer[2000000];
    
    player_key.lock();
    short int update_value = physic->update(snake_ID, thread_running);
    player_key.unlock();

    if (update_value == -4){ // Snake ate
      food_counter++;
      player_key.lock();
      physic->feed_snake();
      player_key.unlock();
    }
    if (update_value >= -2){ // Snake lose
      thread_running[snake_ID] = false;
      
      pos_2d end_signal = {-1,-1};

      if (update_value == -1)
        end_signal.y = -2;

      data->PutData(end_signal);
      data->serialize(buffer);
      send(connection_fd[snake_ID], buffer, data->get_data_size(), 0);

      if(update_value >= 0){
        player_key.lock();
        if (thread_running[update_value]){
          send(connection_fd[update_value], buffer, data->get_data_size(), 0);
          thread_running[update_value] = false;
        }
        player_key.unlock();
      }
      break;
    }
    if (update_value == -5){ // Snake won
      pos_2d end_signal = {-10,10};

      data->PutData(end_signal);
      data->serialize(buffer);
      send(connection_fd[snake_ID], buffer, data->get_data_size(), 0);

      thread_running[snake_ID] = false;
      break;
    }
    
    player_key.lock();
    for(int i = 0; i < SNAKE_MAX; i++){
      if (thread_running[i])
        data->PutData((*snake_vector)[i]->get_corpos(), SNAKE1_PAIR + i);
    }
    for(int i = 0; i < physic->food_vector.size(); i++){
      data->PutData(physic->food_vector[i]);
    }
    player_key.unlock();

    data->PutData(food_counter);
    data->serialize(buffer);
    send(connection_fd[snake_ID], buffer, data->get_data_size(), 0);
    data->clean();

    // read keys from keyboard
    int c = teclado->getchar();
    if (c > 0){
      if (keyboard_map(c, snake_ID, physic, &impulse) == false){
        thread_running[snake_ID] = false;
      }
    }

    if (interation > 40)
      impulse = 0;

    pos_2d this_snake_vel = (*this_snake)[0]->get_velocidade();
    std::this_thread::sleep_for (std::chrono::milliseconds(100 - impulse));
    if (this_snake_vel.y != 0)
      std::this_thread::sleep_for (std::chrono::milliseconds(50));


    if (impulse || interation){
      if (interation == 40)
        impulse = 0;
      else if (interation == 50)
        interation = -1;
      interation++;
    }
  }

  teclado->stop();
  return;
}

bool keyboard_map(int c, int snake_ID, Fisica *f, int *impulse){
  switch (c){
    case KEY_DOWN:
      // head goes down
      f->change_dir(0,snake_ID);
      break;
    case KEY_LEFT:
      // head goes left
      f->change_dir(1,snake_ID);
      break;
    case KEY_UP:
      // head goes up
      f->change_dir(2,snake_ID);
      break;
    case KEY_RIGHT:
      // head goes right
      f->change_dir(3,snake_ID);
      break;
    case ' ':
      // speed up snake
      if (!(*impulse))
        *impulse = 50;
      break;
    case 27:
      // term/inate game
      return false;
  }
  return true;
}

Snake *create_snake(unsigned int length, pos_2d p){
  vel_2d v = {(float)VEL,0};

  Snake *snake = new Snake();
  for (int i =0; i < length; i++){
    Corpo *c = new Corpo(v, p);
    snake->add_corpo(c);
    p.x-=1;
  }

  return snake;
}

int init_server(int portno, int &socket_fd, struct sockaddr_in &myself, char *IP){
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  /*Create socket*/
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) 
    error((char*)"ERROR opening socket");
  
  bzero((char *) &myself, sizeof(myself));/*set all values into the struct to 0*/
  
  myself.sin_family = AF_INET;
  inet_aton(IP, &(myself.sin_addr));/*Store IP Address*/
  myself.sin_port = htons(portno);/*Convert portno into network bytes order*/
  
  if (bind(socket_fd, (struct sockaddr*)&myself, sizeof(myself)) != 0) {
    return -1;
  }

  listen(socket_fd, 2);
  return 0;
}

void error(char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_msg(int line, int collum, char *msg, bool clr){
  if (clr) clear();
  attron(COLOR_PAIR(MSG_PAIR));
  move(line, collum);
  printw(msg);
  attroff(COLOR_PAIR(MSG_PAIR));
  refresh();
  return;
}