/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_gpencil.h
 *  \ingroup editors
 */

#ifndef __ED_GPENCIL_H__
#define __ED_GPENCIL_H__

struct ID;
struct ListBase;
struct bContext;
struct Depsgraph;
struct ScrArea;
struct ARegion;
struct bAnimContext;
struct wmOperator;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct bGPDpalette;
struct bGPDpalettecolor;
struct bGPDspoint;
struct tGPDdraw;
struct Image;
struct BLI_Stack;
struct KeyframeEditData;
struct Object;
struct Palette;
struct PaletteColor;
struct PointerRNA;
struct RegionView3D;
struct ToolSettings;
struct View3D;
struct Scene;
struct ViewLayer;
struct wmWindowManager;
struct wmKeyConfig;
struct wmWindowManager;
struct EvaluationContext;
struct wmWindow;
struct rcti;

struct tGPDdraw;
struct tGPDinterpolate;
struct tGPDprimitive;
struct tGPDfill;
struct tGPDpick;

/* ------------- Grease-Pencil Runtime Data ---------------- */

/* Temporary 'Stroke Point' data (2D / screen-space)
 *
 * Used as part of the 'stroke cache' used during drawing of new strokes
 */
typedef struct tGPspoint {
	int x, y;               /* x and y coordinates of cursor (in relative to area) */
	float pressure;         /* pressure of tablet at this point */
	float strength;         /* pressure of tablet at this point for alpha factor */
	float time;             /* Time relative to stroke start (used when converting to path) */
} tGPspoint;

/* used to sort by zdepth gpencil objects in viewport */
/* TODO: this could be a system parameter in userprefs screen */
#define GP_CACHE_BLOCK_SIZE 16
typedef struct tGPencilSort {
	struct Base *base;
	float zdepth;
} tGPencilSort;

/* ----------- Grease Pencil Tools/Context ------------- */

/* Context-dependent */
struct bGPdata **ED_gpencil_data_get_pointers(const struct bContext *C, struct PointerRNA *ptr);
struct bGPdata  *ED_gpencil_data_get_active(const struct bContext *C);

/* Context independent (i.e. each required part is passed in instead) */
struct bGPdata **ED_gpencil_data_get_pointers_direct(struct ID *screen_id, struct Scene *scene,
                                                     struct ScrArea *sa, struct Object *ob,
                                                     struct PointerRNA *ptr);
struct bGPdata *ED_gpencil_data_get_active_direct(struct ID *screen_id, struct Scene *scene,
                                                  struct ScrArea *sa, struct Object *ob);

/* 3D View */
struct bGPdata  *ED_gpencil_data_get_active_v3d(struct Scene *scene, struct ViewLayer *view_layer);

bool ED_gpencil_has_keyframe_v3d(struct Scene *scene, struct Object *ob, int cfra);

/* ----------- Stroke Editing Utilities ---------------- */

bool ED_gpencil_stroke_can_use_direct(const struct ScrArea *sa, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_can_use(const struct bContext *C, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_color_use(const struct bGPDlayer *gpl, const struct bGPDstroke *gps);

bool ED_gpencil_stroke_minmax(
        const struct bGPDstroke *gps, const bool use_select,
        float r_min[3], float r_max[3]);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);

void ED_operatortypes_gpencil(void);
void ED_operatormacros_gpencil(void);

/* ------------- Copy-Paste Buffers -------------------- */

/* Strokes copybuf */
void ED_gpencil_strokes_copybuf_free(void);


/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void ED_gpencil_draw_2dimage(const struct bContext *C);
void ED_gpencil_draw_view2d(const struct bContext *C, bool onlyv2d);
void ED_gpencil_draw_view3d(struct wmWindowManager *wm,
                            struct Scene *scene,
                            struct ViewLayer *view_layer,
                            const struct Depsgraph *depsgraph,
                            struct View3D *v3d,
                            struct ARegion *ar,
                            bool only3d);
void ED_gpencil_draw_view3d_object(struct wmWindowManager *wm,
                                   struct Scene *scene,
								   const struct Depsgraph *depsgraph,
								   struct Object *ob,
                                   struct View3D *v3d,
                                   struct ARegion *ar,
                                   bool only3d);
void ED_gpencil_draw_ex(struct RegionView3D *rv3d, struct Scene *scene, struct bGPdata *gpd, int winx, int winy,
                        const int cfra, const char spacetype);
void ED_gp_draw_interpolation(const struct bContext *C, struct tGPDinterpolate *tgpi, const int type);
void ED_gp_draw_primitives(const struct bContext *C, struct tGPDprimitive *tgpi, const int type);
void ED_gp_draw_fill(struct tGPDdraw *tgpw);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
bool  ED_gplayer_frames_looper(struct bGPDlayer *gpl, struct Scene *scene,
                               short (*gpf_cb)(struct bGPDframe *, struct Scene *));
void ED_gplayer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, bool onlysel);

bool  ED_gplayer_frame_select_check(struct bGPDlayer *gpl);
void  ED_gplayer_frame_select_set(struct bGPDlayer *gpl, short mode);
void  ED_gplayer_frames_select_border(struct bGPDlayer *gpl, float min, float max, short select_mode);
void  ED_gplayer_frames_select_region(struct KeyframeEditData *ked, struct bGPDlayer *gpl, short tool, short select_mode);
void  ED_gpencil_select_frames(struct bGPDlayer *gpl, short select_mode);
void  ED_gpencil_select_frame(struct bGPDlayer *gpl, int selx, short select_mode);

bool  ED_gplayer_frames_delete(struct bGPDlayer *gpl);
void  ED_gplayer_frames_duplicate(struct bGPDlayer *gpl);

void ED_gplayer_frames_keytype_set(struct bGPDlayer *gpl, short type);

void  ED_gplayer_snap_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);
void  ED_gplayer_mirror_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

void ED_gpencil_anim_copybuf_free(void);
bool ED_gpencil_anim_copybuf_copy(struct bAnimContext *ac);
bool ED_gpencil_anim_copybuf_paste(struct bAnimContext *ac, const short copy_mode);


/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
int ED_undo_gpencil_step(struct bContext *C, int step, const char *name);

/* ------------ Transformation Utilities ------------ */

/* get difference matrix */
void ED_gpencil_parent_location(struct Object *obact, struct bGPdata *gpd, struct bGPDlayer *gpl, float diff_mat[4][4]);
/* reset parent matrix for all layers */
void ED_gpencil_reset_layers_parent(struct Object *obact, struct bGPdata *gpd);

/* ----------- Add Primitive Utilities -------------- */

void ED_gpencil_create_monkey(struct bContext *C, struct bGPdata *gpd);

/* ------------ Object Utilities ------------ */
struct Object *ED_add_gpencil_object(struct bContext *C, struct Scene *scene, const float loc[3]);
void ED_gpencil_add_defaults(struct bContext *C);

struct tGPencilSort *ED_gpencil_allocate_cache(struct tGPencilSort *cache, int *gp_cache_size, int gp_cache_used);
void ED_gpencil_add_to_cache(struct tGPencilSort *cache, struct RegionView3D *rv3d, struct Base *base, int *gp_cache_used);

void ED_gp_project_stroke_to_plane(struct Object *ob, struct RegionView3D *rv3d, struct bGPDstroke *gps, const float origin[3], const int axis, char type);
void ED_gp_project_point_to_plane(struct Object *ob, struct RegionView3D *rv3d, const float origin[3], const int axis, char type, struct bGPDspoint *pt);
void ED_gp_get_drawing_reference(struct View3D *v3d, struct Scene *scene, struct Object *ob, struct bGPDlayer *gpl, char align_flag, float vec[3]);

/* set sculpt cursor */
void ED_gpencil_toggle_brush_cursor(struct bContext *C, bool enable, void *customdata);

/* vertex groups */
void ED_gpencil_vgroup_assign(struct bContext *C, struct Object *ob, float weight);
void ED_gpencil_vgroup_remove(struct bContext *C, struct Object *ob);
void ED_gpencil_vgroup_select(struct bContext *C, struct Object *ob);
void ED_gpencil_vgroup_deselect(struct bContext *C, struct Object *ob);

/* join objects */
int ED_gpencil_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* helper to get brush icon */
int gpencil_get_brush_icon(int type);

#endif /*  __ED_GPENCIL_H__ */
