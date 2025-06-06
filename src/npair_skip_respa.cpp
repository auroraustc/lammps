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

#include "npair_skip_respa.h"

#include "atom.h"
#include "error.h"
#include "my_page.h"
#include "neigh_list.h"
#include "neigh_request.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

template<int TRIM>
NPairSkipRespaTemp<TRIM>::NPairSkipRespaTemp(LAMMPS *lmp) : NPair(lmp) {}

/* ----------------------------------------------------------------------
   build skip list for subset of types from parent list
   iskip and ijskip flag which atom types and type pairs to skip
   this is for respa lists, copy the inner/middle values from parent
------------------------------------------------------------------------- */

template<int TRIM>
void NPairSkipRespaTemp<TRIM>::build(NeighList *list)
{
  int i, j, ii, jj, n, itype, jtype, jnum, joriginal, n_inner, n_middle;
  int *neighptr, *jlist, *neighptr_inner, *neighptr_middle;

  int *type = atom->type;
  tagint *molecule = atom->molecule;

  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;
  MyPage<int> *ipage = list->ipage;
  int molskip = list->molskip;

  int *ilist_skip = list->listskip->ilist;
  int *numneigh_skip = list->listskip->numneigh;
  int **firstneigh_skip = list->listskip->firstneigh;
  int inum_skip = list->listskip->inum;

  int *iskip = list->iskip;
  int **ijskip = list->ijskip;

  int *ilist_inner = list->ilist_inner;
  int *numneigh_inner = list->numneigh_inner;
  int **firstneigh_inner = list->firstneigh_inner;
  MyPage<int> *ipage_inner = list->ipage_inner;
  int *numneigh_inner_skip = list->listskip->numneigh_inner;
  int **firstneigh_inner_skip = list->listskip->firstneigh_inner;

  int *ilist_middle, *numneigh_middle, **firstneigh_middle;
  MyPage<int> *ipage_middle;
  int *numneigh_middle_skip, **firstneigh_middle_skip;
  int respamiddle = list->respamiddle;
  if (respamiddle) {
    ilist_middle = list->ilist_middle;
    numneigh_middle = list->numneigh_middle;
    firstneigh_middle = list->firstneigh_middle;
    ipage_middle = list->ipage_middle;
    numneigh_middle_skip = list->listskip->numneigh_middle;
    firstneigh_middle_skip = list->listskip->firstneigh_middle;
  }

  int inum = 0;
  ipage->reset();
  ipage_inner->reset();
  if (respamiddle) ipage_middle->reset();

  double **x = atom->x;
  double xtmp, ytmp, ztmp;
  double delx, dely, delz, rsq;
  double cutsq_custom = cutoff_custom * cutoff_custom;

  // loop over atoms in other list
  // skip I atom entirely if iskip is set for type[I]
  // skip I,J pair if ijskip is set for type[I],type[J]

  for (ii = 0; ii < inum_skip; ii++) {
    i = ilist_skip[ii];
    itype = type[i];
    if (!molskip && iskip[itype]) continue;

    if (TRIM) {
      xtmp = x[i][0];
      ytmp = x[i][1];
      ztmp = x[i][2];
    }

    n = n_inner = 0;
    neighptr = ipage->vget();
    neighptr_inner = ipage_inner->vget();
    if (respamiddle) {
      n_middle = 0;
      neighptr_middle = ipage_middle->vget();
    }

    // loop over parent outer rRESPA list

    jlist = firstneigh_skip[i];
    jnum = numneigh_skip[i];

    for (jj = 0; jj < jnum; jj++) {
      joriginal = jlist[jj];
      j = joriginal & NEIGHMASK;
      jtype = type[j];
      if (!molskip && ijskip[itype][jtype]) continue;
      if ((molskip == NeighRequest::INTRA) && (molecule[i] != molecule[j])) continue;
      if ((molskip == NeighRequest::INTER) && (molecule[i] == molecule[j])) continue;

      if (TRIM) {
        delx = xtmp - x[j][0];
        dely = ytmp - x[j][1];
        delz = ztmp - x[j][2];
        rsq = delx * delx + dely * dely + delz * delz;

        double cutsq_trim = (cutsq_custom > 0.0) ? cutsq_custom : cutneighsq[itype][jtype];
        if (rsq > cutsq_trim) continue;
      }

      neighptr[n++] = joriginal;
    }

    // loop over parent inner rRESPA list

    jlist = firstneigh_inner_skip[i];
    jnum = numneigh_inner_skip[i];

    for (jj = 0; jj < jnum; jj++) {
      joriginal = jlist[jj];
      j = joriginal & NEIGHMASK;
      jtype = type[j];
      if (!molskip && ijskip[itype][jtype]) continue;
      if ((molskip == NeighRequest::INTRA) && (molecule[i] != molecule[j])) continue;
      if ((molskip == NeighRequest::INTER) && (molecule[i] == molecule[j])) continue;

      if (TRIM) {
        delx = xtmp - x[j][0];
        dely = ytmp - x[j][1];
        delz = ztmp - x[j][2];
        rsq = delx * delx + dely * dely + delz * delz;

        double cutsq_trim = (cutsq_custom > 0.0) ? cutsq_custom : cut_inner_sq;
        if (rsq > cutsq_trim) continue;
      }

      neighptr_inner[n_inner++] = joriginal;
    }

    // loop over parent middle rRESPA list

    if (respamiddle) {
      jlist = firstneigh_middle_skip[i];
      jnum = numneigh_middle_skip[i];

      for (jj = 0; jj < jnum; jj++) {
        joriginal = jlist[jj];
        j = joriginal & NEIGHMASK;
        jtype = type[j];
        if (!molskip && ijskip[itype][jtype]) continue;
        if ((molskip == NeighRequest::INTRA) && (molecule[i] != molecule[j])) continue;
        if ((molskip == NeighRequest::INTER) && (molecule[i] == molecule[j])) continue;

        if (TRIM) {
          delx = xtmp - x[j][0];
          dely = ytmp - x[j][1];
          delz = ztmp - x[j][2];
          rsq = delx * delx + dely * dely + delz * delz;

          double cutsq_trim = (cutsq_custom > 0.0) ? cutsq_custom : cut_middle_sq;
          if (rsq > cutsq_trim) continue;
        }

        neighptr_middle[n_middle++] = joriginal;
      }
    }

    ilist[inum] = i;
    firstneigh[i] = neighptr;
    numneigh[i] = n;
    ipage->vgot(n);
    if (ipage->status()) error->one(FLERR, Error::NOLASTLINE, "Neighbor list overflow, boost neigh_modify one" + utils::errorurl(36));

    ilist_inner[inum] = i;
    firstneigh_inner[i] = neighptr_inner;
    numneigh_inner[i] = n_inner;
    ipage_inner->vgot(n);
    if (ipage_inner->status()) error->one(FLERR, Error::NOLASTLINE, "Neighbor list overflow, boost neigh_modify one" + utils::errorurl(36));

    if (respamiddle) {
      ilist_middle[inum] = i;
      firstneigh_middle[i] = neighptr_middle;
      numneigh_middle[i] = n_middle;
      ipage_middle->vgot(n);
      if (ipage_middle->status()) error->one(FLERR, Error::NOLASTLINE, "Neighbor list overflow, boost neigh_modify one" + utils::errorurl(36));
    }

    inum++;
  }

  list->inum = inum;
  list->inum_inner = inum;
  if (respamiddle) list->inum_middle = inum;
}

namespace LAMMPS_NS {
template class NPairSkipRespaTemp<0>;
template class NPairSkipRespaTemp<1>;
}
