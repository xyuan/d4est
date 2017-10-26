#include <pXest.h>
#include <d4est_util.h>
#include <ini.h>
#include <d4est_operators.h>
#include <d4est_element_data.h>
#include <d4est_linalg.h>
#include <d4est_poisson_flux.h>
#include <d4est_poisson_flux_sipg.h>
#include <d4est_mortars.h>
#include <d4est_quadrature_compactified.h>
#include <d4est_elliptic_data.h>
#include <d4est_quadrature_lobatto.h>

static void
d4est_poisson_flux_boundary
(
 d4est_element_data_t* e_m,
 int f_m,
 int mortar_side_id_m,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_geometry_storage_t* d4est_factors,
 void* params
)
{
  d4est_poisson_flux_data_t* d4est_poisson_flux_params = params;
  int face_nodes_m_lobatto = d4est_lgl_get_nodes((P4EST_DIM) - 1, e_m->deg);

  int deg_quad = d4est_poisson_flux_params->get_deg_mortar_quad(e_m, d4est_poisson_flux_params->get_deg_mortar_quad_ctx);
  int face_nodes_m_quad = d4est_lgl_get_nodes((P4EST_DIM) - 1, deg_quad);
  double* u_m_on_f_m = P4EST_ALLOC(double, face_nodes_m_lobatto);
  double* u_m_on_f_m_quad = P4EST_ALLOC(double, face_nodes_m_quad);
  double* j_div_sj_quad = P4EST_ALLOC(double, face_nodes_m_quad);
  double* dudr_m_on_f_m [(P4EST_DIM)];
  double* dudr_m_on_f_m_quad [(P4EST_DIM)];
  double* dudx_m_on_f_m_quad [(P4EST_DIM)];
  double* u_at_bndry_lobatto = P4EST_ALLOC(double, face_nodes_m_lobatto);
  double* u_at_bndry_lobatto_to_quad = P4EST_ALLOC(double, face_nodes_m_quad);
  double* sj_on_f_m_quad = P4EST_ALLOC(double, face_nodes_m_quad);
  double* n_on_f_m_quad [(P4EST_DIM)];
  double* xyz_on_f_m_quad [(P4EST_DIM)];
  double* n_sj_on_f_m_quad [(P4EST_DIM)];
  double* drst_dxyz_quad [(P4EST_DIM)][(P4EST_DIM)];
  double* xyz_on_f_m_lobatto [(P4EST_DIM)];
  
  D4EST_ALLOC_DBYD_MAT(drst_dxyz_quad, face_nodes_m_quad);
  D4EST_ALLOC_DIM_VEC(xyz_on_f_m_lobatto, face_nodes_m_lobatto);
  D4EST_ALLOC_DIM_VEC(dudr_m_on_f_m, face_nodes_m_lobatto);
  D4EST_ALLOC_DIM_VEC(dudr_m_on_f_m_quad, face_nodes_m_quad);
  D4EST_ALLOC_DIM_VEC(dudx_m_on_f_m_quad,  face_nodes_m_quad);
  D4EST_ALLOC_DIM_VEC(n_on_f_m_quad, face_nodes_m_quad);
  D4EST_ALLOC_DIM_VEC(n_sj_on_f_m_quad, face_nodes_m_quad);
  D4EST_ALLOC_DIM_VEC(xyz_on_f_m_quad, face_nodes_m_quad);

  d4est_quadrature_mortar_t face_object; /* will not be used anyway */
  face_object.dq = e_m->dq;
  face_object.tree = e_m->tree;
  face_object.face = f_m;
  face_object.mortar_side_id = mortar_side_id_m;
  face_object.mortar_subface_id = 0;
  face_object.q[0] = e_m->q[0];
  face_object.q[1] = e_m->q[1];
#if (P4EST_DIM)==3
  face_object
    .q[2] = e_m->q[2];
#endif
  
  d4est_mortars_compute_geometric_data_on_mortar
    (
     d4est_ops,
     d4est_geom,
     d4est_quad,
     QUAD_INTEGRAND_UNKNOWN,
     e_m->tree,
     e_m->q,
     e_m->dq,
     mortar_side_id_m,
     1,
     1,
     &e_m->deg,
     &deg_quad,
     f_m,
     xyz_on_f_m_quad,
     drst_dxyz_quad,
     sj_on_f_m_quad,
     n_on_f_m_quad,
     n_sj_on_f_m_quad,
     j_div_sj_quad,
     COMPUTE_NORMAL_USING_JACOBIAN
    );
   
  for (int d = 0; d < (P4EST_DIM); d++){
    d4est_operators_apply_slicer
      (
       d4est_ops,
       e_m->dudr_elem[d],
       (P4EST_DIM),
       f_m,
       e_m->deg,
       dudr_m_on_f_m[d]
      );

    d4est_quadrature_interpolate
      (
       d4est_ops,
       d4est_quad,
       d4est_geom,
       &face_object,
       QUAD_OBJECT_MORTAR,
       QUAD_INTEGRAND_UNKNOWN,
       dudr_m_on_f_m[d],
       e_m->deg,
       dudr_m_on_f_m_quad[d],
       deg_quad
      );
  }

  d4est_operators_apply_slicer
    (
     d4est_ops,
     e_m->u_elem,
     (P4EST_DIM),
     f_m,
     e_m->deg,
     u_m_on_f_m
    );
  
  for (int d = 0; d < (P4EST_DIM); d++){
    d4est_linalg_fill_vec
      (
       dudx_m_on_f_m_quad[d],
       0.0,
       face_nodes_m_quad
      );
  }
  
  for (int j = 0; j < (P4EST_DIM); j++){
    for (int i = 0; i < (P4EST_DIM); i++){   
      for (int k = 0; k < face_nodes_m_quad; k++){
        dudx_m_on_f_m_quad[j][k] += drst_dxyz_quad[i][j][k]*dudr_m_on_f_m_quad[i][k];
      }
    }
  }
 
  for (int d = 0; d < (P4EST_DIM); d++){

    d4est_operators_apply_slicer(d4est_ops,
                                 e_m->xyz[d],
                                 (P4EST_DIM),
                                 f_m,
                                 e_m->deg,
                                 xyz_on_f_m_lobatto[d]);

  }


  d4est_quadrature_interpolate
    (
     d4est_ops,
     d4est_quad,
     d4est_geom,
     &face_object,
     QUAD_OBJECT_MORTAR,
     QUAD_INTEGRAND_UNKNOWN,
     u_m_on_f_m,
     e_m->deg,
     u_m_on_f_m_quad,
    deg_quad
    );



  
  d4est_poisson_flux_boundary_data_t boundary_data;
  boundary_data.deg_mortar_quad = deg_quad;
  boundary_data.face_object = &face_object;
  boundary_data.face_nodes_m_lobatto = face_nodes_m_lobatto;
  boundary_data.face_nodes_m_quad = face_nodes_m_quad;
  boundary_data.u_m_on_f_m_quad = u_m_on_f_m_quad;
  boundary_data.u_m_on_f_m = u_m_on_f_m;
  boundary_data.sj_on_f_m_quad = sj_on_f_m_quad;
  boundary_data.j_div_sj_quad = j_div_sj_quad; 
  D4EST_COPY_DBYD_MAT(drst_dxyz_quad, boundary_data.drst_dxyz_quad);
  D4EST_COPY_DIM_VEC(dudx_m_on_f_m_quad, boundary_data.dudx_m_on_f_m_quad);
  D4EST_COPY_DIM_VEC(n_on_f_m_quad, boundary_data.n_on_f_m_quad);
  D4EST_COPY_DIM_VEC(n_sj_on_f_m_quad, boundary_data.n_sj_on_f_m_quad);
  D4EST_COPY_DIM_VEC(xyz_on_f_m_quad, boundary_data.xyz_on_f_m_quad);
  D4EST_COPY_DIM_VEC(xyz_on_f_m_lobatto, boundary_data.xyz_on_f_m_lobatto);

  if (d4est_poisson_flux_params->boundary_fcn != NULL){
    d4est_poisson_flux_params->boundary_fcn
      (
       e_m,
       f_m,
       mortar_side_id_m,
       d4est_ops,
       d4est_geom,
       d4est_quad,
       &boundary_data,
       d4est_poisson_flux_params->bc_data,
       d4est_poisson_flux_params->flux_data
      );
  }
  
  D4EST_FREE_DIM_VEC(xyz_on_f_m_lobatto);
  D4EST_FREE_DIM_VEC(xyz_on_f_m_quad);
  D4EST_FREE_DIM_VEC(dudr_m_on_f_m);
  D4EST_FREE_DIM_VEC(dudr_m_on_f_m_quad);
  D4EST_FREE_DIM_VEC(dudx_m_on_f_m_quad);
  D4EST_FREE_DIM_VEC(n_on_f_m_quad);
  D4EST_FREE_DIM_VEC(n_sj_on_f_m_quad);
  D4EST_FREE_DBYD_MAT(drst_dxyz_quad);
  P4EST_FREE(u_m_on_f_m);
  P4EST_FREE(u_m_on_f_m_quad);
  P4EST_FREE(u_at_bndry_lobatto);
  P4EST_FREE(u_at_bndry_lobatto_to_quad);
  P4EST_FREE(sj_on_f_m_quad);
  P4EST_FREE(j_div_sj_quad);
}

static void
d4est_poisson_flux_interface
(
 d4est_element_data_t** e_m,
 int faces_m,
 int f_m,
 int mortar_side_id_m,
 d4est_element_data_t** e_p,
 int faces_p,
 int f_p,
 int mortar_side_id_p,
 int* e_m_is_ghost,
 int orientation,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_geometry_storage_t* d4est_factors,
 void* params
)
{
  d4est_poisson_flux_data_t* d4est_poisson_flux_params = params;
  int stride;
  int deg_p_lobatto [(P4EST_HALF)];
  int face_nodes_p_lobatto [(P4EST_HALF)];
  int face_nodes_p_quad [(P4EST_HALF)];
  int deg_m_lobatto [(P4EST_HALF)];
  int deg_m_quad [(P4EST_HALF)];
  int deg_p_quad [(P4EST_HALF)];
  int face_nodes_m_lobatto [(P4EST_HALF)];
  int face_nodes_m_quad [(P4EST_HALF)];
  int nodes_mortar_quad [(P4EST_HALF)];
  int nodes_mortar_lobatto [(P4EST_HALF)];
  int deg_mortar_quad [(P4EST_HALF)];
  int deg_mortar_lobatto [(P4EST_HALF)];
  int deg_p_lobatto_porder [(P4EST_HALF)];
  int faces_mortar = (faces_m > faces_p) ? faces_m : faces_p;

  d4est_element_data_t* e_p_oriented [(P4EST_HALF)];
  d4est_element_data_reorient_f_p_elements_to_f_m_order(e_p, (P4EST_DIM)-1, f_m, f_p, orientation, faces_p, e_p_oriented);

  /* calculate degs and nodes of each face of (-) side */
  int total_side_nodes_m_lobatto = 0;
  int total_side_nodes_m_quad = 0;
  for (int i = 0; i < faces_m; i++){
    deg_m_lobatto[i] = e_m[i]->deg;
    deg_m_quad[i] = d4est_poisson_flux_params->get_deg_mortar_quad(e_m[i], d4est_poisson_flux_params->get_deg_mortar_quad_ctx);
    
    face_nodes_m_lobatto[i] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, e_m[i]->deg);
    face_nodes_m_quad[i] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, deg_m_quad[i]);
    
    total_side_nodes_m_lobatto += face_nodes_m_lobatto[i];
    total_side_nodes_m_quad += face_nodes_m_quad[i];
  }

  /* calculate degs and nodes of each face of (+) side  */
  int total_side_nodes_p_lobatto = 0;
  int total_side_nodes_p_quad = 0;
  for (int i = 0; i < faces_p; i++){
    deg_p_lobatto[i] = e_p_oriented[i]->deg;
    deg_p_quad[i] = d4est_poisson_flux_params->get_deg_mortar_quad(e_p_oriented[i], d4est_poisson_flux_params->get_deg_mortar_quad_ctx);
    deg_p_lobatto_porder[i] = e_p[i]->deg;

    face_nodes_p_lobatto[i] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, e_p_oriented[i]->deg );
    face_nodes_p_quad[i] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, deg_p_quad[i]);
    
    total_side_nodes_p_lobatto += face_nodes_p_lobatto[i];
    total_side_nodes_p_quad += face_nodes_p_quad[i];
  }    

  /* calculate degs and nodes of the mortar faces */
  int total_nodes_mortar_quad = 0;
  int total_nodes_mortar_lobatto = 0;
  for (int i = 0; i < faces_m; i++)
    for (int j = 0; j < faces_p; j++){
      /* find max degree for each face pair of the two sides*/
      deg_mortar_quad[i+j] = d4est_util_max_int(deg_m_quad[i],
                                          deg_p_quad[j]);
      deg_mortar_lobatto[i+j] = d4est_util_max_int( e_m[i]->deg,
                                              e_p_oriented[j]->deg );      
      nodes_mortar_quad[i+j] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, deg_mortar_quad[i+j] );     
      nodes_mortar_lobatto[i+j] = d4est_lgl_get_nodes( (P4EST_DIM) - 1, deg_mortar_lobatto[i+j] );     
      total_nodes_mortar_quad += nodes_mortar_quad[i+j];
      total_nodes_mortar_lobatto += nodes_mortar_lobatto[i+j];

    }

  int deg_mortar_quad_porder [(P4EST_HALF)];
  int deg_mortar_lobatto_porder [(P4EST_HALF)];
  int nodes_mortar_quad_porder [(P4EST_HALF)];
  
  for(int i = 0; i < faces_mortar; i++){
    int inew = i;
    if (faces_mortar == (P4EST_HALF)){
      inew = d4est_reference_reorient_face_order((P4EST_DIM)-1, f_m, f_p, orientation, i);
    }
    deg_mortar_quad_porder[inew] = deg_mortar_quad[i];
    deg_mortar_lobatto_porder[inew] = deg_mortar_lobatto[i];
    nodes_mortar_quad_porder[inew] = nodes_mortar_quad[i];
  }

  p4est_qcoord_t mortar_q0_forder [(P4EST_HALF)][(P4EST_DIM)];
  p4est_qcoord_t mortar_dq_forder;
  p4est_qcoord_t mortar_q0_porder [(P4EST_HALF)][(P4EST_DIM)];
  p4est_qcoord_t mortar_dq_porder;
  d4est_quadrature_mortar_t mortar_face_object_forder [(P4EST_HALF)];
  d4est_quadrature_mortar_t mortar_face_object_porder [(P4EST_HALF)];
  
  d4est_mortars_compute_qcoords_on_mortar
    (
     e_m[0]->tree,
     e_m[0]->q,
     e_m[0]->dq,
     faces_m,
     faces_mortar,
     f_m,
     mortar_q0_forder,
     &mortar_dq_forder
    );

  d4est_mortars_compute_qcoords_on_mortar
    (
     e_p[0]->tree,
     e_p[0]->q,
     e_p[0]->dq,
     faces_p,
     faces_mortar,
     f_p,
     mortar_q0_porder,
     &mortar_dq_porder
    );


  for (int f = 0; f < faces_mortar; f++){
    mortar_face_object_forder[f].dq = mortar_dq_forder;
    mortar_face_object_forder[f].tree = e_m[0]->tree;
    mortar_face_object_forder[f].mortar_side_id = mortar_side_id_m;
    mortar_face_object_forder[f].mortar_subface_id = f;
    mortar_face_object_forder[f].face = f_m;
    mortar_face_object_forder[f].q[0] = mortar_q0_forder[f][0];
    mortar_face_object_forder[f].q[1] = mortar_q0_forder[f][1];
#if (P4EST_DIM)==3
    mortar_face_object_forder[f].q[2] = mortar_q0_forder[f][2];
#endif

    mortar_face_object_porder[f].dq = mortar_dq_porder;
    mortar_face_object_porder[f].tree = e_p[0]->tree;
    mortar_face_object_porder[f].face = f_p;
    mortar_face_object_porder[f].mortar_side_id = mortar_side_id_p;
    mortar_face_object_porder[f].mortar_subface_id = f;
    mortar_face_object_porder[f].q[0] = mortar_q0_porder[f][0];
    mortar_face_object_porder[f].q[1] = mortar_q0_porder[f][1];
#if (P4EST_DIM)==3
    mortar_face_object_porder[f].q[2] = mortar_q0_porder[f][2];
#endif
  }
  
  double* u_m_on_f_m = P4EST_ALLOC(double, total_side_nodes_m_lobatto);
  double* u_p_on_f_p = P4EST_ALLOC(double, total_side_nodes_p_lobatto);  
  double* u_m_on_f_m_mortar = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* u_m_on_f_m_mortar_quad = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* u_p_on_f_p_mortar = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* u_p_on_f_p_mortar_quad = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* dudr_p_on_f_p_porder [(P4EST_DIM)];
  double* dudr_m_on_f_m [(P4EST_DIM)];
  double* dudr_p_on_f_p_mortar_porder [(P4EST_DIM)];
  double* dudr_m_on_f_m_mortar [(P4EST_DIM)];
  double* dudr_p_on_f_p_mortar_quad_porder [(P4EST_DIM)];
  double* dudr_m_on_f_m_mortar_quad [(P4EST_DIM)];
  double* dudx_p_on_f_p_mortar_quad_porder [(P4EST_DIM)];
  double* dudx_m_on_f_m_mortar_quad [(P4EST_DIM)];
  double* dudx_p_on_f_p_mortar_quad [(P4EST_DIM)];
  double* drst_dxyz_m_on_mortar_quad [(P4EST_DIM)][(P4EST_DIM)];
  double* drst_dxyz_p_on_mortar_quad_porder [(P4EST_DIM)][(P4EST_DIM)];
  double* sj_on_f_m_mortar_quad = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* n_on_f_m_mortar_quad [(P4EST_DIM)];
  double* n_sj_on_f_m_mortar_quad [(P4EST_DIM)];
  double* j_div_sj_on_f_m_mortar_quad = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* j_div_sj_on_f_p_mortar_quad_porder = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* j_div_sj_on_f_p_mortar_quad = P4EST_ALLOC(double, total_nodes_mortar_quad);
  double* tmp = P4EST_ALLOC(double, total_side_nodes_p_quad);

  D4EST_ALLOC_DBYD_MAT(drst_dxyz_m_on_mortar_quad, total_nodes_mortar_quad);
  D4EST_ALLOC_DBYD_MAT(drst_dxyz_p_on_mortar_quad_porder, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudr_p_on_f_p_porder, total_side_nodes_p_lobatto);
  D4EST_ALLOC_DIM_VEC(dudr_p_on_f_p_mortar_porder, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudr_p_on_f_p_mortar_quad_porder, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudx_p_on_f_p_mortar_quad_porder, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudx_p_on_f_p_mortar_quad, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudr_m_on_f_m, total_side_nodes_m_lobatto);
  D4EST_ALLOC_DIM_VEC(dudr_m_on_f_m_mortar, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudr_m_on_f_m_mortar_quad, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(dudx_m_on_f_m_mortar_quad, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(n_on_f_m_mortar_quad, total_nodes_mortar_quad);
  D4EST_ALLOC_DIM_VEC(n_sj_on_f_m_mortar_quad, total_nodes_mortar_quad);
 
  stride = 0;
  for (int i = 0; i < faces_m; i++){
    d4est_operators_apply_slicer
      (
       d4est_ops,
       &(e_m[i]->u_elem[0]),
       (P4EST_DIM),
       f_m,
       e_m[i]->deg,
       &u_m_on_f_m[stride]
      );
    
    stride += face_nodes_m_lobatto[i];
  }
 
  stride = 0;
  for (int i = 0; i < faces_p; i++){
    d4est_operators_apply_slicer
      (
       d4est_ops,
       &(e_p_oriented[i]->u_elem[0]),
       (P4EST_DIM),
       f_p,
       e_p_oriented[i]->deg,
       tmp
      );
    
    d4est_operators_reorient_face_data
      (
       d4est_ops,
       tmp,
       ((P4EST_DIM) - 1),
       e_p_oriented[i]->deg,
       orientation,
       f_m,
       f_p,
       &u_p_on_f_p[stride]
      );
    
    stride += face_nodes_p_lobatto[i];
  }


  /* project (-)-side u trace vector onto mortar space */
  d4est_mortars_project_side_onto_mortar_space
    (
     d4est_ops,
     u_m_on_f_m,
     faces_m,
     deg_m_lobatto,
     u_m_on_f_m_mortar,
     faces_mortar,
     deg_mortar_quad
    );

  /* project (+)-side u trace vector onto mortar space */
  d4est_mortars_project_side_onto_mortar_space
    (
     d4est_ops,
     u_p_on_f_p,
     faces_p,
     deg_p_lobatto,
     u_p_on_f_p_mortar,
     faces_mortar,
     deg_mortar_quad
    );

  
  stride = 0;
  for (int f = 0; f < faces_mortar; f++){

    d4est_quadrature_interpolate
      (
       d4est_ops,
       d4est_quad,
       d4est_geom,
       &mortar_face_object_forder,
       QUAD_OBJECT_MORTAR,
       QUAD_INTEGRAND_UNKNOWN,
       &u_m_on_f_m_mortar[stride],
       deg_mortar_quad[f],
       &u_m_on_f_m_mortar_quad[stride],
       deg_mortar_quad[f]
      );
    
    d4est_quadrature_interpolate
      (
       d4est_ops,
       d4est_quad,
       d4est_geom,
       &mortar_face_object_forder,
       QUAD_OBJECT_MORTAR,
       QUAD_INTEGRAND_UNKNOWN,
       &u_p_on_f_p_mortar[stride],
       deg_mortar_quad[f],
       &u_p_on_f_p_mortar_quad[stride],
       deg_mortar_quad[f]
      );

    stride += nodes_mortar_quad[f];
  }

  
  /* For each component of the vector */
  for (int d = 0; d < (P4EST_DIM); d++){

    stride = 0;
    for (int f = 0; f < faces_m; f++){    
      d4est_operators_apply_slicer
        (
         d4est_ops,
         &e_m[f]->dudr_elem[d][0],
         (P4EST_DIM),
         f_m,
         e_m[f]->deg,
         &dudr_m_on_f_m[d][stride]
        );
      stride += face_nodes_m_lobatto[f];
    }
    
    stride = 0;
    for (int f = 0; f < faces_p; f++){
      d4est_operators_apply_slicer
        (
         d4est_ops,
         &e_p[f]->dudr_elem[d][0],
         (P4EST_DIM),
         f_p,
         e_p[f]->deg,
         &dudr_p_on_f_p_porder[d][stride]
        );
      stride += d4est_lgl_get_nodes((P4EST_DIM)-1, e_p[f]->deg);
    }

    d4est_mortars_project_side_onto_mortar_space
      (
       d4est_ops,
       dudr_p_on_f_p_porder[d],
       faces_p,
       deg_p_lobatto_porder,
       dudr_p_on_f_p_mortar_porder[d],
       faces_mortar,
       deg_mortar_quad_porder
      );

    d4est_mortars_project_side_onto_mortar_space
      (
       d4est_ops,
       dudr_m_on_f_m[d],
       faces_m,
       deg_m_lobatto,
       dudr_m_on_f_m_mortar[d],
       faces_mortar,
       deg_mortar_quad
      );

    stride = 0;
    for (int f = 0; f < faces_mortar; f++){

      d4est_quadrature_interpolate
        (
         d4est_ops,
         d4est_quad,
         d4est_geom,
         &mortar_face_object_forder,
         QUAD_OBJECT_MORTAR,
         QUAD_INTEGRAND_UNKNOWN,
         &dudr_m_on_f_m_mortar[d][stride],
         deg_mortar_quad[f],
         &dudr_m_on_f_m_mortar_quad[d][stride],
         deg_mortar_quad[f]
        );

      stride += nodes_mortar_quad[f];
    }

    stride = 0;
    for (int f = 0; f < faces_mortar; f++){

      d4est_quadrature_interpolate
        (
         d4est_ops,
         d4est_quad,
         d4est_geom,
         &mortar_face_object_porder,
         QUAD_OBJECT_MORTAR,
         QUAD_INTEGRAND_UNKNOWN,
         &dudr_p_on_f_p_mortar_porder[d][stride],
         deg_mortar_quad_porder[f],
         &dudr_p_on_f_p_mortar_quad_porder[d][stride],
         deg_mortar_quad_porder[f]
        );

      stride += nodes_mortar_quad_porder[f];
    }
  }

  d4est_mortars_compute_geometric_data_on_mortar
    (
     d4est_ops,
     d4est_geom,
     d4est_quad,
     QUAD_INTEGRAND_UNKNOWN,
     e_m[0]->tree,
     e_m[0]->q,
     e_m[0]->dq,
     mortar_side_id_m,
     faces_m,
     faces_mortar,
     &deg_mortar_lobatto[0],
     &deg_mortar_quad[0],
     f_m,
     NULL,
     drst_dxyz_m_on_mortar_quad,
     sj_on_f_m_mortar_quad,
     n_on_f_m_mortar_quad,
     n_sj_on_f_m_mortar_quad,
     j_div_sj_on_f_m_mortar_quad,
     COMPUTE_NORMAL_USING_JACOBIAN
    );

  d4est_mortars_compute_geometric_data_on_mortar
    (
     d4est_ops,
     d4est_geom,
     d4est_quad,
     QUAD_INTEGRAND_UNKNOWN,
     e_p[0]->tree,
     e_p[0]->q,
     e_p[0]->dq,
     mortar_side_id_p,
     faces_p,
     faces_mortar,
     &deg_mortar_lobatto_porder[0],
     &deg_mortar_quad_porder[0],
     f_p,
     NULL,
     drst_dxyz_p_on_mortar_quad_porder,
#ifdef POISSON_FLUX_COMPUTE_PORDER_MORTAR_GEOM
     NULL//sj_on_f_p_mortar_quad_porder,
     NULL//n_on_f_p_mortar_quad_porder,
     NULL,
#else
     NULL,
     NULL,
     NULL,
#endif
     NULL,//j_div_sj_on_f_p_mortar_quad_porder,
     COMPUTE_NORMAL_USING_JACOBIAN
    );

  int compare = d4est_util_compare_vecs(sj_on_f_m_mortar_quad, &d4est_factors->sj_m_mortar_quad[e_m[0]->mortar_quad_scalar_stride[f_m]], total_nodes_mortar_quad, 1e-13);
  /* printf("compare = %d\n", compare); */

  D4EST_ASSERT(compare);
  /* double* vec1 = &d4est_factors->sj_m_mortar_quad[e_m[0]->mortar_quad_scalar_stride[f_m]]; */
  /* DEBUG_PRINT_2ARR_DBL(sj_on_f_m_mortar_quad,vec1,total_nodes_mortar_quad); */
  
  
  for (int d = 0; d < (P4EST_DIM); d++){
    d4est_linalg_fill_vec
      (
       dudx_m_on_f_m_mortar_quad[d],
       0.0,
       total_nodes_mortar_quad
      );


    d4est_linalg_fill_vec
      (
       dudx_p_on_f_p_mortar_quad_porder[d],
       0.0,
       total_nodes_mortar_quad
      );
  }
  
  for (int j = 0; j < (P4EST_DIM); j++){
    for (int i = 0; i < (P4EST_DIM); i++){   
      for (int k = 0; k < total_nodes_mortar_quad; k++){
        dudx_m_on_f_m_mortar_quad[j][k] +=
          drst_dxyz_m_on_mortar_quad[i][j][k]*dudr_m_on_f_m_mortar_quad[i][k];
        dudx_p_on_f_p_mortar_quad_porder[j][k] += drst_dxyz_p_on_mortar_quad_porder[i][j][k]*dudr_p_on_f_p_mortar_quad_porder[i][k];
      }
    }    
  }
  
  int face_mortar_stride = 0;
  for (int face = 0; face < faces_mortar; face++){
    int face_p = face;
    if (faces_mortar == (P4EST_HALF))
      face_p = d4est_reference_reorient_face_order((P4EST_DIM)-1, f_m, f_p, orientation, face);

    int oriented_face_mortar_stride = 0;
    for (int b = 0; b < face_p; b++){
      oriented_face_mortar_stride += d4est_lgl_get_nodes((P4EST_DIM)-1, deg_mortar_quad_porder[b]);
    }


    for (int d = 0; d < (P4EST_DIM); d++){
      d4est_operators_reorient_face_data
        (
         d4est_ops,
         &dudx_p_on_f_p_mortar_quad_porder[d][oriented_face_mortar_stride],
         (P4EST_DIM)-1,
         deg_mortar_quad[face],
         orientation,
         f_m,
         f_p,
         &dudx_p_on_f_p_mortar_quad[d][face_mortar_stride]
        );
    }

    d4est_operators_reorient_face_data
        (
         d4est_ops,
         &j_div_sj_on_f_p_mortar_quad_porder[oriented_face_mortar_stride],
         (P4EST_DIM)-1,
         deg_mortar_quad[face],
         orientation,
         f_m,
         f_p,
         &j_div_sj_on_f_p_mortar_quad[face_mortar_stride]
        );
    
    
    face_mortar_stride += d4est_lgl_get_nodes((P4EST_DIM)-1, deg_mortar_quad[face]);
  }

  /* We interpolated a derivative up to the mortar, but this is a different reference space, so we need
 another dr/dx factor */
  
  if (faces_m != faces_mortar){
    for (int d = 0; d < (P4EST_DIM); d++)
      d4est_linalg_vec_scale(.5, dudx_m_on_f_m_mortar_quad[d], total_nodes_mortar_quad);
  }

  if (faces_p != faces_mortar){
    for (int d = 0; d < (P4EST_DIM); d++)
      d4est_linalg_vec_scale(.5, dudx_p_on_f_p_mortar_quad[d], total_nodes_mortar_quad);
  }
  
  d4est_poisson_flux_interface_data_t interface_data;
  interface_data.mortar_face_object = mortar_face_object_forder;
  interface_data.total_side_nodes_m_lobatto = total_side_nodes_m_lobatto;
  interface_data.total_side_nodes_p_lobatto = total_side_nodes_p_lobatto;
  interface_data.total_nodes_mortar_lobatto = total_nodes_mortar_lobatto;
  interface_data.total_nodes_mortar_quad = total_nodes_mortar_quad;
  interface_data.faces_mortar = faces_mortar;
  interface_data.u_m_on_f_m_mortar_quad = u_m_on_f_m_mortar_quad;
  interface_data.u_m_on_f_m = u_m_on_f_m;
  interface_data.u_p_on_f_p = u_p_on_f_p;
  interface_data.sj_on_f_m_mortar_quad = sj_on_f_m_mortar_quad;
  interface_data.j_div_sj_on_f_m_mortar_quad = j_div_sj_on_f_m_mortar_quad;
  interface_data.u_p_on_f_p_mortar_quad = u_p_on_f_p_mortar_quad;
  interface_data.j_div_sj_on_f_p_mortar_quad = j_div_sj_on_f_p_mortar_quad;
  interface_data.deg_mortar_quad = deg_mortar_quad;
  interface_data.nodes_mortar_quad = nodes_mortar_quad;
  interface_data.deg_mortar_lobatto = deg_mortar_lobatto;
  interface_data.nodes_mortar_lobatto = nodes_mortar_lobatto;
  interface_data.deg_m_lobatto = deg_m_lobatto;
  interface_data.face_nodes_m_lobatto = face_nodes_m_lobatto;
  interface_data.deg_p_lobatto = deg_p_lobatto;
  interface_data.face_nodes_p_lobatto = face_nodes_p_lobatto;
  D4EST_COPY_DBYD_MAT(drst_dxyz_m_on_mortar_quad,interface_data.drst_dxyz_m_on_mortar_quad);
  D4EST_COPY_DIM_VEC(dudx_m_on_f_m_mortar_quad,interface_data.dudx_m_on_f_m_mortar_quad);
  D4EST_COPY_DIM_VEC(dudx_p_on_f_p_mortar_quad,interface_data.dudx_p_on_f_p_mortar_quad);
  D4EST_COPY_DIM_VEC(n_on_f_m_mortar_quad,interface_data.n_on_f_m_mortar_quad);
  D4EST_COPY_DIM_VEC(n_sj_on_f_m_mortar_quad,interface_data.n_sj_on_f_m_mortar_quad);

  if (d4est_poisson_flux_params->interface_fcn != NULL){
  d4est_poisson_flux_params->interface_fcn
    (
     e_m,
     faces_m,
     f_m,
     mortar_side_id_m,
     e_p,
     faces_p,
     f_p,
     mortar_side_id_p,
     e_m_is_ghost,
     orientation,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     &interface_data,
     d4est_poisson_flux_params->flux_data
    );

  }

  P4EST_FREE(j_div_sj_on_f_m_mortar_quad);
  P4EST_FREE(j_div_sj_on_f_p_mortar_quad_porder);
  P4EST_FREE(j_div_sj_on_f_p_mortar_quad);
  P4EST_FREE(u_m_on_f_m);
  P4EST_FREE(u_p_on_f_p);
  P4EST_FREE(sj_on_f_m_mortar_quad);
  P4EST_FREE(tmp);  
  P4EST_FREE(u_m_on_f_m_mortar);
  P4EST_FREE(u_m_on_f_m_mortar_quad);
  P4EST_FREE(u_p_on_f_p_mortar);
  P4EST_FREE(u_p_on_f_p_mortar_quad);
  D4EST_FREE_DIM_VEC(dudr_p_on_f_p_porder);
  D4EST_FREE_DIM_VEC(dudr_p_on_f_p_mortar_porder);
  D4EST_FREE_DIM_VEC(dudr_p_on_f_p_mortar_quad_porder);
  D4EST_FREE_DIM_VEC(dudx_p_on_f_p_mortar_quad_porder);
  D4EST_FREE_DIM_VEC(dudx_p_on_f_p_mortar_quad);
  D4EST_FREE_DIM_VEC(dudr_m_on_f_m);
  D4EST_FREE_DIM_VEC(dudr_m_on_f_m_mortar);
  D4EST_FREE_DIM_VEC(dudr_m_on_f_m_mortar_quad);
  D4EST_FREE_DIM_VEC(dudx_m_on_f_m_mortar_quad);
  D4EST_FREE_DIM_VEC(n_on_f_m_mortar_quad);
  D4EST_FREE_DIM_VEC(n_sj_on_f_m_mortar_quad);
  D4EST_FREE_DBYD_MAT(drst_dxyz_m_on_mortar_quad);
  D4EST_FREE_DBYD_MAT(drst_dxyz_p_on_mortar_quad_porder);
}

static
int d4est_poisson_flux_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  d4est_poisson_flux_data_t* pconfig = (d4est_poisson_flux_data_t*)user;
  if (d4est_util_match_couple(section,"flux",name,"name")) {
    D4EST_ASSERT(pconfig->flux_type == FLUX_NOT_SET);
    if (d4est_util_match(value,"sipg")){
      pconfig->flux_type = FLUX_SIPG;
    }
    else {
      D4EST_ABORT("[D4EST_ERROR]: name = sipg is the only supported flux");
    }
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}

static void
d4est_poisson_flux_input
(
 const char* input_file,
 d4est_poisson_flux_data_t* input
)
{
  input->flux_type = FLUX_NOT_SET;
  
  if (ini_parse(input_file, d4est_poisson_flux_input_handler, input) < 0) {
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT("flux", input->flux_type, FLUX_NOT_SET); 
}


void
d4est_poisson_flux_init_element_data
(
 p4est_t* p4est,
 d4est_operators_t* d4est_ops,
 double* u,
 double* Au
){

  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        d4est_element_data_t* ed = quad->p.user_data; 
        int deg = ed->deg;
        int volume_nodes_lobatto = d4est_lgl_get_nodes((P4EST_DIM),deg);

        ed->Au_elem = &(Au[ed->nodal_stride]);
  
        d4est_linalg_copy_1st_to_2nd
          (
           &(u[ed->nodal_stride]),
           &(ed->u_elem)[0],
           volume_nodes_lobatto
          );

        for (int i = 0; i < (P4EST_DIM); i++){
          d4est_operators_apply_dij(d4est_ops, &(u[ed->nodal_stride]), (P4EST_DIM), ed->deg, i, &ed->dudr_elem[i][0]);
        }
      }
    }     
}


d4est_mortar_fcn_ptrs_t
d4est_poisson_flux_fetch_fcns
(
 d4est_poisson_flux_data_t* data
)
{
  d4est_mortar_fcn_ptrs_t flux_fcns;
  flux_fcns.flux_interface_fcn = d4est_poisson_flux_interface;
  flux_fcns.flux_boundary_fcn = d4est_poisson_flux_boundary;
  flux_fcns.user_ctx = (void*)data;
  return flux_fcns;
}


d4est_poisson_flux_data_t*
d4est_poisson_flux_new
(
 p4est_t* p4est,
 const char* input_file,
 d4est_poisson_bc_t bc_type,
 void* bc_data,
 int (*get_deg_mortar_quad)(d4est_element_data_t*, void*),
 void* get_deg_mortar_quad_ctx
)
{
  d4est_poisson_flux_data_t* data = P4EST_ALLOC(d4est_poisson_flux_data_t, 1);
  d4est_poisson_flux_input(input_file, data);

  data->get_deg_mortar_quad = get_deg_mortar_quad;
  data->get_deg_mortar_quad_ctx = get_deg_mortar_quad_ctx;
  data->bc_type = bc_type;
  data->bc_data = bc_data;
  
  if (data->flux_type == FLUX_SIPG) {
    d4est_poisson_flux_sipg_params_new(p4est, "[SIPG_FLUX]", input_file, data);
  }
  else {
    D4EST_ABORT("[D4EST_ERROR]: this flux is currently not supported");
  }

  return data;
}


void
d4est_poisson_flux_destroy
(
 d4est_poisson_flux_data_t* data
)
{
  if (data->destroy != NULL)
    data->destroy(data);
}
