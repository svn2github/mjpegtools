/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 *                                                         *
 * FILE: motionsearch_deint.h                              *
 *                                                         *
 ***********************************************************/

void init_search_pattern (void);
struct vector search_forward_vector (int, int);
struct vector search_backward_vector (int, int);
void motion_compensate_field (void);
