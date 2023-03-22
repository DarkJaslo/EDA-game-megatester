#include <iostream>
#include <vector>
#include <random>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <stdio.h>
#include <map>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <queue>
using namespace std;

const char combs_filename[] = "combs.test";

struct Game{
  vector<int> players;
  vector<int> points;
  char filename[64];

  Game(){
    players = vector<int>(4);
    points = vector<int>(4,0);
  }
};

void abort(){
  remove(combs_filename);
  exit(1);
}

void read_matches(vector<string>& matches){
  ifstream file;
  file.open(combs_filename);

  if(file.fail()){
    cerr << "Error, could not open file " << combs_filename << endl;
    exit(420);
  }

  for(int i = 0; i < matches.size(); ++i){
    getline(file,matches[i]);
  }
  file.close();
  remove(combs_filename);
}

bool winner(Game& g){
  ifstream file;
  file.open(g.filename);

  if(file.fail()){
    cout << "Error abriendo el fichero " << g.filename << endl;
    exit(420);
  }

  string line;

  while(not file.eof()){
      
    getline(file,line);

    if(line == "round 200"){
      getline(file,line); //Basura
      getline(file,line);
      istringstream ss(line);
      string aux;
      ss >> aux;
      vector<int> scores(4);
      vector<int> ordscores(4);
      vector<bool> assigned(4,false);
      int imax = -1;
      int max = 0;
      for(int i = 0; i < 4; ++i){
        string score;
        //cout << score;
        ss >> score;
        scores[i] = stoi(score);
        ordscores[i] = scores[i];
        if(scores[i] > max){
            max = scores[i];
            imax = i;
        }
      }
      sort(ordscores.begin(), ordscores.end());

      for(int i = 0; i <= 3; ++i){
        int j = 0;
        while(1){
          if(scores[j] == ordscores[i]){
            g.points[j] += i+1;
            break;
          }
          ++j;
        }
      }

      file.close();
      return true;
    } 
  }
  cout << "Game crashed ";
  file.close();
  return false;
}

int main(int argc, char ** argv){

  cout.setf(ios::fixed);
  cout.precision(1);

  int nplayers;
  cout << "Enter the number of bots you want to compare" << endl;
  cin >> nplayers;

  int fd[2];
  pipe(fd);

  int pid = fork();
  if(pid == 0){  //Child
    close(fd[0]);
    dup2(fd[1],STDERR_FILENO);
    int fd2 = open(combs_filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    dup2(fd2,1);
    char buf[10];
    sprintf(buf,"%d",nplayers);
    execlp("./combs","./combs",buf,NULL);
    exit(2);
  }
  else if(pid < 0){
    cerr << "FORK ERROR" << endl;
    exit(1);   
  }

  close(fd[1]);
  char buf[64];
  while(read(fd[0],buf,sizeof(buf))>0);
  while(waitpid(-1,NULL,0)>0);
  int ngames = atoi(buf);
  cout << "Running " << ngames << " games: do you agree? (y/n)" << endl;
  char c;
  cin >> c;
  if(c == 'n') exit(0);
  else if(c != 'y'){
    cout << "Unknown character." << endl;
    abort();
  }

  cout << "Enter the names of the bots" << endl;
  vector<string> bots(nplayers);
  for(int i = 0; i < bots.size(); ++i){
    cin >> bots[i];
  }
  vector<int> points(nplayers,0);

  vector<string> matches(ngames);
  read_matches(matches);

  int cores = std::thread::hardware_concurrency();
  if(argc == 2){ //if the amount of cores to use was specified
    cores = atoi(argv[1]);
  }
  if(cores <= 0){
    cout << "Error, please enter the number of CPU cores your machine has (probably, 4 to 8)" << endl;
    cin >> cores;
    if(cores <= 0) exit(1);
  }

  cout << "Running megatester in " << cores << " cores..." << endl;

  queue<Game> games;
  for(int i = 0; i < ngames; ++i){
    Game g;
    sprintf(g.filename,"t%d.test",i);
    istringstream ss(matches[i]);
    ss >> g.players[0];
    ss >> g.players[1];
    ss >> g.players[2];
    ss >> g.players[3];
    games.push(g);
  }

  map<int,Game> pending;
  int procs = 0;
  int cur_games = 0;
  srand(time(NULL));

  while(cur_games < ngames){
    if(procs < cores and not games.empty()){
      //Create and run game
      procs++;
      int seed = rand();
      char seedbuf[64];
      sprintf(seedbuf,"%d",seed);
      Game g = games.front();
      games.pop();

      int pid2 = fork();

      if(pid2 == -1){
        cerr << "FORK ERROR" << endl;
        exit(2);
      }
      else if(pid2 == 0){ //Child
        int fd3 = open(g.filename,O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        dup2(fd3,1);
        close(2);
        fd3 = open("default.cnf",O_RDONLY);
        dup2(fd3,0);
        execlp("./Game","./Game", bots[g.players[0]].c_str(),bots[g.players[1]].c_str(),bots[g.players[2]].c_str(),
              bots[g.players[3]].c_str(),"-s",seedbuf,NULL);
      }
      pending.insert(make_pair(pid2,g));
    }
    else{
      int st;
      int pid_child = waitpid(-1,&st,0);
      auto it = pending.find(pid_child);
      if (it == pending.end()){
        cerr << "DID NOT FIND GAME" << endl;
        cout << it->first << endl;
        exit(1);
      }
      bool w = winner(it->second);
      for(int i = 0; i < 4; ++i){
        points[it->second.players[i]] += it->second.points[i];
      }
      remove(it->second.filename);
      pending.erase(it);
      --procs;
      ++cur_games;
      cout << "Game " << cur_games << " played (" << (double(cur_games)/ngames)*100 << "%)" << endl;
    }
  }

  vector<bool> u(nplayers,false);
  int aux = 0;
  while(aux < nplayers){
    int max = 0;
    int imax = -1;
    for(int i = 0; i < points.size(); ++i){
      if(not u[i] and points[i] > max){
        max = points[i];
        imax = i;
      }
    }
    u[imax] = true;
    cout << bots[imax] << ": " << points[imax] << endl;
    aux++;
  }
}