#ifdef __cplusplus
extern "C" {
#endif
#include "lmp_f2c.h"
int dorgl2_(integer *m, integer *n, integer *k, doublereal *a, integer *lda, doublereal *tau,
            doublereal *work, integer *info)
{
    integer a_dim1, a_offset, i__1, i__2;
    doublereal d__1;
    integer i__, j, l;
    extern int dscal_(integer *, doublereal *, doublereal *, integer *),
        xerbla_(char *, integer *, ftnlen),
        dlarf1f_(char *, integer *, integer *, doublereal *, integer *, doublereal *, doublereal *,
                 integer *, doublereal *, ftnlen);
    a_dim1 = *lda;
    a_offset = 1 + a_dim1;
    a -= a_offset;
    --tau;
    --work;
    *info = 0;
    if (*m < 0) {
        *info = -1;
    } else if (*n < *m) {
        *info = -2;
    } else if (*k < 0 || *k > *m) {
        *info = -3;
    } else if (*lda < max(1, *m)) {
        *info = -5;
    }
    if (*info != 0) {
        i__1 = -(*info);
        xerbla_((char *)"DORGL2", &i__1, (ftnlen)6);
        return 0;
    }
    if (*m <= 0) {
        return 0;
    }
    if (*k < *m) {
        i__1 = *n;
        for (j = 1; j <= i__1; ++j) {
            i__2 = *m;
            for (l = *k + 1; l <= i__2; ++l) {
                a[l + j * a_dim1] = 0.;
            }
            if (j > *k && j <= *m) {
                a[j + j * a_dim1] = 1.;
            }
        }
    }
    for (i__ = *k; i__ >= 1; --i__) {
        if (i__ < *n) {
            if (i__ < *m) {
                i__1 = *m - i__;
                i__2 = *n - i__ + 1;
                dlarf1f_((char *)"R", &i__1, &i__2, &a[i__ + i__ * a_dim1], lda, &tau[i__],
                         &a[i__ + 1 + i__ * a_dim1], lda, &work[1], (ftnlen)1);
            }
            i__1 = *n - i__;
            d__1 = -tau[i__];
            dscal_(&i__1, &d__1, &a[i__ + (i__ + 1) * a_dim1], lda);
        }
        a[i__ + i__ * a_dim1] = 1. - tau[i__];
        i__1 = i__ - 1;
        for (l = 1; l <= i__1; ++l) {
            a[i__ + l * a_dim1] = 0.;
        }
    }
    return 0;
}
#ifdef __cplusplus
}
#endif
