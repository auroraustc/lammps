// clang-format off

/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Paolo Raiteri (Curtin University)
------------------------------------------------------------------------- */

#include "pair_lennard_mdf.h"

#include "atom.h"
#include "comm.h"
#include "force.h"
#include "info.h"
#include "neigh_list.h"
#include "memory.h"
#include "error.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

PairLennardMDF::PairLennardMDF(LAMMPS *lmp) : Pair(lmp) {}

/* ---------------------------------------------------------------------- */

PairLennardMDF::~PairLennardMDF()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);

    memory->destroy(cut);
    memory->destroy(cut_inner);
    memory->destroy(cut_inner_sq);
    memory->destroy(aparm);
    memory->destroy(bparm);
    memory->destroy(lj1);
    memory->destroy(lj2);
    memory->destroy(lj3);
    memory->destroy(lj4);
  }
}

/* ---------------------------------------------------------------------- */

void PairLennardMDF::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair;
  double rsq,r2inv,r6inv,forcelj,factor_lj;
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = 0.0;
  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  double *special_lj = force->special_lj;
  int newton_pair = force->newton_pair;
  double rr, d, dd, tt, dt, dp, philj;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // loop over neighbors of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];

      if (rsq < cutsq[itype][jtype]) {
        r2inv = 1.0/rsq;
        r6inv = r2inv*r2inv*r2inv;
        forcelj = r6inv * (lj1[itype][jtype]*r6inv - lj2[itype][jtype]);

        if (rsq > cut_inner_sq[itype][jtype]) {
          philj = r6inv*(lj3[itype][jtype]*r6inv-lj4[itype][jtype]);

          rr = sqrt(rsq);
          dp = (cut[itype][jtype] - cut_inner[itype][jtype]);
          d = (rr-cut_inner[itype][jtype]) / dp;
          dd = 1.-d;
// taperig function - mdf style
          tt = (1. + 3.*d + 6.*d*d)*dd*dd*dd;
// minus the derivative of the tapering function
          dt = 30.* d*d * dd*dd * rr / dp;

          forcelj = forcelj*tt + philj*dt;
        } else {
          tt = 1;
        }
        fpair = factor_lj*forcelj*r2inv;

        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;
        if (newton_pair || j < nlocal) {
          f[j][0] -= delx*fpair;
          f[j][1] -= dely*fpair;
          f[j][2] -= delz*fpair;
        }

        if (eflag) {
          evdwl = r6inv * (lj3[itype][jtype]*r6inv - lj4[itype][jtype]);
          if (rsq > cut_inner_sq[itype][jtype]) evdwl *= tt;

          evdwl *= factor_lj;

          if (evflag) ev_tally(i,j,nlocal,newton_pair,
                             evdwl,0.0,fpair,delx,dely,delz);
        }
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairLennardMDF::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  memory->create(cut,n+1,n+1,"pair:cut");
  memory->create(cut_inner,n+1,n+1,"pair:cut_inner");
  memory->create(cut_inner_sq,n+1,n+1,"pair:cut_inner_sq");
  memory->create(aparm,n+1,n+1,"pair:aparm");
  memory->create(bparm,n+1,n+1,"pair:bparm");
  memory->create(lj1,n+1,n+1,"pair:lj1");
  memory->create(lj2,n+1,n+1,"pair:lj2");
  memory->create(lj3,n+1,n+1,"pair:lj3");
  memory->create(lj4,n+1,n+1,"pair:lj4");
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairLennardMDF::settings(int narg, char **arg)
{
  if (narg != 2) error->all(FLERR,"Illegal pair_style command");

  cut_inner_global = utils::numeric(FLERR,arg[0],false,lmp);
  cut_global = utils::numeric(FLERR,arg[1],false,lmp);

  if (cut_inner_global <= 0.0 || cut_inner_global > cut_global)
    error->all(FLERR,"Illegal pair_style command");

  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i; j <= atom->ntypes; j++)
        if (setflag[i][j]) {
          cut_inner[i][j] = cut_inner_global;
          cut[i][j] = cut_global;
        }
  }
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairLennardMDF::coeff(int narg, char **arg)
{
  if (narg != 4 && narg != 6)
    error->all(FLERR,"Incorrect args for pair coefficients" + utils::errorurl(21));
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi;
  utils::bounds(FLERR,arg[0],1,atom->ntypes,ilo,ihi,error);
  utils::bounds(FLERR,arg[1],1,atom->ntypes,jlo,jhi,error);

  double aparm_one = utils::numeric(FLERR,arg[2],false,lmp);
  double bparm_one = utils::numeric(FLERR,arg[3],false,lmp);

  double cut_inner_one = cut_inner_global;
  double cut_one = cut_global;
  if (narg == 6) {
    cut_inner_one = utils::numeric(FLERR,arg[4],false,lmp);
    cut_one = utils::numeric(FLERR,arg[5],false,lmp);
  }
  if (cut_inner_one <= 0.0 || cut_inner_one > cut_one)
    error->all(FLERR,"Illegal pair_coeff command");

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      aparm[i][j] = aparm_one;
      bparm[i][j] = bparm_one;
      cut_inner[i][j] = cut_inner_one;
      cut[i][j] = cut_one;
      setflag[i][j] = 1;
      count++;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients" + utils::errorurl(21));
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairLennardMDF::init_one(int i, int j)
{
  if (setflag[i][j] == 0)
    error->all(FLERR, Error::NOLASTLINE,
               "All pair coeffs are not set. Status:\n" + Info::get_pair_coeff_status(lmp));

  cut_inner_sq[i][j] = cut_inner[i][j]*cut_inner[i][j];

  lj1[i][j] = 12.0 * aparm[i][j];
  lj2[i][j] =  6.0 * bparm[i][j];
  lj3[i][j] = aparm[i][j];
  lj4[i][j] = bparm[i][j];

  cut[j][i] = cut[i][j];
  cut_inner[j][i] = cut_inner[i][j];
  cut_inner_sq[j][i] = cut_inner_sq[i][j];
  lj1[j][i] = lj1[i][j];
  lj2[j][i] = lj2[i][j];
  lj3[j][i] = lj3[i][j];
  lj4[j][i] = lj4[i][j];

  return cut[i][j];
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairLennardMDF::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        fwrite(&aparm[i][j],sizeof(double),1,fp);
        fwrite(&bparm[i][j],sizeof(double),1,fp);
        fwrite(&cut_inner[i][j],sizeof(double),1,fp);
        fwrite(&cut[i][j],sizeof(double),1,fp);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairLennardMDF::read_restart(FILE *fp)
{
  read_restart_settings(fp);
  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) utils::sfread(FLERR,&setflag[i][j],sizeof(int),1,fp,nullptr,error);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) {
        if (me == 0) {
          utils::sfread(FLERR,&aparm[i][j],sizeof(double),1,fp,nullptr,error);
          utils::sfread(FLERR,&bparm[i][j],sizeof(double),1,fp,nullptr,error);
          utils::sfread(FLERR,&cut_inner[i][j],sizeof(double),1,fp,nullptr,error);
          utils::sfread(FLERR,&cut[i][j],sizeof(double),1,fp,nullptr,error);
        }
        MPI_Bcast(&aparm[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&bparm[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut_inner[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut[i][j],1,MPI_DOUBLE,0,world);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairLennardMDF::write_restart_settings(FILE *fp)
{
  fwrite(&mix_flag,sizeof(int),1,fp);
  fwrite(&cut_global,sizeof(double),1,fp);
  fwrite(&cut_inner_global,sizeof(double),1,fp);
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairLennardMDF::read_restart_settings(FILE *fp)
{
  int me = comm->me;
  if (me == 0) {
    utils::sfread(FLERR,&mix_flag,sizeof(int),1,fp,nullptr,error);
    utils::sfread(FLERR,&cut_global,sizeof(double),1,fp,nullptr,error);
    utils::sfread(FLERR,&cut_inner_global,sizeof(double),1,fp,nullptr,error);
  }
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world);
  MPI_Bcast(&cut_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&cut_inner_global,1,MPI_DOUBLE,0,world);
}

/* ----------------------------------------------------------------------
   proc 0 writes to data file
------------------------------------------------------------------------- */

void PairLennardMDF::write_data(FILE *fp)
{
  for (int i = 1; i <= atom->ntypes; i++)
    fprintf(fp,"%d %g %g\n",i,aparm[i][i],bparm[i][i]);
}

/* ----------------------------------------------------------------------
   proc 0 writes all pairs to data file
------------------------------------------------------------------------- */

void PairLennardMDF::write_data_all(FILE *fp)
{
  for (int i = 1; i <= atom->ntypes; i++)
    for (int j = i; j <= atom->ntypes; j++)
      fprintf(fp,"%d %d %g %g %g %g\n",
              i,j,aparm[i][j],bparm[i][j],
              cut_inner[i][j],cut[i][j]);
}

/* ---------------------------------------------------------------------- */

double PairLennardMDF::single(int /*i*/, int /*j*/, int itype, int jtype,
                             double rsq,
                             double /*factor_coul*/, double factor_lj,
                             double &fforce)
{
  double r2inv,r6inv,forcelj,philj;
  double rr, dp, d, tt, dt, dd;

  r2inv = 1.0/rsq;
  r6inv = r2inv*r2inv*r2inv;

  philj = r6inv*(lj3[itype][jtype]*r6inv-lj4[itype][jtype]);
  forcelj = r6inv * (lj1[itype][jtype]*r6inv - lj2[itype][jtype]);

  if (rsq > cut_inner_sq[itype][jtype]) {

    rr = sqrt(rsq);
    dp = (cut[itype][jtype] - cut_inner[itype][jtype]);
    d = (rr - cut_inner[itype][jtype]) / dp;
    dd = 1-d;
    tt = (1. + 3.*d + 6.*d*d)* dd*dd*dd;
    dt = 30.* d*d * dd*dd * rr / dp;

    forcelj = forcelj*tt + philj*dt;
    philj *= tt;
  }

  fforce = factor_lj*forcelj*r2inv;

  return factor_lj*philj;
}

/* ---------------------------------------------------------------------- */

void *PairLennardMDF::extract(const char *str, int &dim)
{
  dim = 2;
  if (strcmp(str,"a") == 0) return (void *) aparm;
  if (strcmp(str,"b") == 0) return (void *) bparm;
  return nullptr;
}
