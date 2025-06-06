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
   Contributing author: Zbigniew Koziol
   (National Center for Nuclear Research, Poland)
   e-mail: softquake at gmail dot com
   Writing this was based on C code of Kolmogorov-Crespi potential
   of Jaap Kroes and others.

   This is potential described in
   [Lebedeva et al., Physica E, 44(6), 949-954, 2012.]
------------------------------------------------------------------------- */

#include "pair_lebedeva_z.h"

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "info.h"
#include "memory.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "potential_file_reader.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;

static constexpr int DELTA = 4;

/* ---------------------------------------------------------------------- */

PairLebedevaZ::PairLebedevaZ(LAMMPS *lmp) : Pair(lmp)
{
  single_enable = 0;
  restartinfo = 0;
  manybody_flag = 1;
  centroidstressflag = CENTROID_NOTAVAIL;
  unit_convert_flag = utils::get_supported_conversions(utils::ENERGY);

  // initialize element to parameter maps
  params = nullptr;

  // always compute energy offset
  offset_flag = 1;
}

/* ---------------------------------------------------------------------- */

PairLebedevaZ::~PairLebedevaZ()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    memory->destroy(offset);
  }

  memory->destroy(params);
  memory->destroy(elem2param);
}

/* ---------------------------------------------------------------------- */

void PairLebedevaZ::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair,der;
  double rsq,r,rhosq,exp1,exp2,exp3,r6,r8;
  double sumD,Ulm,fxy,fz;
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = 0.0;
  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  int newton_pair = force->newton_pair;

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
      j &= NEIGHMASK;
      jtype = type[j];

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      // rho^2 = r^2 - z^2
      rhosq = delx*delx + dely*dely;
      rsq = rhosq + delz*delz;

      if (rsq < cutsq[itype][jtype]) {

        int iparam_ij = elem2param[map[itype]][map[jtype]];
        Param& p = params[iparam_ij];

        r = sqrt(rsq);
        r6 = rsq*rsq*rsq;
        r8 = r6*rsq;

        // store exponents
        exp1 = exp(-p.alpha*(r-p.z0));
        exp2 = exp(-p.lambda1*rhosq);
        exp3 = exp(-p.lambda2*(delz*delz-p.z02));
        sumD = 1+p.D1*rhosq+p.D2*rhosq*rhosq;
        Ulm  = -p.A*p.z06/r6+ p.B*exp1+p.C*sumD*exp2*exp3;

        // derivatives
        fpair = -6.0*p.A*p.z06/r8+p.B*p.alpha*exp1/r; // used for x,y,z
        der   = p.D1+2*p.D2*rhosq-p.lambda1*sumD; // used for x,y
        fxy   = 2*p.C*exp2*exp3*der;
        fz    = 2*p.C*p.lambda2*sumD*exp2*exp3;

        f[i][0] += delx*(fpair-fxy);
        f[i][1] += dely*(fpair-fxy);
        f[i][2] += delz*(fpair+fz);
        if (newton_pair || j < nlocal) {
          f[j][0] -= delx*(fpair-fxy);
          f[j][1] -= dely*(fpair-fxy);
          f[j][2] -= delz*(fpair+fz);
        }

        if (eflag) {
          evdwl = Ulm - offset[itype][jtype];
        }

        if (evflag) {
          ev_tally(i, j, nlocal, newton_pair, evdwl, 0.0, fpair, delx, dely, delz);
          if (vflag_either) {
            double fi[3],fj[3];
            fi[0] = -delx * fxy;
            fi[1] = -dely * fxy;
            fi[2] =  delz * fz;
            fj[0] =  delx * fxy;
            fj[1] =  dely * fxy;
            fj[2] = -delz * fz;
            v_tally2_newton(i,fi,x[i]);
            v_tally2_newton(j,fj,x[j]);
          }
        }
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairLebedevaZ::allocate()
{
  allocated = 1;
  int n = atom->ntypes+1;

  memory->create(setflag,n,n,"pair:setflag");
  for (int i = 1; i < n; i++)
    for (int j = i; j < n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n,n,"pair:cutsq");
  memory->create(offset,n,n,"pair:offset");
  map = new int[n];
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairLebedevaZ::settings(int narg, char **arg)
{
  if (narg != 1) error->all(FLERR,"Illegal pair_style command");
  if (!utils::strmatch(force->pair_style,"^hybrid/overlay"))
    error->all(FLERR,"Pair style lebedeva/z requires using hybrid/overlay");

  cut_global = utils::numeric(FLERR,arg[0],false,lmp);
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairLebedevaZ::coeff(int narg, char **arg)
{
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi;
  utils::bounds(FLERR,arg[0],1,atom->ntypes,ilo,ihi,error);
  utils::bounds(FLERR,arg[1],1,atom->ntypes,jlo,jhi,error);

  map_element2type(narg-3,arg+3,false);
  read_file(arg[2]);

  // set setflag only for i,j pairs where both are mapped to elements

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      if ((map[i] >= 0) && (map[j] >= 0)) {
        setflag[i][j] = 1;
        count++;
      }
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients" + utils::errorurl(21));
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairLebedevaZ::init_style()
{
  if (force->newton_pair == 0)
    error->all(FLERR,"Pair style lebedeva/z requires newton pair on");

  neighbor->add_request(this);
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairLebedevaZ::init_one(int i, int j)
{
  if (setflag[i][j] == 0)
    error->all(FLERR, Error::NOLASTLINE,
               "All pair coeffs are not set. Status\n" + Info::get_pair_coeff_status(lmp));
  if (!offset_flag)
    error->all(FLERR, Error::NOLASTLINE, "Must use 'pair_modify shift yes' with this pair style");

  if (offset_flag && (cut_global > 0.0)) {
    int iparam_ij = elem2param[map[i]][map[j]];
    Param& p = params[iparam_ij];
    offset[i][j] = -p.A*pow(p.z0/cut_global,6);
  } else offset[i][j] = 0.0;
  offset[j][i] = offset[i][j];

  return cut_global;
}

/* ----------------------------------------------------------------------
   read Lebedeva potential file
------------------------------------------------------------------------- */

void PairLebedevaZ::read_file(char *filename)
{
  memory->sfree(params);
  params = nullptr;
  nparams = maxparam = 0;

  // open file on proc 0

  if (comm->me == 0) {
    PotentialFileReader reader(lmp, filename, "lebedeva/z", unit_convert_flag);
    char *line;

    // transparently convert units for supported conversions

    int unit_convert = reader.get_unit_convert();
    double conversion_factor = utils::get_conversion_factor(utils::ENERGY, unit_convert);

    while ((line = reader.next_line(NPARAMS_PER_LINE))) {

      try {
        ValueTokenizer values(line);

        std::string iname = values.next_string();
        std::string jname = values.next_string();

        // ielement,jelement = 1st args
        // if both args are in element list, then parse this line
        // else skip to next entry in file
        int ielement, jelement;

        for (ielement = 0; ielement < nelements; ielement++)
          if (iname == elements[ielement]) break;
        if (ielement == nelements) continue;
        for (jelement = 0; jelement < nelements; jelement++)
          if (jname == elements[jelement]) break;
        if (jelement == nelements) continue;

        // expand storage, if needed

        if (nparams == maxparam) {
          maxparam += DELTA;
          params = (Param *) memory->srealloc(params,maxparam*sizeof(Param), "pair:params");

          // make certain all addional allocated storage is initialized
          // to avoid false positives when checking with valgrind

          memset(params + nparams, 0, DELTA*sizeof(Param));
        }

        // load up parameter settings and error check their values

        params[nparams].ielement = ielement;
        params[nparams].jelement = jelement;
        params[nparams].A        = values.next_double();
        params[nparams].B        = values.next_double();
        params[nparams].C        = values.next_double();
        params[nparams].z0       = values.next_double();
        params[nparams].alpha    = values.next_double();
        params[nparams].D1       = values.next_double();
        params[nparams].D2       = values.next_double();
        params[nparams].lambda1  = values.next_double();
        params[nparams].lambda2  = values.next_double();
        // S provides a convenient scaling of all energies
        params[nparams].S        = values.next_double();

      } catch (TokenizerException &e) {
        error->one(FLERR, e.what());
      }

      // energies in meV further scaled by S
      // S = 43.3634 meV = 1 kcal/mol

      double meV = 1e-3*params[nparams].S;
      if (unit_convert) meV *= conversion_factor;

      params[nparams].A *= meV;
      params[nparams].B *= meV;
      params[nparams].C *= meV;

      // precompute some quantities. That speeds up later process
      params[nparams].z02 = pow(params[nparams].z0,2);
      params[nparams].z06 = pow(params[nparams].z0,6);

      nparams++;
      if (nparams >= pow((double)atom->ntypes,3)) break;
    }
  }

  MPI_Bcast(&nparams, 1, MPI_INT, 0, world);
  MPI_Bcast(&maxparam, 1, MPI_INT, 0, world);

  if (comm->me != 0) {
    params = (Param *) memory->srealloc(params,maxparam*sizeof(Param), "pair:params");
  }

  MPI_Bcast(params, maxparam*sizeof(Param), MPI_BYTE, 0, world);

  memory->destroy(elem2param);
  memory->create(elem2param,nelements,nelements,"pair:elem2param");
  for (int i = 0; i < nelements; i++) {
    for (int j = 0; j < nelements; j++) {
      int n = -1;
      for (int m = 0; m < nparams; m++) {
        if (i == params[m].ielement && j == params[m].jelement) {
          if (n >= 0) error->all(FLERR,"Potential file has a duplicate entry for: {} {}",
                                 elements[i], elements[j]);
          n = m;
        }
      }
      if (n < 0) error->all(FLERR,"Potential file is missing an entry for: {} {}",
                            elements[i], elements[j]);
      elem2param[i][j] = n;
    }
  }
}

/* ---------------------------------------------------------------------- */
