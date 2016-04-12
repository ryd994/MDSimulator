#include <cstdlib>
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>
#include <random>
#include <pthread.h>

#define DIMENSION 2

using std::vector;
using std::cout;

typedef vector<double> coordinate_vector;

double potentials(const vector<coordinate_vector> &pos_mat) {
  double pot = 0;
  for (unsigned int i=0; i < pos_mat.size(); ++i) {
    for (unsigned int j=i+1; j < pos_mat.size(); ++j) {
      double dist2 = 0.0;
      for (unsigned int k=0; k<DIMENSION; ++k) {
        double d = pos_mat[i][k]-pos_mat[j][k];
        dist2 += d*d;
      }
      pot += 4*LJEplison * ( pow(LJSigma, 12)/pow(dist2, 6) - pow(LJSigma, 6)/pow(dist2, 3) );
    }
  }
  return pot;
}

struct force_thread_arg_t {
  vector<coordinate_vector> pos_mat;
  double orig_pot;
  unsigned int range_begin, range_end;
  vector<coordinate_vector> *result_vec_p;
};

void* force_thread(void *args_p) {
  struct force_thread_arg_t * args = (struct force_thread_arg_t *)args_p;
  vector<coordinate_vector> &pos_mat = args->pos_mat;
  double orig_pot(args->orig_pot);
  vector<coordinate_vector> &result_vec = *(args->result_vec_p);

  for (unsigned int i=args->range_begin; i<args->range_end; ++i) {
    for (unsigned int j=0; j<DIMENSION; ++j) {
      double orig_pos = pos_mat[i][j];
      pos_mat[i][j] += Delta;
      double new_pot = potentials(pos_mat);
      pos_mat[i][j]  = orig_pos;
      result_vec[i][j] = (orig_pot-new_pot)/Delta;
    }
  }
  return NULL;
}

vector<coordinate_vector> acceleration(const vector<coordinate_vector> &pos_mat,
                                       const vector<double> &mass_mat) {
  double orig_pot = potentials(pos_mat);

  // Allocate result space
  vector<coordinate_vector> result_vec(pos_mat.size());
  for (unsigned int i=0; i<pos_mat.size(); ++i) 
    result_vec[i].resize(DIMENSION);
  
  // Prepare arguments for each thread
  unsigned int BatchSize = pos_mat.size()/NThread;
  vector<force_thread_arg_t> force_thread_args(NThread);
  for (unsigned int i=0; i<force_thread_args.size(); ++i) {
    force_thread_args[i].pos_mat  = pos_mat;
    force_thread_args[i].orig_pot = orig_pot;
    force_thread_args[i].range_begin = i*BatchSize;
    force_thread_args[i].range_end = (i+1)*BatchSize;
    force_thread_args[i].result_vec_p = &result_vec;
  }

  // Add all leftover atoms to the last thread
  force_thread_args.back().range_end = pos_mat.size();

  // Spawn threads
  pthread_t children[NThread];
  for (unsigned int i=0; i<NThread; ++i) {
    pthread_create(&children[i], NULL, *force_thread, &force_thread_args[i]);
  }

  // Join threads
  for (unsigned int i=0; i<NThread; ++i) {
    pthread_join(children[i], NULL);
  }
  
  for (unsigned int i=0; i<result_vec.size(); ++i) {
    for (unsigned int j=0; j<DIMENSION; ++j) {
      result_vec[i][j] /= mass_mat[i];
    }
  }
  return result_vec;
}

void velocity_verlet(vector<coordinate_vector> &pos_mat,
                     vector<coordinate_vector> &vel_mat,
                     const vector<double>      &mass_mat) {
  //Step 1: pos_mat += vel_mat*TimeStep + accel_mat*TimeStep*TimeStep/2
  vector<coordinate_vector> accel_mat = acceleration(pos_mat, mass_mat);
  for (unsigned int i=0; i<pos_mat.size(); ++i) {
    for (unsigned int j=0; j<DIMENSION; ++j) {
      pos_mat[i][j] += vel_mat[i][j]*TimeStep + accel_mat[i][j]*TimeStep*TimeStep/2;
      if (abs(pos_mat[i][j]) > BoxSize) {
        vel_mat[i][j] = -vel_mat[i][j];
      }
    }
  }

  //Step 2: new_half_vel = vel_mat + accel_mat*TimeStep/2
  vector<coordinate_vector> new_half_vel(vel_mat.size());
  for (unsigned int i=0; i<new_half_vel.size(); ++i) {
    new_half_vel[i].reserve(DIMENSION);
    for (unsigned int j=0; j<DIMENSION; ++j) {
      new_half_vel[i].push_back(vel_mat[i][j] + accel_mat[i][j]*TimeStep/2);
    }
  }

  //Step 3: new_vel_mat = vel_mat + (accel_mat+new_accel_mat)*TimeStep/2
  vector<coordinate_vector> new_accel_mat = acceleration(pos_mat, mass_mat);
  for (unsigned int i=0; i<vel_mat.size(); ++i) {
    for (unsigned int j=0; j<DIMENSION; ++j) {
      vel_mat[i][j] += (accel_mat[i][j]+new_accel_mat[i][j])*TimeStep/2;
    } 
  }
}

double energy(const vector<coordinate_vector> &pos_mat,
              const vector<coordinate_vector> &vel_mat,
              const vector<double>            &mass_mat) {
  double ener = 0;
  for (unsigned int i=0; i<vel_mat.size(); ++i) {
    double v2 = 0;
    for (unsigned int j=0; j<DIMENSION; ++j) {
      v2 += vel_mat[i][j]*vel_mat[i][j];
    }
    ener += mass_mat[i]*v2;
  }
  ener /= 2;
  ener += potentials(pos_mat);
  return ener;
}

void print_pos(const vector<coordinate_vector> &pos_mat,
               double energy,
               unsigned int step) {
  cout <<pos_mat.size() <<'\n'
       <<"Step: " <<step <<" Energy: " <<energy <<'\n';
  for (unsigned int i=0; i<pos_mat.size(); ++i) {
    cout <<'A';
    for (unsigned int j=0; j<DIMENSION; ++j) {
      cout <<'\t' <<pos_mat[i][j];
    }
    cout <<'\n' <<std::flush;
  }
}

