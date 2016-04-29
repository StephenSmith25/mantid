/**
 This file is originally based on the implementation of the FBP (Filtered
 Back-Projection) method of tomopy, available from
 https:github.com/tomopy/tomopy/blob/master/src/fbp.c, which is:
 */
/*
 Copyright (c) 2015, UChicago Argonne, LLC. All rights reserved.

 Copyright 2015. UChicago Argonne, LLC. This software was produced
 under U.S. Government contract DE-AC02-06CH11357 for Argonne National
 Laboratory (ANL), which is operated by UChicago Argonne, LLC for the
 U.S. Department of Energy. The U.S. Government has rights to use,
 reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR
 UChicago Argonne, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is
 modified to produce derivative works, such modified software should
 be clearly marked, so as not to confuse it with the version available
 from ANL.

 Additionally, redistribution and use in source and binary forms, with
 or without modification, are permitted provided that the following
 conditions are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.

     * Neither the name of UChicago Argonne, LLC, Argonne National
       Laboratory, ANL, the U.S. Government, nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY UChicago Argonne, LLC AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UChicago
 Argonne, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "MantidAlgorithms/Tomography/tomopy/utils.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

void fbp(const float *data, int dy, int dt, int dx, const float *center,
         const float *theta, float *recon, int ngridx, int ngridy,
         const char *fname, const float *filter_par) {

  int s, p, d, n;
  int quadrant;
  float theta_p, sin_p, cos_p;
  float mov, xi, yi;
  int asize, bsize, csize;
  int ind_data, ind_recon;

  float *gridx, *gridy, *coordx, *coordy, *ax, *ay, *bx, *by, *coorx, *coory,
      *dist;
  int *indi;

  assert(coordx != NULL && coordy != NULL && ax != NULL && ay != NULL &&
         by != NULL && bx != NULL && coorx != NULL && coory != NULL &&
         dist != NULL && indi != NULL);

  gridx = (float *)malloc((size_t)(ngridx + 1) * sizeof(float));
  gridy = (float *)malloc((size_t)(ngridy + 1) * sizeof(float));
  coordx = (float *)malloc((size_t)(ngridy + 1) * sizeof(float));
  coordy = (float *)malloc((size_t)(ngridx + 1) * sizeof(float));
  ax = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  ay = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  bx = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  by = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  coorx = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  coory = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  dist = (float *)malloc((size_t)(ngridx + ngridy) * sizeof(float));
  indi = (int *)malloc((size_t)(ngridx + ngridy) * sizeof(int));

  /* unused arguments: */
  (void)fname;
  (void)filter_par;

  /* For each slice */
  for (s = 0; s < dy; s++) {
    preprocessing(ngridx, ngridy, dx, center[s], &mov, gridx,
                  gridy); /* Outputs: mov, gridx, gridy */

    /* For each projection angle */
    for (p = 0; p < dt; p++) {
      /*
         Calculate the sin and cos values
         of the projection angle and find
         at which quadrant on the cartesian grid.
      */
      theta_p = (float)fmod(theta[p], 2 * M_PI);
      quadrant = calc_quadrant(theta_p);
      sin_p = sinf(theta_p);
      cos_p = cosf(theta_p);

      /* For each detector pixel */
      for (d = 0; d < dx; d++) {
        /* Calculate coordinates */
        xi = (float)(-ngridx - ngridy);
        yi = (float)(1 - dx) / 2.0f + (float)d + mov;
        calc_coords(ngridx, ngridy, xi, yi, sin_p, cos_p, gridx, gridy, coordx,
                    coordy);

        /* Merge the (coordx, gridy) and (gridx, coordy) */
        trim_coords(ngridx, ngridy, coordx, coordy, gridx, gridy, &asize, ax,
                    ay, &bsize, bx, by);

        /*
          Sort the array of intersection points (ax, ay) and
          (bx, by). The new sorted intersection points are
          stored in (coorx, coory). Total number of points
          are csize.
        */
        sort_intersections(quadrant, asize, ax, ay, bsize, bx, by, &csize,
                           coorx, coory);

        /*
          Calculate the distances (dist) between the
          intersection points (coorx, coory). Find the
          indices of the pixels on the reconstruction grid.
        */
        calc_dist(ngridx, ngridy, csize, coorx, coory, indi, dist);

        /* Update */
        ind_recon = s * ngridx * ngridy;
        ind_data = d + p * dx + s * dt * dx;
        for (n = 0; n < csize - 1; n++) {
          recon[indi[n] + ind_recon] += data[ind_data] * dist[n];
        }
      }
    }
  }

  free(gridx);
  free(gridy);
  free(coordx);
  free(coordy);
  free(ax);
  free(ay);
  free(bx);
  free(by);
  free(coorx);
  free(coory);
  free(dist);
  free(indi);
}
