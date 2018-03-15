/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <retro_math.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include <d3dx9shader.h>

#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../video_shader_parse.h"
#include "../drivers/d3d.h"
#include "../../managers/state_manager.h"
#include "../../verbosity.h"

#include "../drivers/d3d_shaders/opaque.hlsl.d3d9.h"
#include "shader_hlsl.h"

#ifdef __cplusplus

#ifndef ID3DXConstantTable_SetDefaults
#define ID3DXConstantTable_SetDefaults(p,a) (p)->SetDefaults(a);
#endif

#ifndef ID3DXConstantTable_SetFloatArray
#define ID3DXConstantTable_SetFloatArray(p,a,b,c,d) (p)->SetFloatArray(a,b,c,d)
#endif

#ifndef ID3DXConstantTable_GetConstantByName
#define ID3DXConstantTable_GetConstantByName(p,a,b)  ((p)->GetConstantByName(a, b))
#endif

#ifndef ID3DXConstantTable_SetMatrix
#define ID3DXConstantTable_SetMatrix(p,a,b,c) ((p)->SetMatrix(a,b,c))
#endif

#else

#ifndef ID3DXConstantTable_SetDefaults
#define ID3DXConstantTable_SetDefaults(p,a) (p)->lpVtbl->SetDefaults(p,a)
#endif

#ifndef ID3DXConstantTable_SetFloatArray
#define ID3DXConstantTable_SetFloatArray(p,a,b,c,d) (p)->lpVtbl->SetFloatArray(p,a,b,c,d)
#endif

#ifndef ID3DXConstantTable_GetConstantByName
#define ID3DXConstantTable_GetConstantByName(p,a,b)  ((p)->lpVtbl->GetConstantByName(p, a, b))
#endif

#ifndef ID3DXConstantTable_SetMatrix
#define ID3DXConstantTable_SetMatrix(p,a,b,c) ((p)->lpVtbl->SetMatrix(p,a,b,c))
#endif

#endif

#define set_param_2f(param, xy, constanttable) if (param) { ID3DXConstantTable_SetFloatArray(constanttable, d3dr, param, xy, 2); }
#define get_constant_by_name(a, b, constanttable) ID3DXConstantTable_GetConstantByName(constanttable, a, b)


struct shader_program_hlsl_data
{
   LPDIRECT3DVERTEXSHADER9 vprg;
   LPDIRECT3DPIXELSHADER9 fprg;

   D3DXHANDLE	   vid_size_f;
   D3DXHANDLE	   tex_size_f;
   D3DXHANDLE	   out_size_f;
   D3DXHANDLE     frame_cnt_f;
   D3DXHANDLE     frame_dir_f;
   D3DXHANDLE	   vid_size_v;
   D3DXHANDLE	   tex_size_v;
   D3DXHANDLE	   out_size_v;
   D3DXHANDLE     frame_cnt_v;
   D3DXHANDLE     frame_dir_v;
   D3DXHANDLE     mvp;

   LPD3DXCONSTANTTABLE v_ctable;
   LPD3DXCONSTANTTABLE f_ctable;
   D3DXMATRIX mvp_val;
};

typedef struct hlsl_shader_data hlsl_shader_data_t;

struct hlsl_shader_data
{
   LPDIRECT3DDEVICE9 dev;
   struct shader_program_hlsl_data prg[RARCH_HLSL_MAX_SHADERS];
   unsigned active_idx;
   struct video_shader *cg_shader;
};

void hlsl_set_proj_matrix(void *data, void *matrix_data)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   const D3DMATRIX *matrix  = (const D3DMATRIX*)matrix_data;
   if (hlsl && matrix)
      hlsl->prg[hlsl->active_idx].mvp_val = *matrix;
}

static void hlsl_set_uniform_parameter(
      void *data,
      struct uniform_info *param,
      void *uniform_data)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;

   (void)hlsl;

   if (!param || !param->enabled)
      return;

   switch (param->type)
   {
      case UNIFORM_1F:
         /* Unimplemented  */
         break;
      case UNIFORM_2F:
         /* Unimplemented  */
         break;
      case UNIFORM_3F:
         /* Unimplemented  */
         break;
      case UNIFORM_4F:
         /* Unimplemented  */
         break;
      case UNIFORM_1FV:
         /* Unimplemented  */
         break;
      case UNIFORM_2FV:
         /* Unimplemented  */
         break;
      case UNIFORM_3FV:
         /* Unimplemented  */
         break;
      case UNIFORM_4FV:
         /* Unimplemented  */
         break;
      case UNIFORM_1I:
         /* Unimplemented - Cg limitation */
         break;
   }
}

static void hlsl_set_params(void *dat, void *shader_data)
{
   float ori_size[2], tex_size[2], out_size[2];
   video_shader_ctx_params_t      *params = (video_shader_ctx_params_t*)dat;
   void *data                             = params->data;
   unsigned width                         = params->width;
   unsigned height                        = params->height;
   unsigned tex_width                     = params->tex_width;
   unsigned tex_height                    = params->tex_height;
   unsigned out_width                     = params->out_width;
   unsigned out_height                    = params->out_height;
   unsigned frame_count                   = params->frame_counter;
   const void *_info                      = params->info;
   const void *_prev_info                 = params->prev_info;
   const void *_feedback_info             = params->feedback_info;
   const void *_fbo_info                  = params->fbo_info;
   unsigned fbo_info_cnt                  = params->fbo_info_cnt;
   float frame_cnt                        = frame_count;
   const struct video_tex_info *info      = (const struct video_tex_info*)_info;
   const struct video_tex_info *prev_info = (const struct video_tex_info*)_prev_info;
   const struct video_tex_info *fbo_info  = (const struct video_tex_info*)_fbo_info;
   hlsl_shader_data_t *hlsl               = (hlsl_shader_data_t*)shader_data;
   LPDIRECT3DDEVICE9 d3dr                 = (LPDIRECT3DDEVICE9)hlsl->dev;

   if (!hlsl || !d3dr)
      return;

   ori_size[0] = (float)width;
   ori_size[1] = (float)height;
   tex_size[0] = (float)tex_width;
   tex_size[1] = (float)tex_height;
   out_size[0] = (float)out_width;
   out_size[1] = (float)out_height;

   ID3DXConstantTable_SetDefaults(
         hlsl->prg[hlsl->active_idx].f_ctable, d3dr);
   ID3DXConstantTable_SetDefaults(
         hlsl->prg[hlsl->active_idx].v_ctable, d3dr);

   set_param_2f(hlsl->prg[hlsl->active_idx].vid_size_f, ori_size, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].tex_size_f, tex_size, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].out_size_f, out_size, hlsl->prg[hlsl->active_idx].f_ctable);

   if (hlsl->prg[hlsl->active_idx].frame_cnt_f)
      d3dx_constant_table_set_float(hlsl->prg[hlsl->active_idx].f_ctable,
            d3dr,hlsl->prg[hlsl->active_idx].frame_cnt_f, frame_cnt);

   if (hlsl->prg[hlsl->active_idx].frame_dir_f)
      d3dx_constant_table_set_float(hlsl->prg[hlsl->active_idx].f_ctable,
            d3dr, hlsl->prg[hlsl->active_idx].frame_dir_f, state_manager_frame_is_reversed() ? -1.0 : 1.0);

   set_param_2f(hlsl->prg[hlsl->active_idx].vid_size_v, ori_size, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].tex_size_v, tex_size, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].out_size_v, out_size, hlsl->prg[hlsl->active_idx].v_ctable);

   if (hlsl->prg[hlsl->active_idx].frame_cnt_v)
      d3dx_constant_table_set_float(hlsl->prg[hlsl->active_idx].v_ctable,
            d3dr, hlsl->prg[hlsl->active_idx].frame_cnt_v, frame_cnt);

   if (hlsl->prg[hlsl->active_idx].frame_dir_v)
      d3dx_constant_table_set_float(hlsl->prg[hlsl->active_idx].v_ctable,
            d3dr, hlsl->prg[hlsl->active_idx].frame_dir_v, state_manager_frame_is_reversed() ? -1.0 : 1.0);

   /* TODO - set lookup textures/FBO textures/state parameters/etc */
}

static bool hlsl_compile_program(
      void *data,
      unsigned idx,
      void *program_data,
      struct shader_program_info *program_info)
{
   hlsl_shader_data_t *hlsl                  = (hlsl_shader_data_t*)data;
   struct shader_program_hlsl_data *program  = (struct shader_program_hlsl_data*)program_data;
   LPDIRECT3DDEVICE9 d3dr                    = (LPDIRECT3DDEVICE9)hlsl->dev;
   ID3DXBuffer *listing_f                    = NULL;
   ID3DXBuffer *listing_v                    = NULL;
   ID3DXBuffer *code_f                       = NULL;
   ID3DXBuffer *code_v                       = NULL;

   if (!program)
      program = &hlsl->prg[idx];

   if (program_info->is_file)
   {
      if (!d3dx_compile_shader_from_file(program_info->combined, NULL, NULL,
               "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &program->f_ctable))
         goto error;
      if (!d3dx_compile_shader_from_file(program_info->combined, NULL, NULL,
               "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &program->v_ctable))
         goto error;
   }
   else
   {
      /* TODO - crashes currently - to do with 'end of line' of stock shader */
      if (!d3dx_compile_shader(program_info->combined,
               strlen(program_info->combined), NULL, NULL,
               "main_fragment", "ps_3_0", 0, &code_f, &listing_f,
               &program->f_ctable ))
         goto error;
      if (!d3dx_compile_shader(program_info->combined,
               strlen(program_info->combined), NULL, NULL,
               "main_vertex", "vs_3_0", 0, &code_v, &listing_v,
               &program->v_ctable ))
         goto error;
   }

   d3d_create_pixel_shader(d3dr, (const DWORD*)d3dx_get_buffer_ptr(code_f),  (void**)&program->fprg);
   d3d_create_vertex_shader(d3dr, (const DWORD*)d3dx_get_buffer_ptr(code_v), (void**)&program->vprg);
   d3dxbuffer_release((void*)code_f);
   d3dxbuffer_release((void*)code_v);

   return true;

error:
   RARCH_ERR("Cg/HLSL error:\n");
   if (listing_f)
      RARCH_ERR("Fragment:\n%s\n", (char*)d3dx_get_buffer_ptr(listing_f));
   if (listing_v)
      RARCH_ERR("Vertex:\n%s\n", (char*)d3dx_get_buffer_ptr(listing_v));
   d3dxbuffer_release((void*)listing_f);
   d3dxbuffer_release((void*)listing_v);

   return false;
}

static bool hlsl_load_stock(hlsl_shader_data_t *hlsl)
{
   struct shader_program_info program_info;

   program_info.combined     = stock_hlsl_program;
   program_info.is_file      = false;

   if (!hlsl_compile_program(hlsl, 0, &hlsl->prg[0], &program_info))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   return true;
}

static void hlsl_set_program_attributes(hlsl_shader_data_t *hlsl, unsigned i)
{
   if (!hlsl)
      return;

   hlsl->prg[i].vid_size_f  = get_constant_by_name(NULL, "$IN.video_size",      hlsl->prg[i].f_ctable);
   hlsl->prg[i].tex_size_f  = get_constant_by_name(NULL, "$IN.texture_size",    hlsl->prg[i].f_ctable);
   hlsl->prg[i].out_size_f  = get_constant_by_name(NULL, "$IN.output_size",     hlsl->prg[i].f_ctable);
   hlsl->prg[i].frame_cnt_f = get_constant_by_name(NULL, "$IN.frame_count",     hlsl->prg[i].f_ctable);
   hlsl->prg[i].frame_dir_f = get_constant_by_name(NULL, "$IN.frame_direction", hlsl->prg[i].f_ctable);
   hlsl->prg[i].vid_size_v  = get_constant_by_name(NULL, "$IN.video_size",      hlsl->prg[i].v_ctable);
   hlsl->prg[i].tex_size_v  = get_constant_by_name(NULL, "$IN.texture_size",    hlsl->prg[i].v_ctable);
   hlsl->prg[i].out_size_v  = get_constant_by_name(NULL, "$IN.output_size",     hlsl->prg[i].v_ctable);
   hlsl->prg[i].frame_cnt_v = get_constant_by_name(NULL, "$IN.frame_count",     hlsl->prg[i].v_ctable);
   hlsl->prg[i].frame_dir_v = get_constant_by_name(NULL, "$IN.frame_direction", hlsl->prg[i].v_ctable);
   hlsl->prg[i].mvp         = get_constant_by_name(NULL, "$modelViewProj",      hlsl->prg[i].v_ctable);
   d3d_matrix_identity(&hlsl->prg[i].mvp_val);
}

static bool hlsl_load_shader(hlsl_shader_data_t *hlsl,
      const char *cgp_path, unsigned i)
{
   struct shader_program_info program_info;
   char path_buf[PATH_MAX_LENGTH];

   path_buf[0]           = '\0';

   program_info.combined = path_buf;
   program_info.is_file  = true;

   fill_pathname_resolve_relative(path_buf, cgp_path,
         hlsl->cg_shader->pass[i].source.path, sizeof(path_buf));

   RARCH_LOG("Loading Cg/HLSL shader: \"%s\".\n", path_buf);

   if (!hlsl_compile_program(hlsl, i + 1, &hlsl->prg[i + 1], &program_info))
      return false;

   return true;
}

static bool hlsl_load_plain(hlsl_shader_data_t *hlsl, const char *path)
{
   if (!hlsl_load_stock(hlsl))
      return false;

   hlsl->cg_shader = (struct video_shader*)calloc(1, sizeof(*hlsl->cg_shader));
   if (!hlsl->cg_shader)
      return false;

   hlsl->cg_shader->passes = 1;

   if (!string_is_empty(path))
   {
      struct shader_program_info program_info;

      program_info.combined = path;
      program_info.is_file  = true;

      RARCH_LOG("Loading Cg/HLSL file: %s\n", path);

      strlcpy(hlsl->cg_shader->pass[0].source.path,
            path, sizeof(hlsl->cg_shader->pass[0].source.path));

      if (!hlsl_compile_program(hlsl, 1, &hlsl->prg[1], &program_info))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg/HLSL file.\n");
      hlsl->prg[1] = hlsl->prg[0];
   }

   return true;
}

static void hlsl_deinit_progs(hlsl_shader_data_t *hlsl)
{
   unsigned i;

   for (i = 1; i < RARCH_HLSL_MAX_SHADERS; i++)
   {
      if (hlsl->prg[i].fprg && hlsl->prg[i].fprg != hlsl->prg[0].fprg)
         d3d_free_pixel_shader(hlsl->dev, hlsl->prg[i].fprg);
      if (hlsl->prg[i].vprg && hlsl->prg[i].vprg != hlsl->prg[0].vprg)
         d3d_free_vertex_shader(hlsl->dev, hlsl->prg[i].vprg);

      hlsl->prg[i].fprg = NULL;
      hlsl->prg[i].vprg = NULL;
   }

   if (hlsl->prg[0].fprg)
      d3d_free_pixel_shader(hlsl->dev, hlsl->prg[0].fprg);
   if (hlsl->prg[0].vprg)
      d3d_free_vertex_shader(hlsl->dev, hlsl->prg[0].vprg);

   hlsl->prg[0].fprg = NULL;
   hlsl->prg[0].vprg = NULL;
}

static bool hlsl_load_preset(hlsl_shader_data_t *hlsl, const char *path)
{
   unsigned i;
   config_file_t *conf = NULL;
   if (!hlsl_load_stock(hlsl))
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);

   conf = config_file_new(path);

   if (!conf)
      goto error;

   if (!hlsl->cg_shader)
      hlsl->cg_shader = (struct video_shader*)calloc(1, sizeof(*hlsl->cg_shader));
   if (!hlsl->cg_shader)
      goto error;

   if (!video_shader_read_conf_cgp(conf, hlsl->cg_shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      goto error;
   }

   config_file_free(conf);

   if (hlsl->cg_shader->passes > RARCH_HLSL_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n", RARCH_HLSL_MAX_SHADERS - 3);
      hlsl->cg_shader->passes = RARCH_HLSL_MAX_SHADERS - 3;
   }

   for (i = 0; i < hlsl->cg_shader->passes; i++)
   {
      if (!hlsl_load_shader(hlsl, path, i))
         goto error;
   }

   /* TODO - textures / imports */
   return true;

error:
   RARCH_ERR("Failed to load preset.\n");
   if (conf)
      config_file_free(conf);
   conf = NULL;

   return false;
}

static void *hlsl_init(void *data, const char *path)
{
   unsigned i;
   d3d_video_t         *d3d = (d3d_video_t*)data;
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)
      calloc(1, sizeof(hlsl_shader_data_t));

   if (!hlsl || !d3d)
      goto error;

   hlsl->dev         = d3d->dev;

   if (!hlsl->dev)
      goto error;

   if (path && (string_is_equal(path_get_extension(path), ".cgp")))
   {
      if (!hlsl_load_preset(hlsl, path))
         goto error;
   }
   else
   {
      if (!hlsl_load_plain(hlsl, path))
         goto error;
   }

   for(i = 1; i <= hlsl->cg_shader->passes; i++)
      hlsl_set_program_attributes(hlsl, i);

   d3d_set_vertex_shader(hlsl->dev, 1, hlsl->prg[1].vprg);
   d3d_set_pixel_shader(hlsl->dev, hlsl->prg[1].fprg);

   return hlsl;

error:
   if (hlsl)
      free(hlsl);
   return NULL;
}

static void hlsl_deinit(void *data)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;

   if (!hlsl)
      return;

   hlsl_deinit_progs(hlsl);
   memset(hlsl->prg, 0, sizeof(hlsl->prg));

   if (hlsl->cg_shader)
      free(hlsl->cg_shader);
   hlsl->cg_shader = NULL;

   if (hlsl)
      free(hlsl);
}

static void hlsl_use(void *data, void *shader_data, unsigned idx, bool set_active)
{
   hlsl_shader_data_t *hlsl      = (hlsl_shader_data_t*)shader_data;
   LPDIRECT3DDEVICE9        d3dr = hlsl ? (LPDIRECT3DDEVICE9)hlsl->dev : NULL;

   if (!hlsl || !hlsl->prg[idx].vprg || !hlsl->prg[idx].fprg)
      return;

   if (set_active)
      hlsl->active_idx = idx;

   d3d_set_vertex_shader(d3dr, idx, hlsl->prg[idx].vprg);
   d3d_set_pixel_shader(d3dr, hlsl->prg[idx].fprg);
}

static unsigned hlsl_num(void *data)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   if (hlsl)
      return hlsl->cg_shader->passes;
   return 0;
}

static bool hlsl_filter_type(void *data, unsigned idx, bool *smooth)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   if (hlsl && idx
         && (hlsl->cg_shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC))
   {
      *smooth = RARCH_FILTER_LINEAR;
      hlsl->cg_shader->pass[idx - 1].filter = RARCH_FILTER_LINEAR;
      return true;
   }
   return false;
}

static void hlsl_shader_scale(void *data, unsigned idx, struct gfx_fbo_scale *scale)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   if (hlsl && idx)
      *scale = hlsl->cg_shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static bool hlsl_set_mvp(void *data, void *shader_data, const void *mat_data)
{
   hlsl_shader_data_t *hlsl        = (hlsl_shader_data_t*)shader_data;
   LPDIRECT3DDEVICE9 d3dr          = hlsl ? (LPDIRECT3DDEVICE9)hlsl->dev : NULL;
   const math_matrix_4x4 *mat      = (const math_matrix_4x4*)mat_data;

   if (!hlsl || !hlsl->prg[hlsl->active_idx].mvp)
      return false;

   ID3DXConstantTable_SetMatrix(hlsl->prg[hlsl->active_idx].v_ctable, d3dr,
         hlsl->prg[hlsl->active_idx].mvp,
         &hlsl->prg[hlsl->active_idx].mvp_val);
   return true;
}

static bool hlsl_mipmap_input(void *data, unsigned idx)
{
   (void)idx;
   return false;
}

static bool hlsl_get_feedback_pass(void *data, unsigned *idx)
{
   (void)idx;
   return false;
}

static struct video_shader *hlsl_get_current_shader(void *data)
{
   return NULL;
}

const shader_backend_t hlsl_backend = {
   hlsl_init,
   hlsl_deinit,
   hlsl_set_params,
   hlsl_set_uniform_parameter,
   NULL,               /* compile_program */
   hlsl_use,
   hlsl_num,
   hlsl_filter_type,
   NULL,              /* hlsl_wrap_type  */
   hlsl_shader_scale,
   NULL,              /* hlsl_set_coords */
   hlsl_set_mvp,
   NULL,              /* hlsl_get_prev_textures */
   hlsl_get_feedback_pass,
   hlsl_mipmap_input,
   hlsl_get_current_shader,

   RARCH_SHADER_HLSL,
   "hlsl"
};
